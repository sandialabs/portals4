#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>

#include "testing.h"

#if MATCHING == 1
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
#endif /* if MATCHING == 1 */

#ifndef MIN
#define MIN(a, b) ((a < b) ? a : b)
#endif

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_h;
    ptl_pt_index_t  pt_index;
    uint64_t       *buf;
    ENTRY_T         entry;
    HANDLE_T        entry_h;
    ptl_md_t        md;
    ptl_handle_md_t md_h;
    int             rank;
    int             num_procs;
    int             ret;
    ptl_process_t  *procs;
    ptl_handle_eq_t eq_h;
    ptl_event_t     ev;
    ptl_hdr_data_t rcvd = 0;
    ptl_hdr_data_t goal = 0;
    ptl_hdr_data_t hdr_data = 1;
    ptl_size_t offset = sizeof(uint64_t);
    uint32_t distance;
    int sends = 0;

    CHECK_RETURNVAL(PtlInit());
    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();

    /* This test only succeeds if we have more than one rank */
    if (num_procs < 2) return 77;

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));

    procs = libtest_get_mapping(ni_h);
    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));

    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 1024, &eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, eq_h, 0, &pt_index));
    assert(pt_index == 0);
    
    buf = malloc(sizeof(uint64_t) * num_procs);
    assert(NULL != buf);

    md.start = buf;
    md.length = sizeof(uint64_t) * num_procs;
    md.options = PTL_MD_UNORDERED;
    md.eq_handle = eq_h;
    md.ct_handle = PTL_CT_NONE;
    CHECK_RETURNVAL(PtlMDBind(ni_h, &md, &md_h));

    entry.start = buf;
    entry.length = sizeof(uint64_t) * num_procs;
    entry.ct_handle = PTL_CT_NONE;
    entry.uid = PTL_UID_ANY;
    entry.options = OPTIONS;
#if MATCHING == 1
    entry.match_id.rank = PTL_RANK_ANY;
    entry.match_bits = 0;
    entry.ignore_bits = 0;
    entry.min_free = 0;
#endif
    CHECK_RETURNVAL(APPEND(ni_h, pt_index, &entry,
                           PTL_PRIORITY_LIST, NULL, &entry_h));

    /* ensure ME is linked before the barrier */
    CHECK_RETURNVAL(PtlEQWait(eq_h, &ev));
    assert( ev.type == PTL_EVENT_LINK );

    libtest_barrier();

    /* Bruck's Concatenation Algorithm */
    memcpy(buf, &rank, sizeof(uint64_t));
    for (distance = 1; distance < num_procs; distance *= 2) {
        ptl_size_t to_xfer;
        int peer;
        ptl_process_t proc;

        if (rank >= distance) {
            peer = rank - distance;
        } else {
            peer = rank + (num_procs - distance);
        }

        to_xfer = sizeof(uint64_t) * MIN(distance, num_procs - distance);
        proc.rank = peer;
        CHECK_RETURNVAL(PtlPut(md_h, 
                               0, 
                               to_xfer, 
                               PTL_NO_ACK_REQ, 
                               proc,
                               0,
                               0,
                               offset,
                               NULL,
                               hdr_data));
        sends += 1;

        /* wait for completion of the proper receive, and keep count
           of uncompleted sends.  "rcvd" is an accumulator to deal
           with out-of-order receives, which are IDed by the
           hdr_data */
        goal |= hdr_data;
        while ((rcvd & goal) != goal) {
            ret = PtlEQWait(eq_h, &ev);
            switch (ret) {
            case PTL_OK:
                if (ev.type == PTL_EVENT_SEND) {
                    sends -= 1;
                } else {
                    rcvd |= ev.hdr_data;
                    assert(ev.type == PTL_EVENT_PUT);
                    assert(ev.rlength == ev.mlength);
                    assert((ev.rlength == to_xfer) || (ev.hdr_data != hdr_data));
                }
                break;
            default:
                fprintf(stderr, "PtlEQWait failure: %d\n", ret);
                abort();
            }
        }
        
        hdr_data <<= 1;
        offset += to_xfer;
    }

    /* wait for any SEND_END events not yet seen */
    while (sends) {
        ret = PtlEQWait(eq_h, &ev);
        switch (ret) {
        case PTL_OK:
            assert( ev.type == PTL_EVENT_SEND );
            sends -= 1;
            break;
        default:
            fprintf(stderr, "PtlEQWait failure: %d\n", ret);
            abort();
        }
    }

    CHECK_RETURNVAL(PtlMDRelease(md_h));
    CHECK_RETURNVAL(UNLINK(entry_h));

    free(buf);

    libtest_barrier();

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_h, pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_h));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
