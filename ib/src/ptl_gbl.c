/*
 * ptl_gbl.c
 */

#include "ptl_loc.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>

/* Default control port. This could be overridden by an environment
 * variable. */
unsigned int ctl_port = PTL_CTL_PORT;

/*
 * per process global state
 * acquire proc_gbl_mutex before making changes
 * that require atomicity
 */
static gbl_t per_proc_gbl;
static pthread_mutex_t per_proc_gbl_mutex = PTHREAD_MUTEX_INITIALIZER;

void session_list_is_empty(void)
{
	/* We don't care. */
	return;
}

static void stop_event_loop_func(EV_P_ ev_async *w, int revents)
{
	ev_break(my_event_loop, EVBREAK_ALL);
}

static void gbl_release(ref_t *ref)
{
	gbl_t *gbl = container_of(ref, gbl_t, ref);

	/* fini the object allocator */
	obj_fini();

	/* Terminate the event loop, which will terminate the event
	 * thread. */
	if (gbl->event_thread_run) {
		/* Create an async event to stop the event loop. May be there is a
		 * better way. */
		ev_async stop_event_loop;

		ev_async_init(&stop_event_loop, stop_event_loop_func);
		ev_async_start(my_event_loop, &stop_event_loop);
		ev_async_send(my_event_loop, &stop_event_loop);
		pthread_join(gbl->event_thread, NULL);
	}

	pthread_mutex_destroy(&gbl->gbl_mutex);

	rpc_fini(gbl->rpc);
}

/* Get an int value from an environment variable. */
static long getenv_val(const char *name, unsigned int *val)
{
	char *endptr;
	unsigned long myval;
	char *str;

	str = getenv(name);
	if (!str)
		return PTL_FAIL;
	errno = 0;
	myval = strtoul(str, &endptr, 10);

	if ((errno == ERANGE && myval == ULONG_MAX) ||
		(errno != 0 && myval == 0))
		return PTL_FAIL;

	/* The value is valid, but it also has to fit in an unsigned int. */
	if (myval > UINT_MAX)
		return PTL_FAIL;

	*val = myval;

	return PTL_OK;
}

/* Given a string such as "1107361792.0;tcp://10.0.2.91:53189",
 * extract the IP adress and return it as a 32 bits number. 
 * 0 means error.
 */
static uint32_t get_node_id_from_tcp(const char *str)
{
	char *p, *q;
	const char *to_find = "tcp://";
	char ipaddr[20];
	struct in_addr inp;

	p = strstr(str, to_find);
	if (!p)
		return 0;

	/* Copy the ip address up to : */
	p += strlen(to_find);
	q = ipaddr;
	while(*p && *p != ':') {
		*q = *p;
		p++;
		q++;
	}

	*q = 0;

	if (!inet_aton(ipaddr, &inp))
		return 0;

	return ntohl(inp.s_addr);
}


/* The job launcher (mpirun) gives some parameters through environment
 * variables:
 *
 * - OMPI_MCA_orte_ess_jobid : job ID
 * - OMPI_COMM_WORLD_RANK : Node ID   (TODO: not correct)
 */
static int get_vars(gbl_t *gbl)
{
	ptl_jid_t jid;
	char *env;

	if (getenv_val("OMPI_MCA_orte_ess_jobid", &jid) ||
		getenv_val("OMPI_UNIVERSE_SIZE", &gbl->num_nids) ||
		getenv_val("OMPI_COMM_WORLD_RANK", &gbl->rank) ||
		getenv_val("OMPI_COMM_WORLD_SIZE", &gbl->nranks) ||
		getenv_val("OMPI_COMM_WORLD_LOCAL_RANK", &gbl->local_rank) ||
		getenv_val("OMPI_COMM_WORLD_LOCAL_SIZE", &gbl->local_nranks)) {
		ptl_warn("some variables are not set or invalid\n");
		return PTL_FAIL;
	}

	gbl->jid = jid;

	/* Extract the node ID. It is the local IP address put back into a 32 bits number. */
	env = getenv("OMPI_MCA_orte_local_daemon_uri");
	if (!env) {
		ptl_warn("OMPI_MCA_orte_local_daemon_uri not found\n");
		return PTL_FAIL;
	}
	gbl->nid = get_node_id_from_tcp(env);
	if (!gbl->nid) {
		ptl_warn("Can't extract node id\n");
		return PTL_FAIL;
	}

	/* Extract the Node ID for the main control process. */
	env = getenv("PORTALS4_MASTER_NID");
	if (env) {
		struct in_addr inp;

		if (!inet_aton(env, &inp)) {
			ptl_warn("Invalid PORTALS4_MASTER_NID\n");
			return PTL_FAIL;
		}

		gbl->main_ctl_nid = ntohl(inp.s_addr);

	} else {
		env = getenv("OMPI_MCA_orte_hnp_uri");
		if (!env) {
			ptl_warn("OMPI_MCA_orte_hnp_uri not found\n");
			return PTL_FAIL;
		}
		gbl->main_ctl_nid = get_node_id_from_tcp(env);
	}
	if (!gbl->main_ctl_nid) {
		ptl_warn("Can't extract node id\n");
		return PTL_FAIL;
	}

	return PTL_OK;
}

/* Start the control daemon. */
static int start_control_daemon(gbl_t *gbl)
{
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		char jobid[10];
		char local_nranks[10];
		char nranks[10];
		char nid[10];
		char master_nid[10];
		char num_nids[10];
		int i;

		/* Close all the file descriptors, except the standard ones. */
		for (i=getdtablesize()-1; i>=3; i--) {
			close(i);
		}

		sprintf(jobid, "%d", gbl->jid);
		sprintf(local_nranks, "%u", gbl->local_nranks);
		sprintf(nranks, "%u", gbl->nranks);
		sprintf(nid, "%u", gbl->nid);
		sprintf(master_nid, "%u", gbl->main_ctl_nid);
		sprintf(num_nids, "%u", gbl->num_nids);

		/* Change the session. */
		if (setsid() == -1) {
			perror("Couldn't change session id\n");
			return 1;
		}

		execl("../src/control",
			  "../src/control",
			  "-n", nid, 
			  "-m", master_nid,
			  "-j", jobid,
			  "-t", local_nranks,
			  "-s", nranks,
			  "-u", num_nids,
#if 0
			  //todo: pass existing params instead
			  "-v",
			  "-l", "100",
#endif
			  NULL);

		_exit(0);
	}

	if (pid > 0) {
		/* The daemon will fork again, so the process we just exec'ed
		 * will die pretty quickly in any case. */
		int status;
		waitpid(pid, &status, 0);
	}

	return 0;
}

static void rpc_callback(struct session *session, void *data)
{
	ptl_warn("Got RPC message %d - dropping it\n", session->rpc_msg.type);
}

static void *event_loop_func(void *arg)
{
	ev_run(my_event_loop, 0);

	return NULL;
}

static int gbl_init(gbl_t *gbl)
{
	int err;

	err = get_vars(gbl);
	if (unlikely(err)) {
		ptl_warn("get_vars failed\n");
		goto err1;
	}

	start_control_daemon(gbl);

	my_event_loop = EV_DEFAULT;

	err = rpc_init(rpc_type_client, gbl->nid, ctl_port, &gbl->rpc, rpc_callback, NULL);
	if (unlikely(err)) {
		ptl_warn("rpc_init failed\n");
		goto err1;
	}

	pthread_mutex_init(&gbl->gbl_mutex, NULL);

	err = get_maps();
	if (unlikely(err)) {
		ptl_warn("get_maps failed\n");
		goto err2;
	}

#if 0
	err = get_init_data(gbl);
	if (err) {
		ptl_warn("get_init_data failed\n");
		goto err2;
	}
#endif

	/* init the object allocator */
	err = obj_init();
	if (unlikely(err)) {
		ptl_warn("obj_init failed\n");
		goto err2;
	}

	/* Create the event loop thread. */
	err = pthread_create(&gbl->event_thread, NULL, event_loop_func, gbl);
	if (unlikely(err)) {
		ptl_warn("event loop creation failed\n");
		goto err2;
	}
	gbl->event_thread_run = 1;

	return PTL_OK;

 err2:
	pthread_mutex_destroy(&gbl->gbl_mutex);
	rpc_fini(gbl->rpc);
 err1:
	return err;
}

int get_gbl(gbl_t **gbl_p)
{
	gbl_t *gbl = &per_proc_gbl;

	if (gbl->ref_cnt == 0)
		return PTL_NO_INIT;

	ref_get(&gbl->ref);

	*gbl_p = gbl;
	return PTL_OK;
}

void gbl_put(gbl_t *gbl)
{
	ref_put(&gbl->ref, gbl_release);
}

/* caller must hold global mutex */
ni_t *gbl_lookup_ni(gbl_t *gbl, ptl_interface_t iface, int ni_type)
{
	if (iface > MAX_IFACE)
		return NULL;

	return gbl->iface[iface].ni;
}

/* caller must hold global mutex */
int gbl_add_ni(gbl_t *gbl, ni_t *ni)
{
	ptl_interface_t iface = ni->iface;

	/* Ensure there's no NI there already. */
	if (iface >= MAX_IFACE || gbl->iface[iface].ni)
		return PTL_ARG_INVALID;

	gbl->iface[iface].ni = ni;
	sprintf(gbl->iface[iface].if_name, "ib%d", iface);

	ni->iface = iface;

	return PTL_OK;
}

/* caller must hold global mutex */
int gbl_remove_ni(gbl_t *gbl, ni_t *ni)
{
	ptl_interface_t iface = ni->iface;

	if (unlikely(ni != gbl->iface[iface].ni))
		return PTL_FAIL;

	gbl->iface[iface].if_name[0] = 0;
	gbl->iface[iface].ni = NULL;

	return PTL_OK;
}

int PtlInit(void)
{
	int ret;
	gbl_t *gbl = &per_proc_gbl;

	ret = pthread_mutex_lock(&per_proc_gbl_mutex);
	if (ret) {
		ptl_warn("unable to acquire proc_gbl mutex\n");
		ret = PTL_FAIL;
		goto err0;
	}

	/* if first call to PtlInit do real initialization */
	if (gbl->ref_cnt == 0) {
		/* check for dangling reference */
		if (gbl->ref.ref_cnt > 0)
			usleep(10000);
		if (gbl->ref.ref_cnt > 0) {
			ptl_warn("still cleaning up, ref.ref_cnt = %d\n",
				gbl->ref.ref_cnt);
			ret = PTL_FAIL;
			goto err1;
		} else {
			ref_init(&gbl->ref);
			//printf("gbl set ref = %d\n", gbl->ref.ref_cnt);
		}

		ret = gbl_init(gbl);
		if (ret != PTL_OK) {
			goto err1;
		}
	}

	gbl->ref_cnt++;
	pthread_mutex_unlock(&per_proc_gbl_mutex);

	return PTL_OK;

err1:
	pthread_mutex_unlock(&per_proc_gbl_mutex);
err0:
	return ret;
}

void PtlFini(void)
{
	int ret;
	gbl_t *gbl = &per_proc_gbl;

	ptl_test_return = PTL_OK;

	ret = pthread_mutex_lock(&per_proc_gbl_mutex);
	if (ret) {
		ptl_warn("unable to acquire proc_gbl mutex\n");
		ptl_test_return = PTL_FAIL;
		abort();
		goto err0;
	}

	/* this would be a bug */
	if (gbl->ref_cnt == 0) {
		ptl_warn("ref_cnt already 0 ?!!\n");
		ptl_test_return = PTL_FAIL;
		goto err1;
	}

	/* note the order is significant here
	   gbl->ref_cnt != 0 implies that the
	   spinlock in gbl->ref has been init'ed
	   so ref_init must come before the initial
	   ref_cnt++ and ref_put must come after
	   the last ref_cnt-- */
	gbl->ref_cnt--;

	if (gbl->ref_cnt == 0)
		ref_put(&gbl->ref, gbl_release);	/* matches ref_init */

	pthread_mutex_unlock(&per_proc_gbl_mutex);

	return;

err1:
	pthread_mutex_unlock(&per_proc_gbl_mutex);
err0:
	return;
}
