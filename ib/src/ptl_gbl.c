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

/* Event loop. */
struct evl evl;

void session_list_is_empty(void)
{
	/* We don't care. */
	return;
}

static void stop_event_loop_func(EV_P_ ev_async *w, int revents)
{
	ev_break(evl.loop, EVBREAK_ALL);
}

static void gbl_release(ref_t *ref)
{
	gbl_t *gbl = container_of(ref, gbl_t, ref);

	/* fini the object allocator */
	obj_fini();

	/* Terminate the event loop, which will terminate the event
	 * thread. */
	if (gbl->event_thread_run) {
		/* Create an async event to stop the event loop. May be there
		 * is a better way. */
		ev_async stop_event_loop;
		ev_async_init(&stop_event_loop, stop_event_loop_func);
		EVL_WATCH(ev_async_start(evl.loop, &stop_event_loop));
		ev_async_send(evl.loop, &stop_event_loop);

		pthread_join(gbl->event_thread, NULL);
		EVL_WATCH(ev_async_stop(evl.loop, &stop_event_loop));
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
static ptl_nid_t get_node_id_from_tcp(const char *str)
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

	return addr_to_nid(inp.s_addr);
}

static int get_loopback_vars(gbl_t *gbl)
{
	struct in_addr inp;

	gbl->num_nids = 1;
	gbl->rank = 0;
	ptl_test_rank = 0;
	gbl->nranks = 1;
	gbl->local_rank = 0;
	gbl->local_nranks = 1;
	//gbl->jid = 0x12345678;

	// TODO look this up somewhere
	inet_aton("192.168.221.11", &inp);
	//gbl->nid = ntohl(inp.s_addr);
	//gbl->main_ctl_nid = gbl->nid;
	return PTL_OK;
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
		ptl_warn("using loopback\n");
		return get_loopback_vars(gbl);
	}

	ptl_test_rank = gbl->rank;

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

		gbl->main_ctl_nid = addr_to_nid(inp.s_addr);

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

#if TEMP_PHYSICAL_NI
	return PTL_OK;
#endif

	pid = fork();
	if (pid == 0) {
		char str[20][60];		/* 20 strings of 60 chars */
		char *argv[21];
		int i;

		/* Close all the file descriptors, except the standard ones. */
		for (i=getdtablesize()-1; i>=3; i--) {
			close(i);
		}

		i = 0;
		sprintf(str[i++], "../src/control");

		sprintf(str[i++], "-j");
		sprintf(str[i++], "%d", gbl->jid);

		sprintf(str[i++], "-t");
		sprintf(str[i++], "%u", gbl->local_nranks);

		sprintf(str[i++], "-s");
		sprintf(str[i++], "%u", gbl->nranks);

		sprintf(str[i++], "-n");
		sprintf(str[i++], "%u", gbl->nid);

		sprintf(str[i++], "-m");
		sprintf(str[i++], "%u", gbl->main_ctl_nid);

		sprintf(str[i++], "-u");
		sprintf(str[i++], "%u", gbl->num_nids);

#if 0
		//todo: use env variables to decide whether to set these
		sprintf(str[i++], "-v");

		sprintf(str[i++], "-l");
		sprintf(str[i++], "100");
#endif

		argv[i] = NULL;
		while(i) {
			i--;
			argv[i] = str[i];
		}

		/* Change the session. */
		if (setsid() == -1) {
			perror("Couldn't change session id\n");
			return 1;
		}

		execv(argv[0], argv);

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
	evl_run(&evl);
	return NULL;
}

static int gbl_init(gbl_t *gbl)
{
	int err;

#if !TEMP_PHYSICAL_NI
	err = get_vars(gbl);
	if (unlikely(err)) {
		ptl_warn("get_vars failed\n");
		goto err1;
	}
#endif

	start_control_daemon(gbl);

	evl_init(&evl);

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
	if (iface >= MAX_IFACE || ni_type >= MAX_NI_TYPES)
		return NULL;

	return gbl->iface[iface].ni[ni_type];
}

/* caller must hold global mutex */
int gbl_add_ni(gbl_t *gbl, ni_t *ni)
{
	/* Ensure there's no NI there already. */
	if (ni->iface >= MAX_IFACE || ni->ni_type >= MAX_NI_TYPES || gbl->iface[ni->iface].ni[ni->ni_type])
		return PTL_ARG_INVALID;

	gbl->iface[ni->iface].ni[ni->ni_type] = ni;
	//sprintf(gbl->iface[ni->iface].if_name, "ib%d", ni->iface);

	return PTL_OK;
}

/* caller must hold global mutex */
int gbl_remove_ni(gbl_t *gbl, ni_t *ni)
{
	ptl_interface_t iface = ni->iface;

	if (unlikely(ni != gbl->iface[iface].ni[ni->ni_type]))
		return PTL_FAIL;

	//gbl->iface[iface].if_name[0] = 0;
	gbl->iface[iface].ni[ni->ni_type] = NULL;

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
