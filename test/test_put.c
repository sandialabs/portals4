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
# define OPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_CT_COMM)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define OPTIONS  (PTL_LE_OP_PUT | PTL_LE_EVENT_CT_COMM)
# define APPEND   PtlLEAppend
# define UNLINK   PtlLEUnlink
#endif /* if INTERFACE == 1 */

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_h;
    ptl_process_t   myself;
    ptl_pt_index_t  pt_index;
    uint64_t        value;
    ENTRY_T         value_e;
    HANDLE_T        value_e_handle;
    ptl_md_t        write_md;
    ptl_handle_md_t write_md_handle;
    int             num_procs;
    ptl_ct_event_t ctc;
    int rank;
    int ret;
    ptl_process_t *procs;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();

    /* This test only succeeds if we have more than one rank */
    if (num_procs < 2) return 77;

#if LOGICAL
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));
#else
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_PHYSICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));
#endif

    CHECK_RETURNVAL(PtlGetId(ni_h, &myself));
    procs = libtest_get_mapping(ni_h);

#if LOGICAL
    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));
#endif

    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, PTL_EQ_NONE, PTL_PT_ANY,
                               &pt_index));
    assert(pt_index == 0);

    if (1 == rank) {
        value_e.start  = &value;
        value_e.length = sizeof(uint64_t);
        value_e.uid    = PTL_UID_ANY;
#if INTERFACE == 1
 #if LOGICAL == 1
        value_e.match_id.rank = PTL_RANK_ANY;
 #else
	value_e.match_id.phys.nid = PTL_NID_ANY;
	value_e.match_id.phys.pid = PTL_PID_ANY;
 #endif
        value_e.match_bits    = 1;
        value_e.ignore_bits   = 0;
#endif
        value_e.options = OPTIONS;
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &value_e.ct_handle));
        CHECK_RETURNVAL(APPEND(ni_h, 0, &value_e, PTL_PRIORITY_LIST, NULL,
                               &value_e_handle));

        value = 0;
    } else if (0 == rank) {
        /* set up the landing pad so that I can read others' values */
        write_md.start     = &value;
        write_md.length    = sizeof(uint64_t);
        write_md.options   = PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_CT_ACK;
        write_md.eq_handle = PTL_EQ_NONE;   // i.e. don't queue send events
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &write_md.ct_handle));
        CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_handle));

        value = 0xdeadbeef;
    }

    libtest_barrier();

    /* 0 writes to 1 */
    if (1 == rank) {
        /* wait for write to arrive */
        ret = PtlCTWait(value_e.ct_handle, 1, &ctc);
        assert(ctc.failure == 0);
        assert(value == 0xdeadbeef);
    } else if (0 == rank) {
        /* write to rank 1 */
        ptl_process_t peer;
#if LOGICAL == 1        
	peer.rank = 1;
#else
        peer = procs[1];
#endif
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 2, &ctc));
        assert(ctc.failure == 0);
    }

    libtest_barrier();

    /* cleanup */
    if (1 == rank) {
        CHECK_RETURNVAL(UNLINK(value_e_handle));
        CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle));
    } else if (0 == rank) {
        CHECK_RETURNVAL(PtlMDRelease(write_md_handle));
        CHECK_RETURNVAL(PtlCTFree(write_md.ct_handle));
    }

    CHECK_RETURNVAL(PtlPTFree(ni_h, pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_h));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
