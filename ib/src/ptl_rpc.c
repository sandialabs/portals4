/*
 * ptl_rpc.c
 */

/* rpc layer for communications between a portals
 * instance and a portals control process
 * try to make symmetric so that client and server
 * can use same code
 */

#include "ctl.h"

static int verbose = 0;

int rpc_get(struct session *session,
	    struct rpc_msg *msg_in, struct rpc_msg *msg_out)
{
	int ret;

	pthread_mutex_lock(&session->mutex);

	msg_in->sequence = session->sequence++;

	ret = send(session->fd, msg_in, sizeof(struct rpc_msg), 0);

	pthread_cond_wait(&session->cond, &session->mutex);

	memcpy(msg_out, &session->rpc_msg, sizeof(struct rpc_msg));

	pthread_mutex_unlock(&session->mutex);

	return PTL_OK;
}

int rpc_send(struct session *session, struct rpc_msg *msg_out)
{
	int ret;

	ret = send(session->fd, msg_out, sizeof(struct rpc_msg), 0);

	ptl_info("rep sent %d bytes \n", ret);

	return PTL_OK;
}

#if 0
int rpc_get_pid(gbl_t *gbl)
{
	int err;
	char req[80];
	char rep[80];
	struct session *session;
	struct list_head *l;
	struct rpc *rpc;

	sprintf(req, "pid");

	rpc = gbl->rpc;

	list_for_each(l, &rpc->session_list) {
		session = list_entry(l, struct session, session_list);

		err = rpc_get(session, req, sizeof(req), rep, sizeof(rep));

		printf("err = %d, req = %s, rep = %s\n", err, req, rep);
	}

	return PTL_OK;
}
#endif

static int init_session(struct rpc *rpc, int s, struct session **session_p)
{
	int err;
	struct session *session;

	session = calloc(1, sizeof(*session));
	if (unlikely(!session)) {
		err = PTL_NO_SPACE;
		if (verbose)
			ptl_fatal("unable to allocate memory for session\n");
		goto err1;
	}

	session->sequence = 1;

	pthread_mutex_init(&session->mutex, NULL);
	pthread_cond_init(&session->cond, NULL);

	pthread_spin_lock(&rpc->session_list_lock);
	list_add(&session->session_list, &rpc->session_list);
	pthread_spin_unlock(&rpc->session_list_lock);

	session->fd = s;

	*session_p = session;

	return PTL_OK;

err1:
	return err;
}

static void fini_session(struct rpc *rpc, struct session *session)
{
	close(session->fd);

	pthread_spin_lock(&rpc->session_list_lock);
	list_del(&session->session_list);
	pthread_spin_unlock(&rpc->session_list_lock);

	pthread_cond_destroy(&session->cond);
	pthread_mutex_destroy(&session->mutex);

	free(session);
}

static int read_one(struct rpc *rpc, struct session *session)
{
	ssize_t ret;
	int err;
	char buf[80];

	ret = recv(session->fd, &session->rpc_msg, sizeof(struct rpc_msg), 0);
	if (ret < 0) {
		err = errno;
		if (verbose) {
			strerror_r(errno, buf, sizeof(buf));
			printf("unable to recv: %s\n", buf);
		}
		goto err1;
	}

	if (ret == 0) {
		printf("session closed\n");
		fini_session(rpc, session);
		goto done;
	} else {
		if (session->rpc_msg.type & 0x80) {
			/* A reply. */
			pthread_mutex_lock(&session->mutex);
			pthread_cond_signal(&session->cond);
			pthread_mutex_unlock(&session->mutex);
		} else {
			/* A request. */
			rpc->callback(session);
		}
	}
done:
	return 0;

err1:
	return err;
}

static int accept_one(struct rpc *rpc)
{
	int err;
	int new_s;
	char buf[80];
	struct sockaddr_in remote_addr;
	socklen_t addr_len;
	struct session *session;

	addr_len = sizeof(remote_addr);

	new_s = accept(rpc->fd, (struct sockaddr *)&remote_addr, &addr_len);
	if (new_s < 0) {
		err = errno;
		if (verbose) {
			strerror_r(errno, buf, sizeof(buf));
			printf("unable to accept on socket: %s\n", buf);
		}
		goto err1;
	}

	err = init_session(rpc, new_s, &session);
	if (err) 
		goto err1;

	if (verbose)
		printf("received new connection on sd = %d\n", new_s);

	return 0;

err1:
	return err;
}

static void *io_task(void *arg)
{
	struct rpc *rpc = arg;
	int err;
	char buf[80];
	int maxfd;
	int nfd;
	int i;
	fd_set readfds;
	struct timeval timeout;
	struct list_head *l, *t;
	struct session *session;

	rpc->io_thread_run = 1;

	if (verbose > 1)
		printf("io_thread started\n");

	while (rpc->io_thread_run) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;

		FD_ZERO(&readfds);

		if (rpc->type == rpc_type_server) {
			FD_SET(rpc->fd, &readfds);
			maxfd = rpc->fd;
		} else {
			maxfd = -1;
		}

		list_for_each(l, &rpc->session_list) {
			session = list_entry(l, struct session, session_list);

			FD_SET(session->fd, &readfds);
			if (session->fd > maxfd)
				maxfd = session->fd;
		}

		nfd = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
		if (nfd < 0) {
			err = errno;
			if (verbose) {
				strerror_r(errno, buf, sizeof(buf));
				printf("select failed: %s\n", buf);
			}
			goto err1;
		}

		while (nfd) {
			if ((rpc->type == rpc_type_server)
			    && FD_ISSET(rpc->fd, &readfds)) {
				err = accept_one(rpc);
				if (err)
					goto err1;
				FD_CLR(rpc->fd, &readfds);
				nfd--;
				goto next;
			}
			list_for_each_safe(l, t, &rpc->session_list) {
				session = list_entry(l, struct session, session_list);
				if (FD_ISSET(session->fd, &readfds)) {
					err = read_one(rpc, session);
					if (err)
						goto err1;
					FD_CLR(session->fd, &readfds);
					nfd--;
					goto next;
				}
			}
			for (i = 0; i <= maxfd; i++) {
				if (FD_ISSET(i, &readfds)) {
					printf("unexpected fd %d readable\n", i);
					FD_CLR(i, &readfds);
					nfd--;
					goto next;
				}
			}
next:
			;
		}
	}

	if (verbose > 1)
		printf("io_thread stopped\n");

	return NULL;

err1:
	return NULL;
}

int rpc_init(enum rpc_type type, unsigned int ctl_port, struct rpc **rpc_p,
			 void (*callback)(struct session *session))
{
	int s;
	int err;
	char buf[80];
	struct rpc *rpc;
	struct session *session = NULL;
	struct sockaddr_in addr;
	socklen_t addr_len;
	int backlog = 5;

	rpc = calloc(1, sizeof(*rpc));
	if (unlikely(!rpc)) {
		ptl_fatal("unable to allocate memory for rpc\n");
		err = PTL_NO_SPACE;
		goto err1;
	}

	rpc->callback = callback;

	INIT_LIST_HEAD(&rpc->session_list);
	pthread_spin_init(&rpc->session_list_lock, 0);
	rpc->type = type;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		err = errno;
		strerror_r(errno, buf, sizeof(buf));
		ptl_fatal("unable to open socket: %s\n", buf);
		goto err2;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(ctl_port);
	addr_len = sizeof(addr);

	if (type == rpc_type_server) {
		int on = 1;
		err = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (err < 0) {
			strerror_r(errno, buf, sizeof(buf));
			ptl_fatal("unable to set SO_REUSEADDR on socket: %s\n", buf);
		}

		err = bind(s, (struct sockaddr *)&addr, addr_len);
		if (err < 0) {
			err = errno;
			strerror_r(errno, buf, sizeof(buf));
			ptl_fatal("unable to bind socket: %s\n", buf);
			goto err3;
		}

		err = listen(s, backlog);
		if (err < 0) {
			err = errno;
			strerror_r(errno, buf, sizeof(buf));
			ptl_fatal("unable to listen on socket: %s\n", buf);
			goto err3;
		}

		rpc->fd = s;

	} else {
		err = connect(s, (struct sockaddr *)&addr, addr_len);
		if (err < 0) {
			err = errno;
			strerror_r(errno, buf, sizeof(buf));
			ptl_fatal("unable to connect to server: %s\n", buf);
			goto err3;
		}

		err = init_session(rpc, s, &session);
		if (err) {
			ptl_warn("init_session failed, err = %d\n", err);
			goto err3;
		}

		rpc->to_server = session;
	}

	rpc->io_thread_run = 0;

	err = pthread_create(&rpc->io_thread, NULL, io_task, rpc);
	if (err) {
		err = errno;
		strerror_r(errno, buf, sizeof(buf));
		ptl_warn("unable to create io thread: %s\n", buf);
		goto err4;
	}

	*rpc_p = rpc;

	ptl_info("ok\n");
	return PTL_OK;

err4:
	if (session)
		fini_session(rpc, session);
err3:
	close(s);
err2:
	free(rpc);
err1:
	return (err == ENOMEM) ? PTL_NO_SPACE : PTL_FAIL;
}

int rpc_fini(struct rpc *rpc)
{
	int err;
	struct list_head *l, *t;
	struct session *session;

	rpc->io_thread_run = 0;

	err = pthread_join(rpc->io_thread, NULL);
	if (err) {
		printf("pthread_join failed\n");
	}

	close(rpc->fd);

	list_for_each_safe(l, t, &rpc->session_list) {
		session = list_entry(l, struct session, session_list);
		fini_session(rpc, session);
	}

	pthread_spin_destroy(&rpc->session_list_lock);

	free(rpc);

	return PTL_OK;
}
