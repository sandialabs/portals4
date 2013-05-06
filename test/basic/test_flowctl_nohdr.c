/*
 * Test flow control where we have no unexpceted header space left
 */

#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "testing.h"

#if INTERFACE == 1
# define ENTRY_T  ptl_me_t
# define HANDLE_T ptl_handle_me_t
# define NI_TYPE  PTL_NI_MATCHING
# define OPTIONS  (PTL_ME_OP_PUT | PTL_ME_OP_GET | PTL_ME_EVENT_CT_COMM)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define OPTIONS  (PTL_LE_OP_PUT | PTL_LE_OP_GET | PTL_LE_EVENT_CT_COMM)
# define APPEND   PtlLEAppend
# define UNLINK   PtlLEUnlink
#endif /* if INTERFACE == 1 */

#define ITERS 16

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_handle;
    ptl_process_t   *procs;
    int             rank;
    ptl_pt_index_t  pt_index, signal_pt_index;
    HANDLE_T        signal_e_handle;
    HANDLE_T        signal_e2_handle;
    int             num_procs;
    ptl_handle_eq_t eq_handle;
    ptl_handle_ct_t ct_handle;
    ptl_handle_md_t md_handle;
    ptl_ni_limits_t limits_reqd, limits_actual;
    ENTRY_T         value_e;   
 
    limits_reqd.max_entries = 1024;
    limits_reqd.max_unexpected_headers = ITERS/2;
    limits_reqd.max_mds = 1024;
    limits_reqd.max_eqs = 1024;
    limits_reqd.max_cts = 1024;
    limits_reqd.max_pt_index = 64;
    limits_reqd.max_iovecs = 1024;
    limits_reqd.max_list_size = 1024;
    limits_reqd.max_triggered_ops = 1024;
    limits_reqd.max_msg_size = 1048576;
    limits_reqd.max_atomic_size = 1048576;
    limits_reqd.max_fetch_atomic_size = 1048576;
    limits_reqd.max_waw_ordered_size = 1048576;
    limits_reqd.max_war_ordered_size = 1048576;
    limits_reqd.max_volatile_size = 1048576; 
    limits_reqd.features = 0;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();
    if (num_procs < 2) {
        fprintf(stderr, "test_flowctl_noeq requires at least two processes\n");
        return 77;
    }

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, &limits_reqd, &limits_actual, &ni_handle));
    procs = libtest_get_mapping(ni_handle);
    CHECK_RETURNVAL(PtlSetMap(ni_handle, num_procs, procs));


    if (0 == rank) {

        /* create data PT space */
        CHECK_RETURNVAL(PtlEQAlloc(ni_handle, (num_procs - 1) * ITERS + 64, &eq_handle));
        CHECK_RETURNVAL(PtlPTAlloc(ni_handle, PTL_PT_FLOWCTRL, eq_handle, 5,
                                   &pt_index));

        /* create signal ME */
        CHECK_RETURNVAL(PtlCTAlloc(ni_handle, &ct_handle));
        CHECK_RETURNVAL(PtlPTAlloc(ni_handle, 1, eq_handle, 6,
                                   &signal_pt_index));
        value_e.start = NULL;
        value_e.length = 0;
        value_e.ct_handle = ct_handle;
        value_e.uid = PTL_UID_ANY;
        value_e.options = OPTIONS | PTL_LE_EVENT_CT_COMM;
#if INTERFACE == 1
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits = 0;
        value_e.ignore_bits = 0;
#endif
        CHECK_RETURNVAL(APPEND(ni_handle, 5, &value_e, PTL_OVERFLOW_LIST, NULL, &signal_e_handle));
    } else {
        ptl_md_t        md;

        /* 16 extra just in case... */
        CHECK_RETURNVAL(PtlEQAlloc(ni_handle, ITERS * 2 + 16, &eq_handle));

        md.start = NULL;
        md.length = 0;
        md.options = 0;
        md.eq_handle = eq_handle;
        md.ct_handle = PTL_CT_NONE;

        CHECK_RETURNVAL(PtlMDBind(ni_handle, &md, &md_handle));
    }

    fprintf(stderr,"at barrier \n");
    libtest_barrier();

    if (0 == rank) {
        ptl_ct_event_t  ct;
        ptl_event_t ev;
        int ret, count = 0, saw_flowctl = 0;

        fprintf(stderr,"begin ctwait \n");
        /* wait for signal counts */
        CHECK_RETURNVAL(PtlCTWait(ct_handle, num_procs - 1, &ct));
        if (ct.success != num_procs - 1 || ct.failure != 0) {
            return 1;
        }
        fprintf(stderr,"done CT wait \n");
        /* wait for event entries */
        while (1) {
            ret = PtlEQGet(eq_handle, &ev);
            if (PTL_OK == ret) {
                count++;
                fprintf(stderr, "found EQ value \n");
            } else {
                fprintf(stderr, "0: Unexpected return code from EQGet: %d\n", ret);
                return 1;
            }

            if (ev.type == PTL_EVENT_PT_DISABLED) {
                saw_flowctl++;
                break;
            }
        }

        fprintf(stderr, "0: Saw %d flowctl\n", saw_flowctl);
        if (saw_flowctl == 0) {
            return 1;
        }
        /* Now clear out all of the unexpected messages so we can clean up everything */
        CHECK_RETURNVAL(APPEND(ni_handle, 5, &value_e, PTL_PRIORITY_LIST, NULL, &signal_e2_handle));
    } else {
        ptl_process_t target;
        ptl_event_t ev;
        int ret, count = 0, fails = 0;
        int i;

        target.rank = 0;
        printf("beginning puts \n");
        for (i = 0 ; i < ITERS ; ++i) {
            CHECK_RETURNVAL(PtlPut(md_handle,
                                   0,
                                   0,
                                   PTL_ACK_REQ,
                                   target,
                                   5,
                                   0,
                                   0,
                                   NULL,
                                   0));
        }

        while (count < ITERS) {
            ret = PtlEQGet(eq_handle, &ev);
            if (PTL_EQ_EMPTY == ret) {
                continue;
            } else if (PTL_OK != ret) {
                fprintf(stderr, "%d: PtlEQGet returned %d\n", rank, ret);
                return 1;
            }

            if (ev.ni_fail_type == PTL_NI_OK) {
                if (ev.type == PTL_EVENT_SEND) {
                    continue;
                } else if (ev.type == PTL_EVENT_ACK) {
                    count++;
                } else {
                    fprintf(stderr, "%d: Unexpected event type %d\n", rank, ev.type);
                }
            } else if (ev.ni_fail_type == PTL_NI_PT_DISABLED) {
                count++;
                fails++;
            } else if (ev.ni_fail_type == PTL_EQ_EMPTY){
                continue;               
            } else if (ev.ni_fail_type == PTL_EQ_DROPPED){
                continue;
            } else {
                fprintf(stderr, "%d: Unexpected fail type: %d\n", rank, ev.ni_fail_type);
                return 1;
            }
        }

        fprintf(stderr, "%d: Saw %d of %d ACKs as fails\n", rank, fails, count);

        CHECK_RETURNVAL(PtlPut(md_handle,
                               0,
                               0,
                               PTL_NO_ACK_REQ,
                               target,
                               6,
                               0,
                               0,
                               NULL,
                               0));
        CHECK_RETURNVAL(PtlEQWait(eq_handle, &ev));
    }

    fprintf(stderr,"at final barrier \n");

    

    libtest_barrier();

    if (0 == rank) {
        CHECK_RETURNVAL(UNLINK(signal_e_handle));
        CHECK_RETURNVAL(UNLINK(signal_e2_handle));
        CHECK_RETURNVAL(PtlPTFree(ni_handle, signal_pt_index));
        CHECK_RETURNVAL(PtlCTFree(ct_handle));
        CHECK_RETURNVAL(PtlPTFree(ni_handle, pt_index));
        CHECK_RETURNVAL(PtlEQFree(eq_handle));
    } else {
        CHECK_RETURNVAL(PtlMDRelease(md_handle));
        CHECK_RETURNVAL(PtlEQFree(eq_handle));
    }

    fprintf(stderr,"final cleanup \n");
    CHECK_RETURNVAL(PtlNIFini(ni_handle));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
