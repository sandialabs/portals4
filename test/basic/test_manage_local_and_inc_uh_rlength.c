// put some unexpected puts to rank 1
// barrier
// have rank 1 append an ME that matches zero or more of them
// barrier
// clean up
#include <portals4.h>
#include <support.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include "testing.h"
#define ENTRY_T  ptl_me_t
#define HANDLE_T ptl_handle_me_t
#define NI_TYPE  PTL_NI_MATCHING
// for catching unexpected puts
#define AOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE)
//# define AOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE)
//# define SOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_CT_OVERFLOW)
#define SOPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE)
//# define SSOPTIONS (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_USE_ONCE | PTL_ME_EVENT_CT_OVERFLOW)
//# define ME_OPTIONS (PTL_ME_OP_PUT | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_MANAGE_LOCAL)
#define ME_OPTIONS (PTL_ME_OP_PUT | PTL_ME_MANAGE_LOCAL | PTL_ME_LOCAL_INC_UH_RLENGTH)
#define APPEND   PtlMEAppend
#define UNLINK   PtlMEUnlink
#define SEARCH   PtlMESearch // buffer size (in uint64_t) of ME appended by rank 1
#define ME_BUF_SIZE (200)
#define MIN_FREE 1



int main(int   argc, char *argv[])
{
    ptl_handle_ni_t  ni_h;
    ptl_pt_index_t   pt_index;
    uint64_t         value;
    ENTRY_T          value_e;
    HANDLE_T         value_e_handle;
    ptl_md_t         write_md;
    ptl_handle_md_t  write_md_handle;
    int              num_procs, error_found;
    ptl_ct_event_t   ctc;
    int              rank;
    ptl_process_t   *procs;
    ptl_handle_eq_t  eq_h;
    ptl_event_t      event;
    ENTRY_T          append_me; // the ME to be appended
    HANDLE_T         append_me_handle; // handle for the ME to be appended
    uint64_t         me_buffer[ME_BUF_SIZE]; // buffer used by the ME being appended


    for (int i = 0; i < ME_BUF_SIZE; ++i)
        me_buffer[i] = 0;

    CHECK_RETURNVAL(PtlInit());
    CHECK_RETURNVAL(libtest_init());
    rank      = libtest_get_rank();
    num_procs = libtest_get_size();
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));

    procs     = libtest_get_mapping(ni_h);

    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));

    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 8192, &eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, eq_h, PTL_PT_ANY,
                               &pt_index));
    assert(pt_index == 0);
    if (1 == rank) {
        // create ME to post on overflow list that will catch everything unexpected
        value_e.start         = &value;
        value_e.length        = sizeof(uint64_t);
        value_e.uid           = PTL_UID_ANY;
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits    = 1;
        value_e.ignore_bits   = 0xffffffff; /* match on anything */
        value_e.options       = AOPTIONS;
        value_e.ct_handle     = PTL_CT_NONE;
        CHECK_RETURNVAL(APPEND(ni_h, 0, &value_e, PTL_OVERFLOW_LIST, NULL,
                               &value_e_handle));
        value = 0;

    } else if (0 == rank) {
        // create MD to use with puts
        write_md.start     = &value;
        write_md.length    = sizeof(uint64_t);
        write_md.options   = PTL_MD_EVENT_CT_ACK;
        write_md.eq_handle = PTL_EQ_NONE; // i.e. don't queue send events
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &write_md.ct_handle));
        CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_handle));
        value              = 0xdeadbeef;
    }

    libtest_barrier();    /* 0 writes unexpecteds to 1 */

    if (0 == rank) {
        /* write to rank 1 */
        ptl_process_t peer;
        peer.rank = 1;
        /* Put three with match bits 1 and two with match bits 55 */
        /* Use the MD counter to count ACKS */
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 55, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 55, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 55, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 55, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 55, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 5, &ctc));
        assert(ctc.failure == 0);
    }

    libtest_barrier();    /* 1 creates an ME and appends it */

    if (1 == rank) {
        int ret;
        ptl_event_t ev;
        unsigned int which;
        append_me.start         = me_buffer;
        append_me.length        = ME_BUF_SIZE * sizeof(uint64_t);
        append_me.uid           = PTL_UID_ANY;
        append_me.match_id.rank = PTL_RANK_ANY;
        append_me.match_bits    = 55; // only will match unexpected headers with these match bits
        append_me.ignore_bits   = 0; // don't ignore any match bits
        append_me.options       = ME_OPTIONS;
        append_me.min_free      = MIN_FREE;
        append_me.ct_handle     = PTL_CT_NONE; // don't use a counter

        /* append */
        // this will remove those (if any) that match
        CHECK_RETURNVAL(APPEND(ni_h, 0, &append_me, PTL_PRIORITY_LIST, NULL, &append_me_handle));


        /* cleanup: do search-delete matching everything on the unexpected list to remove all entries */
        // this will generate matches for whatever unexpected headers remain on the list
        value_e.options       = SOPTIONS;
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits    = 0;
        value_e.ignore_bits   = 0xffffffff;
        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_DELETE, NULL));
    }

    libtest_barrier(); 


    if (rank == 1) {
        int ret;
        while ((ret = PtlEQGet(eq_h, &event)) == PTL_OK) {
            
            switch (event.type) {
                case PTL_EVENT_AUTO_UNLINK:
                    printf("UNLINK: \n");
                    break;
                case PTL_EVENT_GET:
                    printf("GET: ");
                    break;
                case PTL_EVENT_GET_OVERFLOW:
                    printf("GET OVERFLOW: ");
                    break;
                case PTL_EVENT_PUT:
                    printf("PUT: ");
                    break;
                case PTL_EVENT_PUT_OVERFLOW:
                    printf("PUT OVERFLOW: ");
                    break;
                case PTL_EVENT_ATOMIC:
                    printf("ATOMIC: ");
                    break;
                case PTL_EVENT_ATOMIC_OVERFLOW:
                    printf("ATOMIC OVERFLOW: ");
                    break;
                case PTL_EVENT_FETCH_ATOMIC:
                    printf("FETCHATOMIC: ");
                    break;
                case PTL_EVENT_FETCH_ATOMIC_OVERFLOW:
                    printf("FETCHATOMIC OVERFLOW: ");
                    break;
                case PTL_EVENT_REPLY:
                    printf("REPLY: ");
                    break;
                case PTL_EVENT_SEND:
                    printf("SEND: ");
                    break;
                case PTL_EVENT_ACK:
                    printf("ACK: ");
                    break;
                case PTL_EVENT_PT_DISABLED:
                    printf("PT DISABLED: ");
                    break;
                case PTL_EVENT_AUTO_FREE:
                    printf("FREE: ");
                    break;
                case PTL_EVENT_SEARCH:
                    printf("SEARCH: ");
                    break;
                case PTL_EVENT_LINK:
                    printf("LINK: ");
                    break;
            }
        }
        
    }
        
    //if (rank == 2) {
    //    /* cleanup: do search-delete matching everything on the unexpected list to remove all entries */
    //    // this will generate matches for whatever unexpected headers remain on the list
    //    value_e.options       = SOPTIONS;
    //    value_e.match_id.rank = PTL_RANK_ANY;
    //    value_e.match_bits    = 0;
    //    value_e.ignore_bits   = 0xffffffff;
    //    CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_DELETE, NULL));
    //}

    
    libtest_barrier();

    
    if (1 == rank) {
        CHECK_RETURNVAL(UNLINK(value_e_handle));
        CHECK_RETURNVAL(UNLINK(append_me_handle)); // this is required if the ME we appended made it to the priority list
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
