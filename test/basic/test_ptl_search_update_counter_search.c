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
    uint64_t tag;
    ENTRY_T         value_e;
    HANDLE_T        value_e_handle;
    ptl_md_t        write_md;
    unsigned int send_val;
    ptl_handle_md_t write_md_handle;
    int             num_procs;
    ptl_ct_event_t ctc;
    int rank;
    ptl_process_t *procs;
    ptl_handle_eq_t eq_h;
    ptl_ct_event_t myct;
    ptl_process_t peer;
    
     
    CHECK_RETURNVAL(PtlInit());
    CHECK_RETURNVAL(libtest_init());
    rank = libtest_get_rank();
    num_procs = libtest_get_size();

    /* First NIInit() and set up portals table */ 
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));
    
    procs = libtest_get_mapping(ni_h);
    
    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));
    CHECK_RETURNVAL(PtlEQAlloc(ni_h, 8192, &eq_h));
    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, eq_h, PTL_PT_ANY,
                               &pt_index));
    assert(pt_index == 0);
    
    libtest_barrier();

    if (rank == 1) {
        value_e.start  = &value;
        value_e.length = sizeof(uint64_t);
        value_e.uid    = PTL_UID_ANY;
#if MATCHING == 1
        //value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_id.rank = 0;
        value_e.match_bits    = 1;
        value_e.ignore_bits   = 0;
#endif
        value_e.options = AOPTIONS;
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &value_e.ct_handle));
        //CHECK_RETURNVAL(PtlCTSet(value_e.ct_handle, (ptl_ct_event_t) {0, 0}));
        CHECK_RETURNVAL(APPEND(ni_h, 0, &value_e, PTL_OVERFLOW_LIST, NULL,
                               &value_e_handle));

        value = 0;
    } else if (rank == 0) {
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
                               pt_index, 2, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 2, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 3, 0, NULL, 0));
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, peer,
                               pt_index, 4, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 5, &ctc));
        //assert(ctc.failure == 0);
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
        //value_e.match_id.rank = 0;
        value_e.match_bits    = 2;
        value_e.ignore_bits   = 0;
#endif
        value_e.options = SOPTIONS;

        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_ONLY, NULL));
        PtlCTGet(value_e.ct_handle, &myct);
        printf("DKRUSE: test_ptl_search_update, no match: myct successes = %d\n", myct.success);
        printf("DKRUSE: test_ptl_search_update, no match: myct failure   = %d\n", myct.failure);
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
    //if (
    //    rank == 0) {
    //    peer.rank = 1;
    //    /* setup write_md */
    //    write_md.start     = &send_val;
    //    write_md.length    = sizeof(uint32_t);
    //    write_md.options   = PTL_MD_EVENT_CT_ACK;
    //    write_md.eq_handle = PTL_EQ_NONE;
    //    CHECK_RETURNVAL(PtlCTAlloc(ni_h, &write_md.ct_handle));
    //    CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_handle));
    //    
    //    send_val = 1;
    //    CHECK_RETURNVAL(PtlPut(write_md_handle,
    //                           0,
    //                           sizeof(uint32_t),
    //                           PTL_NO_ACK_REQ,
    //                           peer,
    //                           pt_index,
    //                           send_val, /* match bits */
    //                           0,
    //                           NULL,
    //                           0)
    //                    );
    //    send_val = 2;
    //    CHECK_RETURNVAL(PtlPut(write_md_handle,
    //                           0,
    //                           sizeof(uint32_t),
    //                           PTL_NO_ACK_REQ,
    //                           peer,
    //                           pt_index,
    //                           send_val, /* match bits */
    //                           0,
    //                           NULL,
    //                           0)
    //                    );
    //    send_val = 2;
    //    CHECK_RETURNVAL(PtlPut(write_md_handle,
    //                           0,
    //                           sizeof(uint32_t),
    //                           PTL_NO_ACK_REQ,
    //                           peer,
    //                           pt_index,
    //                           send_val, /* match bits */
    //                           0,
    //                           NULL,
    //                           0)
    //                    );
    //    send_val = 3;
    //    CHECK_RETURNVAL(PtlPut(write_md_handle,
    //                           0,
    //                           sizeof(uint32_t),
    //                           PTL_NO_ACK_REQ,
    //                           peer,
    //                           pt_index,
    //                           send_val, /* match bits */
    //                           0,
    //                           NULL,
    //                           0)
    //                    );

    //    
    //    send_val = 4;
    //    CHECK_RETURNVAL(PtlPut(write_md_handle,
    //                           0,
    //                           sizeof(uint32_t),
    //                           PTL_NO_ACK_REQ,
    //                           peer,
    //                           pt_index,
    //                           send_val, /* match bits */
    //                           0,
    //                           NULL,
    //                           0)
    //                    );
    //    
    //    
    //}
    //
    //libtest_barrier();

    //if (rank == 1) {
    //    value_e.start  = &value;
    //    value_e.length = sizeof(uint64_t);
    //    value_e.uid    = PTL_UID_ANY;
//#if //MATCHING == 1
//    //    value_e.match_id.rank = PTL_RANK_ANY;
//    //    value_e.match_bits    = 1;
//    //    value_e.ignore_bits   = 0;
//#end//if
//    //    value_e.options = AOPTIONS;
//    //    value = 0;
//    //    
//    //    CHECK_RETURNVAL(PtlCTAlloc(ni_h, &value_e.ct_handle));
//    //    CHECK_RETURNVAL(PtlCTSet(value_e.ct_handle, (ptl_ct_event_t) {0, 0}));
//    //    PtlCTGet(value_e.ct_handle, &myct);
//    //    printf("DKRUSE: after PtlCTSet: myct successes = %d\n", myct.success);
//    //    printf("DKRUSE: after PtlCTSet: myct failure   = %d\n", myct.failure);
//
//    //}
//    //
//    //libtest_barrier();
//
//    //if (rank == 1) {
//    //    
//#if //MATCHING == 1
//    //    value_e.match_id.rank = PTL_RANK_ANY;
//    //    value_e.match_bits    = 1;
//    //    value_e.ignore_bits   = 0;
//#end//if
//    //    value_e.options = SOPTIONS;
//    //    CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_ONLY, NULL));
//    //    PtlCTGet(value_e.ct_handle, &myct);
//    //    printf("DKRUSE: test_ptl_search_update, no match: myct successes = %d\n", myct.success);
//    //    printf("DKRUSE: test_ptl_search_update, no match: myct failure   = %d\n", myct.failure);
//    //}
//    //   
//    //libtest_barrier();
//    //
//    //return 0;
//
//    //if (
//    //    rank == 0) {
//    //    /* Append some data to the PTE */
//    //    for (tag = 0; tag < 4; tag++) {
//    //        value_e.start  = &tag;
//    //        value_e.length = sizeof(uint64_t);
//    //        value_e.uid    = PTL_UID_ANY;
////#if //MATCHING == 1
//    //        value_e.match_id.rank = PTL_RANK_ANY;
//    //        value_e.match_bits    = (tag == 1 || tag == 2) ? 1 : 0;
//    //        value_e.ignore_bits   = 0;
////#end//if
//    //        value_e.options = AOPTIONS;
//    //        //CHECK_RETURNVAL(APPEND(ni_h,
//    //        //                       pt_index,
//    //        //                       &value_e,
//    //        //                       PTL_PRIORITY_LIST,
//    //        //                       NULL,
//    //        //                       &value_e_handle));
//    //        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &value_e.ct_handle));
//    //        APPEND(ni_h,
//    //               pt_index,
//    //               &value_e,
//    //               PTL_UNEXPECTED_LIST,
//    //               NULL,
//    //               &value_e_handle);
//    //    }
//    //    
//    //    value = -1; 
//    //    test_e.start  = &value;
//    //    test_e.length = sizeof(uint64_t);
//    //    test_e.uid    = PTL_UID_ANY;
////#if //MATCHING == 1
//    //    test_e.match_id.rank = PTL_RANK_ANY;
//    //    test_e.match_bits    = 1; 
//    //    test_e.ignore_bits   = 0;
//    //    test_e.options = PTL_ME_USE_ONCE;
////#els//e
//    //    test_e.options = PTL_LE_USE_ONCE;
////#end//if
//    //    CHECK_RETURNVAL(PtlCTAlloc(ni_h, &test_e.ct_handle));
//    //    CHECK_RETURNVAL(SEARCH(ni_h, pt_index, &test_e, PTL_SEARCH_ONLY, NULL));
//    //    PtlCTGet(value_e.ct_handle, &myct);
//    //    printf("DKRUSE: test_ptl_search_update, no match: myct successes = %d\n", myct.success);
//    //    printf("DKRUSE: test_ptl_search_update, no match: myct failure   = %d\n", myct.failure);
//    //    
//    //    /* 
//    //       Need to check:
//    //       
//    //       CASE: PTL_LE_USE_ONCE
//    //       no matches -> 0 success, mult fails
//    //       first match  -> 1 success, 0 fails
//    //       later match -> 1 success, mult or 1 fails
//    //       DEFAULT:
//    //       no match -> 0 success, 1 fail
//    //       1 match  -> 1 success, 1 fail
//    //       mult match -> mult success, 1 fail
//    //    */
//    //}
//    //libtest_barrier();
//    //return 0;
//    
//     //CHECK_RETURNVAL(APPEND(ni_h, 0, &value_e, PTL_OVERFLOW_LIST, NULL,
//     //                       &value_e_handle));
//     //value = 0;
//        
//    /* set up the landing pad so that I can read others' values */
//    /* TODO want to set up LE/ME instead of an MD */
//    //write_md.start     = &value;
//    //write_md.length    = sizeof(uint64_t);
//    //write_md.options   = PTL_MD_EVENT_CT_ACK;
//    //write_md.eq_handle = PTL_EQ_NONE;   // i.e. don't queue send events
//    //CHECK_RETURNVAL(PtlCTAlloc(ni_h, &write_md.ct_handle));
//    //CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_handle));
//
//
//
//        
//    
//    //cases to check only for search_only
//
//    // when opt != PTL_LE_USE_ONCE
//    //     1. no match
//    //     2. 1 match
//    //     3. mult match
//
//
//
//
//    
//    // when opt == PTL_LE_USE_ONCE
//    //     1. no match, only happens when there is nothing in the LE queue
////
////if (0 == rank) {
////#if MATCHING == 1
////        value_e.match_id.rank = PTL_RANK_ANY;
////        value_e.match_bits    = 0;
////        value_e.ignore_bits   = 0;
////#endif
////        ptl_process_t peer;
////        peer.rank = 1;
////        value = 0xdeadbeef; 
////        //CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_NO_ACK_REQ, peer,
////        //                       pt_index, 1, 0, NULL, 0));
////        //CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_NO_ACK_REQ, peer,
////        //                       pt_index, 1, 0, NULL, 0));
////    }
////    libtest_barrier();
////    if (rank == 1) {
////        value_e.options |= PTL_LE_USE_ONCE;
////        value = 0;
////        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_ONLY, NULL));
////        PtlCTGet(value_e.ct_handle, &myct);
////        printf("DKRUSE: test_ptl_search_update, no match: myct successes = %d\n", myct.success);
////        printf("DKRUSE: test_ptl_search_update, no match: myct failure   = %d\n", myct.failure);
////        assert(myct.success == 0);
////        assert(myct.failure == 0);
////    }
////    libtest_barrier();
////
////
////    // match
////    if (0 == rank) {
////#if MATCHING == 1
////#endif
////        ptl_process_t peer;
////        peer.rank = 1;
////        value = 0xdeadbeef; 
////        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_NO_ACK_REQ, peer,
////                               pt_index, 1, 0, NULL, 0));
////    }
////    libtest_barrier();
////    if (rank == 1) {
////        value_e.options |= PTL_LE_USE_ONCE;
////        value = 0;
////        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_ONLY, NULL));
////        PtlCTGet(value_e.ct_handle, &myct);
////        printf("DKRUSE: test_ptl_search_update: 1 match, value = %x\n", value);
////        printf("DKRUSE: test_ptl_search_update: 1 match, myct successes = %d\n", myct.success);
////        printf("DKRUSE: test_ptl_search_update: 1 match, myct failure   = %d\n", myct.failure);
////        assert(myct.success == 1);
////        assert(myct.failure == 0);
////    }
////    libtest_barrier();
////    
////    
////    
////        //     2. 1 match first entry
////        //     2. 1 match middle entry
////        //     3. mult match first entry 
////        //     3. mult match not first entry 
////        
////
////    /* 0 writes unexpecteds to 1 */
////    if (rank == 0) {
////        /* write to rank 1 */
////        ptl_process_t peer;
////        peer.rank = 1;
////        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_NO_ACK_REQ, peer,
////                               pt_index, 1, 0, NULL, 0));
////        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_NO_ACK_REQ, peer,
////                               pt_index, 1, 0, NULL, 0));
////        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_NO_ACK_REQ, peer,
////                               pt_index, 1, 0, NULL, 0));
////        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_NO_ACK_REQ, peer,
////                               pt_index, 1, 0, NULL, 0));
////        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_NO_ACK_REQ, peer,
////                               pt_index, 1, 0, NULL, 0));
////        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 5, &ctc));
////        assert(ctc.failure == 0);
////    }
////
////
////    libtest_barrier();
////
////    /* 1 does searching */
////    if (1 == rank) {
////        int count = 0;
////        int ret;
////        ptl_event_t ev;
////        unsigned int which;
////
////        value_e.start  = &value;
////        value_e.length = sizeof(uint64_t);
////        value_e.uid    = PTL_UID_ANY;
////#if MATCHING == 1
////        value_e.match_id.rank = PTL_RANK_ANY;
////        value_e.match_bits    = 1;
////        value_e.ignore_bits   = 0;
////#endif
////        value_e.options = SOPTIONS;
////
////
////        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_ONLY, NULL));
////        //PtlCTGet(value_e.ct_handle, &myct);
////        //printf("DKRUSE myct successes = %d\n", myct.success);
////        //printf("DKRUSE myct failure   = %d\n", myct.failure);
////        while (count < 5) {
////            ret = PtlEQPoll(&eq_h, 1, 1000, &ev, &which);
////            if (PTL_OK == ret) {
////                if (PTL_EVENT_SEARCH != ev.type) {
////                    printf("1: SEARCH ONLY: Got event of type %d, expecting %d\n", ev.type, PTL_EVENT_SEARCH);
////                    return 1;
////                }
////                count++;
////            } else if (PTL_EQ_EMPTY) {
////                fprintf(stderr, "1: SEARCH ONLY: unexpectedly found empty queue\n");
////                return 1;
////            } else {
////                fprintf(stderr, "1: Unexpected return code from EQWait: %d\n", ret);
////                return 1;
////            }
////            sleep(1);
////        }
////        
////            
////        
////
////        ret = PtlEQGet(eq_h, &ev);
////        if (PTL_OK == ret) {
////            printf("1: SEARCH ONLY: got unexpected ok of type %d after clearing queue\n", ev.type);
////            return 1;
////        } else if (PTL_EQ_EMPTY) {
////            ;
////        } else {
////            fprintf(stderr, "1: Unexpected return code from EQWait: %d\n", ret);
////            return 1;
////        }
////
////        CHECK_RETURNVAL(SEARCH(ni_h, 0, &value_e, PTL_SEARCH_DELETE, NULL));
////        //PtlCTGet(value_e.ct_handle, &myct);
////        //printf("DKRUSE myct successes = %d\n", myct.success);
////        //printf("DKRUSE myct failure   = %d\n", myct.failure);
////        count = 0;
////        while (count < 5) {
////            ret = PtlEQPoll(&eq_h, 1, 1000, &ev, &which);
////            if (PTL_OK == ret) {
////                if (PTL_EVENT_PUT_OVERFLOW != ev.type) {
////                    printf("1: SEARCH DELETE: Got event of type %d, expecting %d\n", ev.type, PTL_EVENT_PUT_OVERFLOW);
////                    return 1;
////                }
////                count++;
////            } else if (PTL_EQ_EMPTY) {
////                fprintf(stderr, "1: SEARCH DELETE: unexpectedly found empty queue\n");
////                return 1;
////            } else {
////                fprintf(stderr, "1: Unexpected return code from EQWait: %d\n", ret);
////                return 1;
////            }
////        }
////
////        ret = PtlEQGet(eq_h, &ev);
////        if (PTL_OK == ret) {
////            printf("1: SEARCH DELETE: got unexpected ok of type %d after clearing queue\n", ev.type);
////            return 1;
////        } else if (PTL_EQ_EMPTY) {
////            ;
////        } else {
////            fprintf(stderr, "1: Unexpected return code from EQWait: %d\n", ret);
////            return 1;
////        }
////    }        
////
////    libtest_barrier();
//
////    /* cleanup */
////    if (1 == rank) {
////        CHECK_RETURNVAL(UNLINK(value_e_handle));
////        CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle));
////    } else if (0 == rank) {
////        CHECK_RETURNVAL(PtlMDRelease(write_md_handle));
////        CHECK_RETURNVAL(PtlCTFree(write_md.ct_handle));
////    }
////
////    CHECK_RETURNVAL(PtlPTFree(ni_h, pt_index));
////    CHECK_RETURNVAL(PtlNIFini(ni_h));
////    CHECK_RETURNVAL(libtest_fini());
////    PtlFini();
////
////    return 0;
////}
//
///* vim:set expandtab: */
