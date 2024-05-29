// This test confirms that when doing an ME search and a match is found and 
// USE_ONCE is ***NOT*** set, failure is incremented. It also confirms all matches 
// are found. It also confirms the matched headers were deleted.

#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <unistd.h>

#include "testing.h"

# define ENTRY_T  ptl_me_t
# define HANDLE_T ptl_handle_me_t
# define NI_TYPE  PTL_NI_MATCHING
# define AOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE)
# define SOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE)
# define SSOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_USE_ONCE)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
# define SEARCH   PtlMESearch

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
    int             num_procs, error_found;
    ptl_ct_event_t ctc;
    int rank;
    ptl_process_t *procs;
    ptl_handle_eq_t eq_h;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));

    procs = libtest_get_mapping(ni_h);

    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));
    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 8192, &eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, eq_h, PTL_PT_ANY,
                               &pt_index));
    assert(pt_index == 0);

    if (1 == rank) {
        value_e.start  = &value;
        value_e.length = sizeof(uint64_t);
        value_e.uid    = PTL_UID_ANY;
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits    = 1;
        value_e.ignore_bits = 0xffffffff; /* match on anything */
        value_e.options = AOPTIONS;
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &value_e.ct_handle));
        CHECK_RETURNVAL(APPEND(ni_h, 0, &value_e, PTL_OVERFLOW_LIST, NULL,
                               &value_e_handle));

        value = 0;
    } else if (0 == rank) {
        /* set up the landing pad so that I can read others' values */
        write_md.start     = &value;
        write_md.length    = sizeof(uint64_t);
        write_md.options   = PTL_MD_EVENT_CT_ACK;
        write_md.eq_handle = PTL_EQ_NONE;   // i.e. don't queue send events
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &write_md.ct_handle));
        CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_handle));

        value = 0xdeadbeef;
    }

    libtest_barrier();

    /* 0 writes unexpecteds to 1; match bits differ from what will be searched for.*/
    if (0 == rank) {
        /* write to rank 1 */
        ptl_process_t peer;
	      peer.rank = 1;
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 55, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 55, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 5, &ctc));
        assert(ctc.failure == 0);
    }

    libtest_barrier();

    /* 1 does searching */
    if (1 == rank) {
        int ret;
        ptl_event_t ev;
        unsigned int which;
        ptl_ct_event_t ct_event; /* for inspecting counter value */
        ct_event.success = 0;
        ct_event.failure = 0;
        PtlCTSet(value_e.ct_handle, ct_event);

        value_e.start  = &value;
        value_e.length = sizeof(uint64_t);
        value_e.uid    = PTL_UID_ANY;
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits    = 55; /* will match everything with match bits 1 */ 
        value_e.ignore_bits   = 0;
        value_e.options = SOPTIONS; /* Not USE_ONCE */

        /* do a search-delete search */
        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_DELETE, NULL));

        /* inspect counter value */
        /* counter success should be 0 and failure 1 */
        error_found = 0;
        PtlCTGet(value_e.ct_handle, &ct_event);
        fprintf(stderr, ">>>>>>>>>> TEST OUTPUT:\n");
        fprintf(stderr, "ct.success       : %d\n", ct_event.success);
        fprintf(stderr, "ct.failure       : %d\n", ct_event.failure);
        if (ct_event.success != 3) {
          fprintf(stderr, "When searching for unexpected headers with match bits = 1, expected 3 but found %d\n", ct_event.success);
          error_found = 1;
        }
        if (ct_event.failure != 1) {
          fprintf(stderr, "When searching for unexpected headers expected failure = 1 to indicate search completed, but found %d\n", ct_event.failure);
          error_found = 1;
        }

        /* Now make sure the headers were deleted */
        ct_event.success = 0;
        ct_event.failure = 0;
        PtlCTSet(value_e.ct_handle, ct_event);
        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_DELETE, NULL));

        /* inspect counter value */
        /* counter success should be 0 and failure 1 */
        PtlCTGet(value_e.ct_handle, &ct_event);
        fprintf(stderr, ">>>>>>>>>> TEST OUTPUT:\n");
        fprintf(stderr, "ct.success       : %d\n", ct_event.success);
        fprintf(stderr, "ct.failure       : %d\n", ct_event.failure);
        if (ct_event.success != 0) {
          fprintf(stderr, "When searching for unexpected headers with match bits = 1, expected 0 (because they were deleted by previous search) but found %d\n", ct_event.success);
          error_found = 1;
        }
        if (ct_event.failure != 1) {
          fprintf(stderr, "When searching for unexpected headers expected failure = 1 to indicate search completed, but found %d\n", ct_event.failure);
          error_found = 1;
        }
        if (error_found) {
          fprintf(stderr, "TEST FAILED\n");
          return 1;
        } else {
          fprintf(stderr, "TEST PASSED\n");
        }
        
        /* cleanup: do search-delete matching everything on the unexpected list to remove all entries */
        value_e.options = SOPTIONS;
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits    = 0;
        value_e.ignore_bits   = 0xffffffff;

        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_DELETE, NULL));
    }        

    libtest_barrier();

    /* more cleanup */
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
