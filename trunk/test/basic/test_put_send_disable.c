#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "testing.h"

#if MATCHING == 1
# define ENTRY_T  ptl_me_t
# define HANDLE_T ptl_handle_me_t
# define NI_TYPE  PTL_NI_MATCHING
# define OPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_UNLINK_DISABLE)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define OPTIONS  (PTL_LE_OP_PUT | PTL_LE_EVENT_LINK_DISABLE | PTL_LE_EVENT_UNLINK_DISABLE)
# define APPEND   PtlLEAppend
# define UNLINK   PtlLEUnlink
#endif /* if MATCHING == 1 */

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_h;
    ptl_pt_index_t  pt_index;
    uint64_t        value;
    ENTRY_T         value_e;
    HANDLE_T        value_e_handle;
    ptl_md_t        write_md;
    ptl_handle_md_t write_md_handle;
    int             num_procs;
    ptl_handle_eq_t eq_h;
    int rank;
    int ret;
    ptl_process_t *procs;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();

    /* This test only succeeds if we have more than one rank */
    if (num_procs < 2) return 77;

#if PHYSICAL_ADDR == 0
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));
#else
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_PHYSICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));
#endif

    procs = libtest_get_mapping(ni_h);

#if PHYSICAL_ADDR == 0
    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));
#endif

    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 8192, &eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, eq_h, PTL_PT_ANY,
                               &pt_index));
    assert(pt_index == 0);

    if (1 == rank) {
        value_e.start  = &value;
        value_e.length = sizeof(uint64_t);
        value_e.uid    = PTL_UID_ANY;
#if MATCHING == 1
 #if PHYSICAL_ADDR == 0
        value_e.match_id.rank = PTL_RANK_ANY;
 #else
	value_e.match_id.phys.nid = PTL_NID_ANY;
	value_e.match_id.phys.pid = PTL_PID_ANY;
 #endif
        value_e.match_bits    = 1;
        value_e.ignore_bits   = 0;
#endif
        value_e.options = OPTIONS;
        value_e.ct_handle = PTL_CT_NONE;

        CHECK_RETURNVAL(APPEND(ni_h, 0, &value_e, PTL_PRIORITY_LIST, NULL,
                               &value_e_handle));

        value = 0;
    } else if (0 == rank) {
        /* set up the landing pad so that I can read others' values */
        write_md.start     = &value;
        write_md.length    = sizeof(uint64_t);
        write_md.options   = PTL_MD_EVENT_SEND_DISABLE;
        write_md.eq_handle = eq_h;
        write_md.ct_handle = PTL_CT_NONE;
        CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_handle));

        value = 0xdeadbeef;
    }

    libtest_barrier();

    /* 0 writes to 1 */
    if (1 == rank) {
        /* wait for write to arrive */
        ptl_event_t ev;
        ret = PtlEQWait(eq_h, &ev);
        assert(ev.ni_fail_type == PTL_OK);
        assert(ev.type == PTL_EVENT_PUT);
        assert(value == 0xdeadbeef);
    } else if (0 == rank) {
        /* write to rank 1 */
        ptl_process_t peer;
        ptl_event_t ev;

#if PHYSICAL_ADDR == 0
	peer.rank = 1;
#else
        peer = procs[1];
#endif
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        ret = PtlEQWait(eq_h, &ev);
        assert(PTL_OK == ret);
        assert(ev.ni_fail_type == PTL_OK);
        assert(ev.type == PTL_EVENT_ACK);
    }

    libtest_barrier();

    /* cleanup */
    if (1 == rank) {
        CHECK_RETURNVAL(UNLINK(value_e_handle));
    } else if (0 == rank) {
        CHECK_RETURNVAL(PtlMDRelease(write_md_handle));
    }

    CHECK_RETURNVAL(PtlEQFree(eq_h));
    CHECK_RETURNVAL(PtlPTFree(ni_h, pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_h));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
