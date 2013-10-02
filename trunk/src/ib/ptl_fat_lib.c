/**
 * @file ptl_fat_lib.c
 *
 * @brief Specific parts for the fat library only.
 */

#include "ptl_loc.h"

/* Define to be able to get a dump of some of the data structures. */
//#define DEBUG_DUMP

/*
 * per process global state
 * acquire proc_gbl_mutex before making changes
 * that require atomicity
 */
gbl_t per_proc_gbl;

/* Event loop. */
struct evl evl;

#if !WITH_TRANSPORT_UDP
static void stop_event_loop_func(EV_P_ ev_async *w, int revents)
{
    ev_break(evl.loop, EVBREAK_ALL);
}
#endif

void gbl_release(ref_t *ref)
{
    gbl_t *gbl = container_of(ref, gbl_t, ref);

    /* cleanup ni object pool */
    pool_fini(&gbl->ni_pool);

    /* fini the index service */
    index_fini(gbl);

    /* Terminate the event loop, which will terminate the event
     * thread. */
#if !WITH_TRANSPORT_UDP
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
#endif
    iface_fini(gbl);

    pthread_mutex_destroy(&gbl->gbl_mutex);
}

static void *event_loop_func(void *arg)
{
    evl_run(&evl);
    return NULL;
}

#ifdef DEBUG_DUMP
static void dump_buf(buf_t *buf)
{
    printf("  Buffer %x\n", buf_to_handle(buf));
    printf("    type = %d\n", buf->type);
    printf("    xxbuf = %p\n", buf->transfer.rdma.xxbuf);

    printf("    init_state = %d\n", buf->init_state);
    printf("    tgt_state = %d\n", buf->tgt_state);
    printf("    recv_state = %d\n", buf->recv_state);

    printf("    event_mask = %x\n", buf->event_mask);
    printf("    recv_buf = %p\n", buf->recv_buf);
    //      printf("    ack_req = %d\n", buf->ack_req);
    //      printf("    state_waiting = %d\n", buf->state_waiting);
    printf("    ref = %d\n", buf_ref_cnt(buf));
}

static void dump_everything(int unused)
{
    int i, j, k;
    gbl_t *gbl = &per_proc_gbl;

    printf("Dumping gbl\n");

    gbl_get();

    for (i = 0; i < gbl->num_iface; i++) {

        for (j = 0; j < 4; j++) {
            ni_t *ni = gbl->iface[i].ni[j];

            if (!ni)
                continue;

            printf("Dumping NI %d:\n", j);
            printf("  options: %x\n", ni->options);
            printf("  recv_list: %d\n", list_empty(&ni->rdma.recv_list));

            printf("  buffers used: %d\n", atomic_read(&ni->buf_pool.count));

            printf("  limits.max_entries = %d\n", ni->limits.max_entries);
            printf("  limits.max_unexpected_headers = %d\n",
                   ni->limits.max_unexpected_headers);
            printf("  limits.max_mds = %d\n", ni->limits.max_mds);
            printf("  limits.max_cts = %d\n", ni->limits.max_cts);
            printf("  limits.max_eqs = %d\n", ni->limits.max_eqs);
            printf("  limits.max_pt_index = %d\n", ni->limits.max_pt_index);
            printf("  limits.max_iovecs = %d\n", ni->limits.max_iovecs);
            printf("  limits.max_list_size = %d\n", ni->limits.max_list_size);
            printf("  limits.max_triggered_ops = %d\n",
                   ni->limits.max_triggered_ops);
            printf("  limits.max_msg_size = %zd\n", ni->limits.max_msg_size);
            printf("  limits.max_atomic_size = %zd\n",
                   ni->limits.max_atomic_size);
            printf("  limits.max_fetch_atomic_size = %zd\n",
                   ni->limits.max_fetch_atomic_size);
            printf("  limits.max_waw_ordered_size = %zd\n",
                   ni->limits.max_waw_ordered_size);
            printf("  limits.max_war_ordered_size = %zd\n",
                   ni->limits.max_war_ordered_size);
            printf("  limits.max_volatile_size = %zd\n",
                   ni->limits.max_volatile_size);
            printf("  limits.features = %d\n", ni->limits.features);


            printf("  current.max_entries = %d\n", ni->current.max_entries);
            printf("  current.max_unexpected_headers = %d\n",
                   ni->current.max_unexpected_headers);
            printf("  current.max_mds = %d\n", ni->current.max_mds);
            printf("  current.max_cts = %d\n", ni->current.max_cts);
            printf("  current.max_eqs = %d\n", ni->current.max_eqs);
            printf("  current.max_pt_index = %d\n", ni->current.max_pt_index);
            printf("  current.max_iovecs = %d\n", ni->current.max_iovecs);
            printf("  current.max_list_size = %d\n",
                   ni->current.max_list_size);
            printf("  current.max_triggered_ops = %d\n",
                   ni->current.max_triggered_ops);
            printf("  current.max_msg_size = %zd\n",
                   ni->current.max_msg_size);
            printf("  current.max_atomic_size = %zd\n",
                   ni->current.max_atomic_size);
            printf("  current.max_fetch_atomic_size = %zd\n",
                   ni->current.max_fetch_atomic_size);
            printf("  current.max_waw_ordered_size = %zd\n",
                   ni->current.max_waw_ordered_size);
            printf("  current.max_war_ordered_size = %zd\n",
                   ni->current.max_war_ordered_size);
            printf("  current.max_volatile_size = %zd\n",
                   ni->current.max_volatile_size);
            printf("  current.features = %d\n", ni->current.features);


            printf("  PTs:\n");
            for (k = 0; k <= ni->limits.max_pt_index; k++) {
                pt_t *pt = &ni->pt[k];
                if (!pt->in_use)
                    continue;

                printf("  PT %d:\n", k);
                printf("    	priority_size = %d\n", pt->priority_size);
                printf("    	priority_list = %d\n",
                       list_empty(&pt->priority_list));
                printf("    	overflow_size = %d\n", pt->overflow_size);
                printf("    	overflow_list = %d\n",
                       list_empty(&pt->overflow_list));
                printf("    	unexpected_size = %d\n", pt->unexpected_size);
                printf("    	unexpected_list = %d\n",
                       list_empty(&pt->unexpected_list));
            }

#if 0
            printf("  EQs:\n");
            obj_t *obj;
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

#endif
            if (ni->options & PTL_NI_LOGICAL) {
                printf("  Connections on logical NI:\n");

                if (ni->logical.rank_table) {
                    for (k = 0; k < ni->logical.map_size; k++) {
                        struct rank_entry *entry = &ni->logical.rank_table[k];
                        printf("    rank            = %d\n", entry->rank);
                        printf("    max pending wr  = %d\n",
                               entry->connect->rdma.max_req_avail);
                        printf("    pending send wr = %d\n",
                               atomic_read(&entry->connect->
                                           rdma.num_req_posted));
                    }
                }
            }
#if 0
            {
                /* Note: This will break everything if the process is not stuck
                 * on something. */
                struct ibv_wc wc;
                printf("  polling CQ: ret=%d\n",
                       ibv_poll_cq(ni->rdma.cq, 1, &wc));
            }
#endif


        }
    }

    gbl_put();
}
#endif

int gbl_init(gbl_t *gbl)
{
    int err;

#ifdef DEBUG_DUMP
    signal(SIGUSR1, dump_everything);
#endif

    /* Misc initializations. */
    err = misc_init_once();
    if (err)
        return PTL_FAIL;

    /* Init the index service */
    err = index_init(gbl);
    if (err)
        return err;

    err = init_iface_table(gbl);
    if (err)
        return err;

    pthread_mutex_init(&gbl->gbl_mutex, NULL);

    /* Create the event loop thread. */
    evl_init(&evl);

    err = pthread_create(&gbl->event_thread, NULL, event_loop_func, gbl);
    if (unlikely(err)) {
        ptl_warn("event loop creation failed\n");
        goto err;
    }
    gbl->event_thread_run = 1;

    /* init ni object pool */
    err = pool_init(gbl, &gbl->ni_pool, "ni", sizeof(ni_t), POOL_NI, NULL);
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
    return _PtlInit(&per_proc_gbl);
}

void PtlFini(void)
{
    _PtlFini(&per_proc_gbl);
}

int PtlNIInit(ptl_interface_t iface_id, unsigned int options, ptl_pid_t pid,
              const ptl_ni_limits_t *desired, ptl_ni_limits_t *actual,
              ptl_handle_ni_t *ni_handle)
{
    return _PtlNIInit(&per_proc_gbl, iface_id, options, pid, desired, actual,
                      ni_handle);
}

int PtlNIFini(ptl_handle_ni_t ni_handle)
{
    int ret;

    for (;;) {
        ret = _PtlNIFini(&per_proc_gbl, ni_handle);
        if (ret == PTL_IN_USE)
            usleep(10000);
        else
            break;
    }

    return ret;
}
