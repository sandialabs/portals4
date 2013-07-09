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
# define OPTIONS  (PTL_ME_OP_PUT | PTL_ME_UNEXPECTED_HDR_DISABLE | \
                   PTL_ME_EVENT_SUCCESS_DISABLE | PTL_ME_EVENT_CT_COMM | \
                   PTL_ME_IS_ACCESSIBLE | \
                   PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_UNLINK_DISABLE)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define OPTIONS  (PTL_LE_OP_PUT | PTL_LE_UNEXPECTED_HDR_DISABLE | \
                   PTL_LE_EVENT_SUCCESS_DISABLE | PTL_LE_EVENT_CT_COMM | \
                   PTL_LE_IS_ACCESSIBLE | \
                   PTL_LE_EVENT_LINK_DISABLE | PTL_LE_EVENT_UNLINK_DISABLE)
# define APPEND   PtlLEAppend
# define UNLINK   PtlLEUnlink
#endif /* if MATCHING == 1 */

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_h;
    ptl_pt_index_t  pt_index;
    ENTRY_T         value_e;
    HANDLE_T        value_e_handle;
    ptl_md_t        md;
    ptl_handle_md_t md_handle;
    int             num_procs;
    ptl_ct_event_t  ct;
    int rank;
    ptl_process_t *procs;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();

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

    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, PTL_EQ_NONE, PTL_PT_ANY,
                               &pt_index));
    assert(pt_index == 0);

    if (0 == rank) {
        ptl_process_t peer;
        unsigned int which;
        ptl_size_t test;

        /* setup match list entry */
        value_e.start  = NULL;
        value_e.length = 0;
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
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &value_e.ct_handle));
        CHECK_RETURNVAL(APPEND(ni_h, 0, &value_e, PTL_OVERFLOW_LIST, NULL,
                               &value_e_handle));

        /* setup md */
        md.start     = NULL;
        md.length    = 0;
        md.options   = PTL_MD_EVENT_CT_ACK;
        md.eq_handle = PTL_EQ_NONE;
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &md.ct_handle));
        CHECK_RETURNVAL(PtlMDBind(ni_h, &md, &md_handle));

        /* write to myself */
#if PHYSICAL_ADDR == 0
	peer.rank = 0;
#else
        peer = procs[0];
#endif
        CHECK_RETURNVAL(PtlPut(md_handle, 0, 0, PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(md.ct_handle, 1, &ct));

        /* Now the ct should already be incremented (give it 2
           seconds), since we already have the ack */
        test = 1;
        CHECK_RETURNVAL(PtlCTPoll(&value_e.ct_handle, &test, 1, 2 * 1000, &ct, &which));
    }

    libtest_barrier();

    /* cleanup */
    if (0 == rank) {
        CHECK_RETURNVAL(UNLINK(value_e_handle));
        CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle));
        CHECK_RETURNVAL(PtlMDRelease(md_handle));
        CHECK_RETURNVAL(PtlCTFree(md.ct_handle));
    }

    CHECK_RETURNVAL(PtlPTFree(ni_h, pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_h));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
