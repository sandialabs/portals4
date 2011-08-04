#include <portals4.h>
#include <portals4_runtime.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "testing.h"

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t   myself;
    ptl_pt_index_t  logical_pt_index;
    uint64_t        value, readval;
    ptl_le_t        value_le;
    ptl_handle_le_t value_le_handle;
    ptl_md_t        read_md;
    ptl_handle_md_t read_md_handle;
    ptl_handle_eq_t pt_eq_handle;
    char            verb = 0;
    int             my_rank, num_procs;

    CHECK_RETURNVAL(PtlInit());

    my_rank   = runtime_get_rank();
    num_procs = runtime_get_size();

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING |
                              PTL_NI_LOGICAL, PTL_PID_ANY, NULL, NULL,
                              &ni_logical));
    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    assert(my_rank == myself.rank);
    CHECK_RETURNVAL(PtlEQAlloc(ni_logical, 100, &pt_eq_handle));
    CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, pt_eq_handle, PTL_PT_ANY,
                               &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    value              = myself.rank + 0xdeadbeefc0d1f1edUL;
    value_le.start     = &value;
    value_le.length    = sizeof(uint64_t);
    value_le.ac_id.uid = PTL_UID_ANY;
    value_le.options   = PTL_LE_OP_GET | PTL_LE_EVENT_CT_COMM;
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &value_le.ct_handle));
    CHECK_RETURNVAL(PtlLEAppend(ni_logical, 0, &value_le, PTL_PRIORITY_LIST,
                                (void *)(0xcafecafe00UL + myself.rank),
                                &value_le_handle));
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    runtime_barrier();

    /* now I can communicate between ranks with ni_logical */

    /* set up the landing pad so that I can read others' values */
    readval           = 0;
    read_md.start     = &readval;
    read_md.length    = sizeof(uint64_t);
    read_md.options   = PTL_MD_EVENT_CT_REPLY;
    read_md.eq_handle = pt_eq_handle;
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &read_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &read_md, &read_md_handle));

    if (getenv("VERBOSE") != NULL) {
        verb = 1;
    }

    /* read rank 0's value */
    {
        ptl_ct_event_t ctc;
        ptl_process_t  r0 = { .rank = 0 };
        CHECK_RETURNVAL(PtlGet(read_md_handle, myself.rank % sizeof(uint64_t),
                               sizeof(uint64_t) -
                               (myself.rank % sizeof(uint64_t)), r0,
                               logical_pt_index, myself.rank, myself.rank %
                               sizeof(uint64_t),
                               (void *)(uintptr_t)(myself.rank + 1)));
        CHECK_RETURNVAL(PtlCTWait(read_md.ct_handle, 1, &ctc));
        assert(ctc.failure == 0);
    }
    if (myself.rank == 0) {
        NO_FAILURES(value_le.ct_handle, num_procs);
    }

    {
        int fetched = 0;
        do {
            ptl_event_t event;
            int         retval;
            fetched = 0;
            switch (retval = PtlEQGet(pt_eq_handle, &event)) {
                case PTL_OK:
                    fetched = 1;
                    if (verb == 1) {
                        printf("%i ", (int)myself.rank);
                        switch (event.type) {
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
                            case PTL_EVENT_AUTO_UNLINK:
                                printf("UNLINK: ");
                                break;
                            case PTL_EVENT_AUTO_FREE:
                                printf("FREE: ");
                                break;
                            case PTL_EVENT_SEARCH:
                                printf("SEARCH: ");
                                break;
                        }
                    }
                    switch (event.type) {
                        case PTL_EVENT_GET:
                        case PTL_EVENT_GET_OVERFLOW:
                        case PTL_EVENT_PUT:
                        case PTL_EVENT_PUT_OVERFLOW:
                        case PTL_EVENT_ATOMIC:
                        case PTL_EVENT_ATOMIC_OVERFLOW:
                        case PTL_EVENT_FETCH_ATOMIC:
                        case PTL_EVENT_FETCH_ATOMIC_OVERFLOW:
                        case PTL_EVENT_PT_DISABLED:
                        case PTL_EVENT_AUTO_UNLINK:
                        case PTL_EVENT_AUTO_FREE:
                        case PTL_EVENT_SEARCH:
                            /* target */
                            assert(myself.rank == 0);
                            if (verb) {
                                printf("match_bits(%u), rlength(%u), mlength(%u), remote_offset(%u), start(%p,%p), user_ptr(%p), hdr_data(%u), initiator(%u), uid(%u), jid(%u), ni_fail_type(%u), pt_index(%u), atomic_op(%u), atomic_type(%u)",
                                       (unsigned)event.match_bits,
                                       (unsigned)event.rlength,
                                       (unsigned)event.mlength,
                                       (unsigned)event.remote_offset,
                                       event.start, &value, event.user_ptr,
                                       (unsigned)event.hdr_data,
                                       (unsigned)event.initiator.rank,
                                       event.uid, event.jid,
                                       (unsigned)event.ni_fail_type,
                                       (unsigned)event.pt_index,
                                       (unsigned)event.atomic_operation,
                                       (unsigned)event.atomic_type);
                            }
                            assert(((char *)event.start) - event.remote_offset == (char *)&value);
                            assert(event.pt_index == logical_pt_index);
                            assert(event.ni_fail_type == PTL_NI_OK);
                            assert(event.mlength == event.rlength);
                            assert(event.rlength == sizeof(uint64_t) - event.remote_offset);
                            assert(event.remote_offset == event.initiator.rank % sizeof(uint64_t));
                            assert(event.user_ptr == (void *)(0xcafecafe00UL + myself.rank));
                            switch (event.type) {
                                default:
                                    break;
                                case PTL_EVENT_PUT:
                                case PTL_EVENT_PUT_OVERFLOW:
                                case PTL_EVENT_ATOMIC:
                                case PTL_EVENT_ATOMIC_OVERFLOW:
                                case PTL_EVENT_FETCH_ATOMIC:
                                case PTL_EVENT_FETCH_ATOMIC_OVERFLOW:
                                    assert(event.hdr_data == 0);
                            }
                            break;
                        case PTL_EVENT_REPLY:
                        case PTL_EVENT_SEND:
                        case PTL_EVENT_ACK:
                            /* initiator */
                            if (verb) {
                                printf("mlength(%u), offset(%u), user_ptr(%p), ni_fail_type(%u)",
                                       (unsigned)event.mlength,
                                       (unsigned)event.remote_offset,
                                       event.user_ptr,
                                       (unsigned)event.ni_fail_type);
                            }
                            assert(event.mlength == sizeof(uint64_t) - (myself.rank % sizeof(uint64_t)));
                            assert(event.remote_offset == myself.rank % sizeof(uint64_t));
                            assert(event.user_ptr == (void *)(uintptr_t)(myself.rank + 1));
                            assert(event.ni_fail_type == PTL_NI_OK);
                            break;
                    }
                    if (verb) {
                        printf("\n");
                    }
                    break;
                case PTL_EQ_EMPTY:
                    break;
                default:
                    CHECK_RETURNVAL(retval);
                    break;
            }
        } while (fetched == 1);
    }

    CHECK_RETURNVAL(PtlMDRelease(read_md_handle));
    CHECK_RETURNVAL(PtlCTFree(read_md.ct_handle));
    CHECK_RETURNVAL(PtlLEUnlink(value_le_handle));
    CHECK_RETURNVAL(PtlCTFree(value_le.ct_handle));

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlEQFree(pt_eq_handle));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
