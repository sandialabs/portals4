#include <portals4.h>
#include <portals4_runtime.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>

#include "testing.h"

#if INTERFACE == 1
#define ENTRY_T ptl_me_t
#define HANDLE_T ptl_handle_me_t
#define NI_TYPE PTL_NI_MATCHING
#define OPTIONS (PTL_ME_OP_PUT | PTL_ME_EVENT_CT_COMM)
#define APPEND PtlMEAppend
#define UNLINK PtlMEUnlink
#else
#define ENTRY_T ptl_le_t
#define HANDLE_T ptl_handle_le_t
#define NI_TYPE PTL_NI_NO_MATCHING
#define OPTIONS (PTL_LE_OP_PUT | PTL_LE_EVENT_CT_COMM)
#define APPEND PtlLEAppend
#define UNLINK PtlLEUnlink
#endif

#define BUFSIZE 4096

int main(
    int argc,
    char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t myself;
    ptl_pt_index_t logical_pt_index;
    ptl_process_t *amapping;
    unsigned char *value, *readval;
    ENTRY_T value_e;
    HANDLE_T value_e_handle;
    ptl_md_t write_md;
    ptl_handle_md_t write_md_handle;
    int my_rank, num_procs;
    ptl_handle_eq_t eq_handle;
    int verb = 0;

    if (getenv("VERBOSE")) {
        verb = 1;
    }
    CHECK_RETURNVAL(PtlInit());

    my_rank = runtime_get_rank();
    num_procs = runtime_get_size();

    amapping = malloc(sizeof(ptl_process_t) * num_procs);
    value = malloc(sizeof(unsigned char) * BUFSIZE);
    readval = malloc(sizeof(unsigned char) * BUFSIZE);

    assert(amapping);
    assert(value);
    assert(readval);

    CHECK_RETURNVAL(PtlNIInit
                    (PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL, PTL_PID_ANY,
                     NULL, NULL, num_procs, NULL, amapping, &ni_logical));
    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    assert(my_rank == myself.rank);
    CHECK_RETURNVAL(PtlEQAlloc(ni_logical, 100, &eq_handle));
    CHECK_RETURNVAL(PtlPTAlloc
                    (ni_logical, 0, eq_handle, PTL_PT_ANY,
                     &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    memset(value, 42, BUFSIZE);
    value_e.start = value;
    value_e.length = BUFSIZE/2;
    value_e.ac_id.uid = PTL_UID_ANY;
#if INTERFACE == 1
    value_e.match_id.rank = PTL_RANK_ANY;
    value_e.match_bits = 1;
    value_e.ignore_bits = 0;
#endif
    value_e.options = OPTIONS;
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &value_e.ct_handle));
    CHECK_RETURNVAL(APPEND
                    (ni_logical, 0, &value_e, PTL_PRIORITY_LIST,
                     (void *)(0xcafecafe00UL + myself.rank),
                     &value_e_handle));
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    runtime_barrier();
    /* don't need this anymore, so free up resources */
    free(amapping);

    /* now I can communicate between ranks with ni_logical */

    /* set up the landing pad so that I can read others' values */
    memset(readval, 61, BUFSIZE);
    write_md.start = readval;
    write_md.length = BUFSIZE;
    write_md.options = PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_CT_BYTES;
    write_md.eq_handle = eq_handle;   // i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &write_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &write_md, &write_md_handle));

    /* set rank 0's value */
    {
        ptl_ct_event_t ctc;
        ptl_process_t r0 = {.rank = 0 };
        CHECK_RETURNVAL(PtlPut
                        (write_md_handle, 0, BUFSIZE, PTL_CT_ACK_REQ, r0,
                         logical_pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 1, &ctc));
        assert(ctc.failure == 0);
        assert(ctc.success == BUFSIZE/2);
    }
    if (myself.rank == 0) {
        NO_FAILURES(value_e.ct_handle, num_procs);
        for (unsigned idx=0; idx<BUFSIZE/2; ++idx) {
            if (value[idx] != 61) {
                fprintf(stderr, "bad value at idx %u (readval[%u] = %i, should be 61)\n", idx, idx, value[idx]);
                abort();
            }
        }
        for (unsigned idx=BUFSIZE/2; idx < BUFSIZE; ++idx) {
            if (value[idx] != 42) {
                fprintf(stderr, "bad value at idx %u (readval[%u] = %i, should be 42)\n", idx, idx, value[idx]);
                abort();
            }
        }
    }
    {
        int fetched = 0;
        size_t events = 0;
        do {
            ptl_event_t event;
            int retval;
            fetched = 0;
            switch (retval = PtlEQGet(eq_handle, &event)) {
                case PTL_OK:
                    fetched = 1;
                    events++;
                    if (verb) {
                        printf("%i ", (int)myself.rank);
                        switch (event.type) {
                            case PTL_EVENT_GET:
                                printf("GET: ");
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
                            case PTL_EVENT_REPLY:
                                printf("REPLY: ");
                                break;
                            case PTL_EVENT_SEND:
                                printf("SEND: ");
                                break;
                            case PTL_EVENT_ACK:
                                printf("ACK: ");
                                break;
                            case PTL_EVENT_DROPPED:
                                printf("DROPPED: ");
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
                            case PTL_EVENT_PROBE:
                                printf("PROBE: ");
                                break;
                        }
                    }
                    switch (event.type) {
                        case PTL_EVENT_GET:
                        case PTL_EVENT_PUT:
                        case PTL_EVENT_PUT_OVERFLOW:
                        case PTL_EVENT_ATOMIC:
                        case PTL_EVENT_ATOMIC_OVERFLOW:
                        case PTL_EVENT_DROPPED:
                        case PTL_EVENT_PT_DISABLED:
                        case PTL_EVENT_AUTO_UNLINK:
                        case PTL_EVENT_AUTO_FREE:
                        case PTL_EVENT_PROBE:
                            /* target */
                            assert(myself.rank == 0);
                            if (verb) {
                                printf("match_bits(%u), ", (unsigned)event.match_bits);
                                printf("%u",(unsigned)event.rlength);
                                printf("%u",(unsigned)event.mlength);
                                printf("%u",(unsigned)event.remote_offset);
                                printf("%p",event.start);
                                printf("%p",event.user_ptr);
                                printf("%u",(unsigned)event.hdr_data);
                                printf("%u",(unsigned)event.initiator.rank);
                                printf("%u",event.uid);
                                printf("%u",event.jid);
                                printf("%u",(unsigned)event.ni_fail_type);
                                printf("%u",(unsigned)event.pt_index);
                                printf("%u",(unsigned)event.atomic_operation);
                                printf("%u",(unsigned)event.atomic_type);
                                /*printf
                                    ("match_bits(%u), rlength(%u), mlength(%u), remote_offset(%u), start(%p,%p), user_ptr(%p), hdr_data(%u), initiator(%u), uid(%u), jid(%u), ni_fail_type(%u), pt_index(%u), atomic_op(%u), atomic_type(%u)",
                                     (unsigned)event.match_bits,
                                     (unsigned)event.rlength,
                                     (unsigned)event.mlength,
                                     (unsigned)event.remote_offset,
                                     event.start, value, event.user_ptr,
                                     (unsigned)event.hdr_data,
                                     (unsigned)event.initiator.rank,
                                     event.uid, event.jid,
                                     (unsigned)event.ni_fail_type,
                                     (unsigned)event.pt_index,
                                     (unsigned)event.atomic_operation,
                                     (unsigned)event.atomic_type);*/
                            }
                            assert(event.match_bits == INTERFACE);
                            assert(((char *)event.start) -
                                   event.remote_offset == (char *)value);
                            assert(event.pt_index == logical_pt_index);
                            assert(event.ni_fail_type == PTL_NI_OK);
                            assert(event.mlength == event.rlength);
                            assert(event.rlength == BUFSIZE/2);
                            assert(event.remote_offset ==
                                   event.initiator.rank % sizeof(uint64_t));
                            assert(event.user_ptr ==
                                   (void *)(0xcafecafe00UL + myself.rank));
                            assert(event.hdr_data == 0);
                            break;
                        case PTL_EVENT_REPLY:
                        case PTL_EVENT_SEND:
                        case PTL_EVENT_ACK:
                            /* initiator */
                            if (verb) {
                                printf
                                    ("mlength(%u), offset(%u), user_ptr(%p), ni_fail_type(%u)",
                                     (unsigned)event.mlength,
                                     (unsigned)event.remote_offset,
                                     event.user_ptr,
                                     (unsigned)event.ni_fail_type);
                            }
                            assert(event.mlength == BUFSIZE/2);
                            assert(event.remote_offset == 0);
                            assert(event.user_ptr == NULL);
                            assert(event.ni_fail_type == PTL_NI_OK);
                            break;
                    }
                    if (verb) {
                        printf("\n");
                    }
                    break;
                case PTL_EQ_EMPTY:
                    assert(events > 0);
                    break;
                default:
                    abort();
            }
        } while (fetched == 1);
    }
    CHECK_RETURNVAL(PtlMDRelease(write_md_handle));
    CHECK_RETURNVAL(PtlCTFree(write_md.ct_handle));
    CHECK_RETURNVAL(UNLINK(value_e_handle));
    CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle));

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlEQFree(eq_handle));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    PtlFini();

    return 0;
}
/* vim:set expandtab: */
