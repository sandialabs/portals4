#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <unistd.h>

#include "testing.h"

#if MATCHING == 1
# define ENTRY_T  ptl_me_t
# define HANDLE_T ptl_handle_me_t
# define NI_TYPE  PTL_NI_MATCHING
# define AOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE)
# define SOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
# define SEARCH   PtlMESearch
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define AOPTIONS  (PTL_LE_OP_PUT | PTL_LE_EVENT_LINK_DISABLE | PTL_LE_EVENT_COMM_DISABLE)
# define SOPTIONS  (PTL_LE_OP_PUT | PTL_LE_EVENT_LINK_DISABLE)
# define APPEND   PtlLEAppend
# define UNLINK   PtlLEUnlink
# define SEARCH   PtlLESearch
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
#if MATCHING == 1
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits    = 1;
        value_e.ignore_bits   = 0;
#endif
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

    /* 0 writes unexpecteds to 1 */
    if (0 == rank) {
        /* write to rank 1 */
        ptl_process_t peer;
	peer.rank = 1;
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 5, &ctc));
        assert(ctc.failure == 0);
    }


    libtest_barrier();

    /* 1 does searching */
    if (1 == rank) {
        int count = 0;
        int ret;
        ptl_event_t ev;
        unsigned int which;

        value_e.start  = &value;
        value_e.length = sizeof(uint64_t);
        value_e.uid    = PTL_UID_ANY;
#if MATCHING == 1
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits    = 1;
        value_e.ignore_bits   = 0;
#endif
        value_e.options = SOPTIONS;

        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_ONLY, NULL));
        while (count < 5) {
            ret = PtlEQPoll(&eq_h, 1, 1000, &ev, &which);
            if (PTL_OK == ret) {
                if (PTL_EVENT_SEARCH != ev.type) {
                    printf("1: SEARCH ONLY: Got event of type %d, expecting %d\n", ev.type, PTL_EVENT_SEARCH);
                    return 1;
                }
                count++;
            } else if (PTL_EQ_EMPTY) {
                fprintf(stderr, "1: SEARCH ONLY: unexpectedly found empty queue\n");
                return 1;
            } else {
                fprintf(stderr, "1: Unexpected return code from EQWait: %d\n", ret);
                return 1;
            }
            sleep(1);
        }

        ret = PtlEQGet(eq_h, &ev);
        if (PTL_OK == ret) {
            printf("1: SEARCH ONLY: got unexpected ok of type %d after clearing queue\n", ev.type);
            return 1;
        } else if (PTL_EQ_EMPTY) {
            ;
        } else {
            fprintf(stderr, "1: Unexpected return code from EQWait: %d\n", ret);
            return 1;
        }

        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_DELETE, NULL));
        while (count < 5) {
            ret = PtlEQPoll(&eq_h, 1, 1000, &ev, &which);
            if (PTL_OK == ret) {
                if (PTL_EVENT_PUT_OVERFLOW != ev.type) {
                    printf("1: SEARCH DELETE: Got event of type %d, expecting %d\n", ev.type, PTL_EVENT_PUT_OVERFLOW);
                    return 1;
                }
                count++;
            } else if (PTL_EQ_EMPTY) {
                fprintf(stderr, "1: SEARCH DELETE: unexpectedly found empty queue\n");
                return 1;
            } else {
                fprintf(stderr, "1: Unexpected return code from EQWait: %d\n", ret);
                return 1;
            }
        }

        ret = PtlEQGet(eq_h, &ev);
        if (PTL_OK == ret) {
            printf("1: SEARCH DELETE: got unexpected ok of type %d after clearing queue\n", ev.type);
            return 1;
        } else if (PTL_EQ_EMPTY) {
            ;
        } else {
            fprintf(stderr, "1: Unexpected return code from EQWait: %d\n", ret);
            return 1;
        }
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
