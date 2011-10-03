/*
 * ptl_gbl.c
 */

#include "ptl_loc.h"

#include <netdb.h>
#include <sys/wait.h>

/* Define to be able to get a dump of some of the data structures. */
#undef DEBUG_DUMP

/*
 * per process global state
 * acquire proc_gbl_mutex before making changes
 * that require atomicity
 */
gbl_t per_proc_gbl;
static pthread_mutex_t per_proc_gbl_mutex = PTHREAD_MUTEX_INITIALIZER;

unsigned int pagesize;
unsigned int linesize;

/* Event loop. */
struct evl evl;

static void stop_event_loop_func(EV_P_ ev_async *w, int revents)
{
	ev_break(evl.loop, EVBREAK_ALL);
}

void gbl_release(ref_t *ref)
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

#ifdef DEBUG_DUMP
static void dump_xi(xi_t *xi)
{
	printf("  XI %x\n", xi_to_handle(xi));

	printf("    state = %d\n", xi->state);
	printf("    next_state = %d\n", xi->next_state);
	printf("    event_mask = %x\n", xi->event_mask);
	printf("    recv_buf = %p\n", xi->recv_buf);
	printf("    ack_req = %d\n", xi->ack_req);
	printf("    state_waiting = %d\n", xi->state_waiting);
	printf("    ref = %d\n", xi->obj.obj_ref.ref_cnt);
}

static void dump_everything(int unused)
{
	int i, j, k;

	printf("Dumping gbl\n");
	
	get_gbl();

	for (i=0; i<gbl->num_iface; i++) {

		for(j=0; j<4; j++) {
			ni_t *ni = gbl->iface[i].ni[j];
			xi_t *xi;
			obj_t *obj;

			if (!ni)
				continue;

			printf("Dumping NI %d:\n", j);
			printf("  options: %x\n", ni->options);
			printf("  xi_wait_list: %d\n", list_empty(&ni->xi_wait_list));
			printf("  xt_wait_list: %d\n", list_empty(&ni->xt_wait_list));
			printf("  send_list: %d\n", list_empty(&ni->send_list));
			printf("  rdma_list: %d\n", list_empty(&ni->rdma_list));
			printf("  recv_list: %d\n", list_empty(&ni->rdma.recv_list));

			printf("  limits.max_entries = %d\n", ni->limits.max_entries);
			printf("  limits.max_unexpected_headers = %d\n", ni->limits.max_unexpected_headers);
			printf("  limits.max_mds = %d\n", ni->limits.max_mds);
			printf("  limits.max_cts = %d\n", ni->limits.max_cts);
			printf("  limits.max_eqs = %d\n", ni->limits.max_eqs);
			printf("  limits.max_pt_index = %d\n", ni->limits.max_pt_index);
			printf("  limits.max_iovecs = %d\n", ni->limits.max_iovecs);
			printf("  limits.max_list_size = %d\n", ni->limits.max_list_size);
			printf("  limits.max_triggered_ops = %d\n", ni->limits.max_triggered_ops);
			printf("  limits.max_msg_size = %zd\n", ni->limits.max_msg_size);
			printf("  limits.max_atomic_size = %zd\n", ni->limits.max_atomic_size);
			printf("  limits.max_fetch_atomic_size = %zd\n", ni->limits.max_fetch_atomic_size);
			printf("  limits.max_waw_ordered_size = %zd\n", ni->limits.max_waw_ordered_size);
			printf("  limits.max_war_ordered_size = %zd\n", ni->limits.max_war_ordered_size);
			printf("  limits.max_volatile_size = %zd\n", ni->limits.max_volatile_size);
			printf("  limits.features = %d\n", ni->limits.features);


			printf("  current.max_entries = %d\n", ni->current.max_entries);
			printf("  current.max_unexpected_headers = %d\n", ni->current.max_unexpected_headers);
			printf("  current.max_mds = %d\n", ni->current.max_mds);
			printf("  current.max_cts = %d\n", ni->current.max_cts);
			printf("  current.max_eqs = %d\n", ni->current.max_eqs);
			printf("  current.max_pt_index = %d\n", ni->current.max_pt_index);
			printf("  current.max_iovecs = %d\n", ni->current.max_iovecs);
			printf("  current.max_list_size = %d\n", ni->current.max_list_size);
			printf("  current.max_triggered_ops = %d\n", ni->current.max_triggered_ops);
			printf("  current.max_msg_size = %zd\n", ni->current.max_msg_size);
			printf("  current.max_atomic_size = %zd\n", ni->current.max_atomic_size);
			printf("  current.max_fetch_atomic_size = %zd\n", ni->current.max_fetch_atomic_size);
			printf("  current.max_waw_ordered_size = %zd\n", ni->current.max_waw_ordered_size);
			printf("  current.max_war_ordered_size = %zd\n", ni->current.max_war_ordered_size);
			printf("  current.max_volatile_size = %zd\n", ni->current.max_volatile_size);
			printf("  current.features = %d\n", ni->current.features);


			printf("  XI:\n");
			list_for_each_entry(xi, &ni->xi_wait_list, list) {
				dump_xi(xi);
			}

			printf("  PTs:\n");
			for (k=0; k<ni->limits.max_pt_index; k++) {
				pt_t *pt = &ni->pt[k];
				if (!pt->in_use)
					continue;

				printf("  PT %d:\n", k);
				printf("    	priority_size = %d\n", pt->priority_size);
				printf("    	priority_list = %d\n", list_empty(&pt->priority_list));
				printf("    	overflow_size = %d\n", pt->overflow_size);
				printf("    	overflow_list = %d\n", list_empty(&pt->overflow_list));
				printf("    	unexpected_size = %d\n", pt->unexpected_size);
				printf("    	unexpected_list = %d\n", list_empty(&pt->unexpected_list));
			}

#if 0
			printf("  EQs:\n");
			list_for_each_entry(obj, &ni->eq_pool.busy_list, obj_list) {
				eq_t *eq = container_of(obj, eq_t, obj);

				printf("  EQ: %x\n", eq_to_handle(eq));
				printf("    count = %d\n", eq->count);
				printf("    producer = %d\n", eq->producer);
				printf("    consumer = %d\n", eq->consumer);
				printf("    prod_gen = %d\n", eq->prod_gen);
				printf("    cons_gen = %d\n", eq->cons_gen);
				printf("    interrupt = %d\n", eq->interrupt);
				printf("    overflow = %d\n", eq->overflow);
			}

			printf("  XIs:\n");
			list_for_each_entry(obj, &ni->xi_pool.busy_list, obj_list) {
				xi_t *xi = container_of(obj, xi_t, obj);

				dump_xi(xi);
			}

			printf("  XTs:\n");
			list_for_each_entry(obj, &ni->xt_pool.busy_list, obj_list) {
				xt_t *xt = container_of(obj, xt_t, obj);

				printf("  XT %x\n", xt_to_handle(xt));
				printf("    state = %d\n", xt->state);
				printf("    rdma_comp = %d\n", xt->rdma.rdma_comp);
				printf("    rdma_dir = %d\n", xt->rdma_dir);
				printf("    put_resid = %ld\n", xt->put_resid);
				printf("    get_resid = %ld\n", xt->get_resid);
			}
#endif

#if 0
			{
				/* Note: This will break everything if the process is not stuck
				 * on something. */
				struct ibv_wc wc;
				printf("  polling CQ: ret=%d\n", ibv_poll_cq(ni->rdma.cq, 1, &wc));
			}
#endif
			

		}
	}

	gbl_put();
}
#endif

static int gbl_init(gbl_t *gbl)
{
	int err;

#ifdef DEBUG_DUMP
	signal(SIGUSR1, dump_everything);
#endif

	err = iface_init(gbl);
	if (err)
		return err;

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
			usleep(100000);
		if (gbl->ref.ref_cnt > 0) {
			WARN();
			ret = PTL_FAIL;
			goto err1;
		} else {
			ref_set(&gbl->ref, 1);
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
	   so ref_set must come before the initial
	   ref_cnt++ and ref_put must come after
	   the last ref_cnt-- */
	gbl->ref_cnt--;

	if (gbl->ref_cnt == 0)
		ref_put(&gbl->ref, gbl_release);	/* matches ref_set */

	pthread_mutex_unlock(&per_proc_gbl_mutex);

	return;

err1:
	pthread_mutex_unlock(&per_proc_gbl_mutex);
err0:
	return;
}
