/*
 * Test flow control where we exhaust the eq, but have room to receive messages 
 */

#include <portals4.h>
#include <support/support.h>

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
# define OPTIONS  (PTL_ME_OP_PUT)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define OPTIONS  (PTL_LE_OP_PUT)
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
    HANDLE_T        value_e_handle, signal_e_handle;
    int             num_procs;
    ptl_handle_eq_t eq_handle;
    ptl_handle_ct_t ct_handle;
    ptl_handle_md_t md_handle;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();
    if (num_procs < 2) {
        fprintf(stderr, "test_flowctl_noeq requires at least two processes\n");
        return 77;
    }

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_handle));
    procs = libtest_get_mapping(ni_handle);
    CHECK_RETURNVAL(PtlSetMap(ni_handle, num_procs, procs));


    if (0 == rank) {
        ENTRY_T         value_e;

        /* create data ME */
        CHECK_RETURNVAL(PtlEQAlloc(ni_handle, (num_procs - 1) * ITERS / 2, &eq_handle));
        CHECK_RETURNVAL(PtlPTAlloc(ni_handle, PTL_PT_FLOWCTRL, eq_handle, 5,
                                   &pt_index));
        value_e.start = NULL;
        value_e.length = 0;
        value_e.ct_handle = PTL_CT_NONE;
        value_e.uid = PTL_UID_ANY;
        value_e.options = OPTIONS;
#if INTERFACE == 1
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits = 0;
        value_e.ignore_bits = 0;
#endif
        CHECK_RETURNVAL(APPEND(ni_handle, 5, &value_e, PTL_PRIORITY_LIST, NULL, &value_e_handle));

        /* create signal ME */
        CHECK_RETURNVAL(PtlCTAlloc(ni_handle, &ct_handle));
        CHECK_RETURNVAL(PtlPTAlloc(ni_handle, 1, PTL_EQ_NONE, 6,
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
        CHECK_RETURNVAL(APPEND(ni_handle, 6, &value_e, PTL_PRIORITY_LIST, NULL, &signal_e_handle));
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

    libtest_barrier();

    if (0 == rank) {
        ptl_ct_event_t  ct;
        ptl_event_t ev;
        int ret, count = 0, saw_dropped = 0, saw_flowctl = 0;

        /* wait for signal counts */
        CHECK_RETURNVAL(PtlCTWait(ct_handle, num_procs - 1, &ct));
        if (ct.success != num_procs - 1 || ct.failure != 0) {
            return 1;
        }

        fprintf(stderr, "0: got all my signals\n");
        
        /* wait for event entries */
        while (count < ITERS * (num_procs - 1)) {
            ret = PtlEQGet(eq_handle, &ev);
            if (PTL_OK == ret) {
                count++;
            } else if (PTL_EQ_DROPPED) {
                count++;
                saw_dropped++;
            } else {
                fprintf(stderr, "0: Unexpected return code from EQGet: %d\n", ret);
                return 1;
            }

            if (ev.type == PTL_EVENT_PT_DISABLED) {
                saw_flowctl++;
                break;
            }
        }

        fprintf(stderr, "0: Saw %d dropped, %d flowctl\n", saw_dropped, saw_flowctl);
        if (saw_dropped != 0 || saw_flowctl == 0) {
            return 1;
        }
    } else {
        ptl_process_t target;
        ptl_event_t ev;
        int ret, count = 0, fails = 0;
        int i;

        target.rank = 0;
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

        fprintf(stderr, "%d: done with sends\n", rank);

        while (count < ITERS) {
            ret = PtlEQGet(eq_handle, &ev);
            if (PTL_EQ_EMPTY == ret) {
                continue;
            } else if (PTL_OK != ret) {
                fprintf(stderr, "%d: PtlEQGet returned %d\n", rank, ret);
                return 1;
            }

            fprintf(stderr, "%d: got event %d (%d %d)\n", rank, count, ev.type, ev.ni_fail_type);

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
            } else {
                fprintf(stderr, "%d: Unexpected fail type: %d\n", rank, ev.ni_fail_type);
                return 1;
            }
        }

        fprintf(stderr, "%d: Saw %d of %d events as fails\n", rank, fails, count);

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
        /* wait for the send event on the last put */
        CHECK_RETURNVAL(PtlEQWait(eq_handle, &ev));
    }

    if (0 == rank) {
        CHECK_RETURNVAL(UNLINK(signal_e_handle));
        CHECK_RETURNVAL(PtlPTFree(ni_handle, signal_pt_index));
        CHECK_RETURNVAL(PtlCTFree(ct_handle));
        CHECK_RETURNVAL(UNLINK(value_e_handle));
        CHECK_RETURNVAL(PtlPTFree(ni_handle, pt_index));
        CHECK_RETURNVAL(PtlEQFree(eq_handle));
    } else {
        CHECK_RETURNVAL(PtlMDRelease(md_handle));
        CHECK_RETURNVAL(PtlEQFree(eq_handle));
    }

    libtest_barrier();

    CHECK_RETURNVAL(PtlNIFini(ni_handle));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
