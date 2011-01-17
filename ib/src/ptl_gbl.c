/*
 * ptl_gbl.c
 */

#include "ptl_loc.h"

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

static void gbl_release(ref_t *ref)
{
	gbl_t *gbl = container_of(ref, gbl_t, ref);

	/* fini the object allocator */
	obj_fini();

	pthread_mutex_destroy(&gbl->gbl_mutex);

	rpc_fini(gbl->rpc);
}

#ifdef notused
int ni_read(ni_t *ni)
{
	int err;
	char buf[64];

	err = recv(ni->fd, buf, sizeof(buf), 0);
	if (err < 0) {
		perror("read");
		return PTL_FAIL;
	}

	printf("got received %d bytes\n", err);
	
	return PTL_OK;
}

int gbl_read(gbl_t *gbl)
{
	int err;
	char buf[64];

	err = recv(gbl->fd, buf, sizeof(buf), 0);
	if (err < 0) {
		perror("read");
		return PTL_FAIL;
	}

	if (!strcmp(buf, "quit"))
		return PTL_FAIL;

	return PTL_OK;
}
#endif

/* Get an int value from an environment variable. */
static long getenv_val(const char *name, unsigned int *val)
{
	char *endptr;
	unsigned long myval;
	char *str;

	str = getenv(name);
	if (!str)
		return PTL_FAIL;

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

/* The job launcher gives some parameters through environment
 * variables:
 *
 * - PORTALS4_JIB : job ID
 * - PORTALS4_NID : Node ID
 */
static int get_vars(gbl_t *gbl)
{
	ptl_jid_t jid;

	if (getenv_val("PORTALS4_JIB", &jid) ||
		getenv_val("PORTALS4_NID", &gbl->nid)) {
		ptl_warn("PORTALS4_JIB or PORTALS4_NID environment variables not set\n");
		return PTL_FAIL;
	}

	gbl->jid = jid;

	return PTL_OK;
}

/* Start the control daemon. */
static int start_daemon(gbl_t *gbl)
{
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		char jobid[10];

		sprintf(jobid, "%d", gbl->jid);

		execl("./src/p4oibd",
			  "./src/p4oibd",
			  "-n", "nid_sample.csv", 
			  "-r", "rank_sample.csv",
			  "-j", jobid,
			  NULL);

		_exit(0);
	}

	/* Hack: leave some time for the process to start. Ideally we
	   should retry connect()'ing a few times instead. */
	usleep(1000);

	return 0;
}

/* Retrieve the shared memory filename, and mmap it. */
static int get_init_data(gbl_t *gbl)
{
	struct rpc_msg rpc_msg;
	int err;
	void *m;

	memset(&rpc_msg, 0, sizeof(rpc_msg));
	rpc_msg.type = QUERY_INIT_DATA;
	err = rpc_get(gbl->rpc->to_server, &rpc_msg, &rpc_msg);
	if (err)
		goto err;

	if (rpc_msg.reply_init_data.shmem_filesize == 0)
		goto err;

	gbl->shmem.fd = open(rpc_msg.reply_init_data.shmem_filename, O_RDONLY);
	if (gbl->shmem.fd == -1)
		goto err;

	m = mmap(NULL, rpc_msg.reply_init_data.shmem_filesize,
			 PROT_READ, MAP_SHARED, gbl->shmem.fd, 0);
	if (m == MAP_FAILED)
		goto err;

	gbl->shmem.m = (struct shared_config *)m;

	gbl->shmem.rank_table = m + gbl->shmem.m->rank_table_offset;
	gbl->shmem.nid_table = m + gbl->shmem.m->nid_table_offset;

	return PTL_OK;

 err:
	if (gbl->shmem.fd != -1) {
		close(gbl->shmem.fd);
		gbl->shmem.fd = -1;
	}
	return PTL_FAIL;
}

static void rpc_callback(struct session *session)
{
	printf("FZ- got RPC message - dropping it\n");
}

static int gbl_init(gbl_t *gbl)
{
	int err;

	err = get_vars(gbl);
	if (unlikely(err)) {
		ptl_warn("get_vars failed\n");
		goto err1;
	}

	start_daemon(gbl);

	err = rpc_init(rpc_type_client, ctl_port, &gbl->rpc, rpc_callback);
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

	err = get_init_data(gbl);
	if (err) {
		ptl_warn("get_init_data failed\n");
		goto err2;
	}

	/* init the object allocator */
	err = obj_init();
	if (unlikely(err)) {
		ptl_warn("obj_init failed\n");
		goto err2;
	}

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
	int i;
	ni_t *ni;

	for (i = 0; i < MAX_IFACE; i++) {
		if (gbl->iface[i].if_index != iface)
			continue;

		ni = gbl->iface[i].ni;
		if (!ni)
			continue;

		if (ni->ni_type != ni_type)
			continue;

		return ni;
	}

	return NULL;
}

/* caller must hold global mutex */
int gbl_add_ni(gbl_t *gbl, ni_t *ni)
{
	ptl_interface_t iface = ni->iface;
	int slot = -1;

	for (slot = 0; slot < MAX_IFACE; slot++) {
		if (!gbl->iface[slot].ni)
			goto found_slot;
	}

	return PTL_NO_SPACE;

found_slot:
	if (!if_indextoname(iface, gbl->iface[slot].if_name)) {
		return PTL_ARG_INVALID;
	}
	gbl->iface[slot].if_index = iface;
	gbl->iface[slot].ni = ni;

	ni->iface = iface;
	ni->slot = slot;
	return PTL_OK;
}

/* caller must hold global mutex */
int gbl_remove_ni(gbl_t *gbl, ni_t *ni)
{
	int slot = ni->slot;

	if (unlikely(ni != gbl->iface[slot].ni))
		return PTL_FAIL;

	gbl->iface[slot].if_index = 0;
	gbl->iface[slot].if_name[0] = 0;
	gbl->iface[slot].ni = NULL;

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
			ptl_warn("still cleaning up, ref.ref_cnt = %d\n", gbl->ref.ref_cnt);
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
