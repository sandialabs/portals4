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

	ptl_info("rep sent %d / %zd bytes \n", ret, sizeof(struct rpc_msg));

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

static int init_session(struct rpc *rpc, int s, struct session **session_p,
						void (*cb)(EV_P_ ev_io *w, int revents))
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

	session->fd = s;
	session->rpc = rpc;
	ev_io_init(&session->watcher, cb, s, EV_READ);
	session->watcher.data = session;

	pthread_spin_lock(&rpc->session_list_lock);
	list_add(&session->session_list, &rpc->session_list);
	pthread_spin_unlock(&rpc->session_list_lock);

	/* This is running inside the event loop, so locking is already
	 * done. */
	ev_io_start(evl.loop, &session->watcher);

	*session_p = session;

	return PTL_OK;

err1:
	return err;
}

static void fini_session(struct rpc *rpc, struct session *session)
{
	/* Called from inside the event loop, so don't lock. */
	ev_io_stop(evl.loop, &session->watcher);

	close(session->fd);
	session->fd = -1;

	pthread_spin_lock(&rpc->session_list_lock);
	list_del(&session->session_list);
	pthread_spin_unlock(&rpc->session_list_lock);

	pthread_cond_destroy(&session->cond);
	pthread_mutex_destroy(&session->mutex);

	free(session);

	/* Tell the owner that the session list is now empty. Only usefull
	 * for the control process. */
	if (list_empty(&rpc->session_list)) {
		pthread_spin_lock(&rpc->session_list_lock);
		session_list_is_empty();
		pthread_spin_unlock(&rpc->session_list_lock);
	}
}

static void read_one(EV_P_ ev_io *w, int revents)
{
	ssize_t ret;
	int err;
	struct session *session = w->data;

	ret = recv(session->fd, &session->rpc_msg, sizeof(struct rpc_msg), MSG_WAITALL);
	if (ret < 0) {
		err = -errno;
		if (verbose) {
			perror("unable to recv");
		}
		goto done;
	}

	if (ret == 0) {
		if (verbose)
			printf("session closed, pid=%d\n", getpid());
		err = 1;
	} else {
		if (session->rpc_msg.type & 0x80) {
			/* A reply. */
			pthread_mutex_lock(&session->mutex);
			pthread_cond_signal(&session->cond);
			pthread_mutex_unlock(&session->mutex);
		} else {
			/* A request. */
			session->rpc->callback(session, session->rpc->callback_data);
		}

		err = 0;
	}

done:
	if (err) {
		fini_session(session->rpc, session);
	}
}

static void accept_one(EV_P_ ev_io *w, int revents)
{
	int err;
	int new_s;
	char buf[80];
	struct sockaddr_in remote_addr;
	socklen_t addr_len;
	struct session *session;
	struct rpc *rpc = w->data;

	addr_len = sizeof(remote_addr);

	new_s = accept(rpc->fd, (struct sockaddr *)&remote_addr, &addr_len);
	if (new_s < 0) {
		err = errno;
		if (verbose) {
			strerror_r(errno, buf, sizeof(buf));
			printf("unable to accept on socket: %s\n", buf);
		}
		return;
	}

	err = init_session(rpc, new_s, &session, read_one);
	if (err) {
		close(new_s);
		return;
	}

	if (verbose)
		printf("received new connection on sd = %d\n", new_s);
}

int rpc_init(enum rpc_type type, ptl_nid_t nid, unsigned int ctl_port,
			 struct rpc **rpc_p,
			 void (*callback)(struct session *session, void *data_callback),
			 void *callback_data)
{
	int s;
	int i;
	int err;
	char buf[80];
	struct rpc *rpc;
	struct session *session = NULL;
	struct sockaddr_in addr;
	socklen_t addr_len;
	int backlog = 32;

#if TEMP_PHYSICAL_NI
	return PTL_OK;
#endif

	rpc = calloc(1, sizeof(*rpc));
	if (unlikely(!rpc)) {
		ptl_fatal("unable to allocate memory for rpc\n");
		err = PTL_NO_SPACE;
		goto err1;
	}

	rpc->callback = callback;
	rpc->callback_data = callback_data;

	INIT_LIST_HEAD(&rpc->session_list);
	pthread_spin_init(&rpc->session_list_lock, 0);
	rpc->type = type;
	rpc->fd = -1;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		err = errno;
		strerror_r(errno, buf, sizeof(buf));
		ptl_fatal("unable to open socket: %s\n", buf);
		goto err2;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(ctl_port);
	addr_len = sizeof(addr);

	if (type == rpc_type_server) {
		int on = 1;

		addr.sin_addr.s_addr = htonl(INADDR_ANY);

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
		ev_io_init(&rpc->watcher, accept_one, s, EV_READ);
		rpc->watcher.data = rpc;

		EVL_WATCH(ev_io_start(evl.loop, &rpc->watcher));

	} else {

		addr.sin_addr.s_addr = nid_to_addr(nid);

		/* Retry connect()'ing for 5 seconds because the daemon might
		 * be starting. */
		for (i=0; i<50; i++) {
			err = connect(s, (struct sockaddr *)&addr, addr_len);
			if (err == -1 && errno == ECONNREFUSED) {
				/* The control daemon has not started yet. */
				usleep(100000);	/* 1/10 second */
			} else {
				/* Some other error. Don't retry. */
				break;
			}
		}

		if (err < 0) {
			err = errno;
			strerror_r(errno, buf, sizeof(buf));
			ptl_error("unable to connect to server: %s\n", buf);
			goto err3;
		}

		err = init_session(rpc, s, &session, read_one);
		if (err) {
			ptl_warn("init_session failed, err = %d\n", err);
			goto err3;
		}

		rpc->to_server = session;
	}

	*rpc_p = rpc;

	ptl_info("ok\n");
	return PTL_OK;

err3:
	close(s);
err2:
	free(rpc);
err1:
	return (err == ENOMEM) ? PTL_NO_SPACE : PTL_FAIL;
}

int rpc_fini(struct rpc *rpc)
{
	struct list_head *l, *t;
	struct session *session;

#if TEMP_PHYSICAL_NI
	return PTL_OK;
#endif

	if (rpc->fd != -1) {
		EVL_WATCH(ev_io_stop(evl.loop, &rpc->watcher));
		close(rpc->fd);
		rpc->fd = -1;
	}

	list_for_each_safe(l, t, &rpc->session_list) {
		session = list_entry(l, struct session, session_list);
		fini_session(rpc, session);
	}

	pthread_spin_destroy(&rpc->session_list_lock);

	free(rpc);

	return PTL_OK;
}
