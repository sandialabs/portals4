/*
 * ptl_gbl.c
 */

#include "ptl_loc.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>

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

/*
 * iface_init
 *	init iface table
 */
void iface_init(gbl_t *gbl)
{
	int i;

	//memset(gbl->iface, 0, MAX_IFACE*sizeof(*gbl->iface));

	for (i = 0; i < MAX_IFACE; i++) {
		gbl->iface[i].id.phys.nid = PTL_NID_ANY;
		gbl->iface[i].id.phys.pid = PTL_PID_ANY;
	}
}

/*
 * iface_fini
 *	fini iface table
 */
void iface_fini(gbl_t *gbl)
{
}

static void gbl_release(ref_t *ref)
{
	gbl_t *gbl = container_of(ref, gbl_t, ref);

	/* cleanup ni object pool */
	pool_fini(&gbl->ni_pool);

	/* fini the index service */
	index_fini();

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

	iface_fini(gbl);

	pthread_mutex_destroy(&gbl->gbl_mutex);
}

static void *event_loop_func(void *arg)
{
	evl_run(&evl);
	return NULL;
}

static int gbl_init(gbl_t *gbl)
{
	int err;

	iface_init(gbl);

	pthread_mutex_init(&gbl->gbl_mutex, NULL);

	/* init the index service */
	err = index_init();
	if (err)
		return err;

	pagesize = sysconf(_SC_PAGESIZE);
	linesize = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

	/* Create the event loop thread. */
	evl_init(&evl);

	err = pthread_create(&gbl->event_thread, NULL, event_loop_func, gbl);
	if (unlikely(err)) {
		ptl_warn("event loop creation failed\n");
		goto err;
	}
	gbl->event_thread_run = 1;

	/* init ni object pool */
	err = pool_init(&gbl->ni_pool, "ni", sizeof(ni_t), POOL_NI, NULL);
	if (err) {
		WARN();
		goto err;
	}

	return PTL_OK;

err:
	pthread_mutex_destroy(&gbl->gbl_mutex);
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

/*
 * gbl_lookup_ni
 *	lookup ni in iface table
 *	caller should hold global mutex
 */
ni_t *gbl_lookup_ni(gbl_t *gbl, ptl_interface_t iface, int ni_type)
{
	if (iface >= MAX_IFACE || ni_type >= MAX_NI_TYPES) {
		WARN();
		return NULL;
	}

	return gbl->iface[iface].ni[ni_type];
}

/*
 * gbl_add_ni
 *	add ni to iface table
 *	caller should hold global mutex
 */
int gbl_add_ni(gbl_t *gbl, ni_t *ni)
{
	struct iface *iface;

	/* Ensure there's no NI there already. */
	if (ni->ifacenum >= MAX_IFACE ||
	    ni->ni_type >= MAX_NI_TYPES) {
		WARN();
		return PTL_ARG_INVALID;
	}

	iface = &gbl->iface[ni->ifacenum];

	if (iface->ni[ni->ni_type]) {
		WARN();
		return PTL_ARG_INVALID;
	}

	iface->ni[ni->ni_type] = ni;
	ni->iface = iface;
	
	return PTL_OK;
}

/*
 * gbl_remove_ni
 *	remove ni from iface table
 *	caller should hold global mutex
 */
int gbl_remove_ni(gbl_t *gbl, ni_t *ni)
{
	struct iface *iface = ni->iface;

	if (unlikely(ni != iface->ni[ni->ni_type])) {
		WARN();
		return PTL_FAIL;
	}

	iface->ni[ni->ni_type] = NULL;
	ni->iface = NULL;

	return PTL_OK;
}

int PtlInit()
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
			usleep(100000);
		if (gbl->ref.ref_cnt > 0) {
			WARN();
			ret = PTL_FAIL;
			goto err1;
		} else {
			ref_init(&gbl->ref);
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
