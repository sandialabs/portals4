#include <portals4.h>
#include <support/support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>

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

#define BUFSIZE 4096
static int verb = 0;

static size_t emptyEQ(ptl_handle_eq_t eq_handle,
                      ptl_process_t   myself,
                      size_t          expected)
{
    size_t events = 0;

    do {
        ptl_event_t event;
        int         retval;
        if (events == expected) {
            retval = PtlEQGet(eq_handle, &event);
        } else {
            retval = PtlEQWait(eq_handle, &event);
        }
        switch (retval) {
            case PTL_OK:
                events++;
                if (verb) {
                    printf("\t%i ", (int)myself.rank);
                    switch (event.type) {
                        case PTL_EVENT_GET:                   printf("GET: "); break;
                        case PTL_EVENT_GET_OVERFLOW:          printf("GET-OVERFLOW: "); break;
                        case PTL_EVENT_PUT:                   printf("PUT: "); break;
                        case PTL_EVENT_PUT_OVERFLOW:          printf("PUT-OVERFLOW: "); break;
                        case PTL_EVENT_ATOMIC:                printf("ATOMIC: "); break;
                        case PTL_EVENT_ATOMIC_OVERFLOW:       printf("ATOMIC-OVERFLOW: "); break;
                        case PTL_EVENT_FETCH_ATOMIC:          printf("FETCHATOMIC: "); break;
                        case PTL_EVENT_FETCH_ATOMIC_OVERFLOW: printf("FETCHATOMIC-OVERFLOW: "); break;
                        case PTL_EVENT_REPLY:                 printf("REPLY: "); break;
                        case PTL_EVENT_SEND:                  printf("SEND: "); break;
                        case PTL_EVENT_ACK:                   printf("ACK: "); break;
                        case PTL_EVENT_PT_DISABLED:           printf("PT-DISABLED: "); break;
                        case PTL_EVENT_AUTO_UNLINK:           printf("UNLINK: "); break;
                        case PTL_EVENT_AUTO_FREE:             printf("FREE: "); break;
                        case PTL_EVENT_SEARCH:                printf("SEARCH: "); break;
                        case PTL_EVENT_LINK:                  printf("LINK: "); break;
                    }
                }
                switch (event.type) {
                    case PTL_EVENT_SEARCH:
                    case PTL_EVENT_GET:
                    case PTL_EVENT_GET_OVERFLOW:
                    case PTL_EVENT_PUT:
                    case PTL_EVENT_PUT_OVERFLOW:
                        /* target */
                        if (verb) {
                            printf("initiator.rank(%u), ", (unsigned)event.initiator.rank);
                            printf("pt_index(%u), ", (unsigned)event.pt_index);
                            printf("uid(%u), ", event.uid);
                            printf("match_bits(%u), ", (unsigned)event.match_bits);
                            printf("rlength(%u), ", (unsigned)event.rlength);
                            printf("mlength(%u), ", (unsigned)event.mlength);
                            printf("remote_offset(%u), ", (unsigned)event.remote_offset);
                            printf("start(%p), ", event.start);
                            printf("user_ptr(%p), ", event.user_ptr);
                            printf("hdr_data(%u), ", (unsigned)event.hdr_data);
                            printf("ni_fail_type(%u)", (unsigned)event.ni_fail_type);
                        }
                        break;
                    case PTL_EVENT_ATOMIC:
                    case PTL_EVENT_ATOMIC_OVERFLOW:
                    case PTL_EVENT_FETCH_ATOMIC:
                    case PTL_EVENT_FETCH_ATOMIC_OVERFLOW:
                        /* target */
                        if (verb) {
                            printf("initiator.rank(%u), ", (unsigned)event.initiator.rank);
                            printf("pt_index(%u), ", (unsigned)event.pt_index);
                            printf("uid(%u), ", event.uid);
                            printf("match_bits(%u), ", (unsigned)event.match_bits);
                            printf("rlength(%u), ", (unsigned)event.rlength);
                            printf("mlength(%u), ", (unsigned)event.mlength);
                            printf("remote_offset(%u), ", (unsigned)event.remote_offset);
                            printf("start(%p), ", event.start);
                            printf("user_ptr(%p), ", event.user_ptr);
                            printf("hdr_data(%u), ", (unsigned)event.hdr_data);
                            printf("ni_fail_type(%u), ", (unsigned)event.ni_fail_type);
                            printf("atomic_operation(%u), ", (unsigned)event.atomic_operation);
                            printf("atomic_type(%u)", (unsigned)event.atomic_type);
                        }
                        break;
                    case PTL_EVENT_PT_DISABLED:
                        /* target */
                        if (verb) {
                            printf("pt_index(%u), ", (unsigned)event.pt_index);
                            printf("ni_fail_type(%u), ", (unsigned)event.ni_fail_type);
                        }
                        break;
                    case PTL_EVENT_LINK:
                    case PTL_EVENT_AUTO_UNLINK:
                    case PTL_EVENT_AUTO_FREE:
                        /* target */
                        if (verb) {
                            printf("pt_index(%u), ", (unsigned)event.pt_index);
                            printf("user_ptr(%p), ", event.user_ptr);
                            printf("ni_fail_type(%u) ", (unsigned)event.ni_fail_type);
                        }
                        break;
                    case PTL_EVENT_REPLY:
                    case PTL_EVENT_ACK:
                        /* initiator */
                        if (verb) {
                            printf("mlength(%u), remote_offset(%u), user_ptr(%p), ni_fail_type(%u)",
                                   (unsigned)event.mlength,
                                   (unsigned)event.remote_offset,
                                   event.user_ptr,
                                   (unsigned)event.ni_fail_type);
                        }
                        break;
                    case PTL_EVENT_SEND:
                        /* initiator */
                        if (verb) {
                            printf("user_ptr(%p), ni_fail_type(%u)",
                                   event.user_ptr,
                                   (unsigned)event.ni_fail_type);
                        }
                        break;
                }
                if (verb) {
                    printf("\n");
                }
                break;
            case PTL_EQ_EMPTY:
                break;
            default:
                abort();
        }
    } while (events < expected);
    return events;
}

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t   myself;
    ptl_pt_index_t  logical_pt_index;
    unsigned char  *unexpected_buf;
    uint64_t        sendval, recvval;
    ENTRY_T         unexpected_e, recv_e;
    HANDLE_T        unexpected_e_handle, recv_e_handle;
    ptl_md_t        write_md;
    ptl_handle_md_t write_md_handle;
    int             num_procs;
    ptl_handle_eq_t recv_eq;

    if (getenv("VERBOSE")) {
        verb = 1;
    }
    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    num_procs = libtest_get_size();

    unexpected_buf = malloc(sizeof(unsigned char) * BUFSIZE);

    assert(unexpected_buf);

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_logical));

    CHECK_RETURNVAL(PtlSetMap(ni_logical, num_procs,
                              libtest_get_mapping()));

    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    CHECK_RETURNVAL(PtlEQAlloc(ni_logical, 100, &recv_eq));
    CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, recv_eq, PTL_PT_ANY,
                               &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    memset(unexpected_buf, 42, BUFSIZE);
    unexpected_e.start  = unexpected_buf;
    unexpected_e.length = BUFSIZE;
    unexpected_e.uid    = PTL_UID_ANY;
#if INTERFACE == 1
    unexpected_e.match_id.rank = PTL_RANK_ANY;
    unexpected_e.match_bits    = 0;
    memset(&unexpected_e.ignore_bits, 0xff, sizeof(ptl_match_bits_t));
    unexpected_e.min_free = BUFSIZE - sizeof(sendval) + 1;
    unexpected_e.options  = OPTIONS | PTL_ME_MANAGE_LOCAL;
#else
    unexpected_e.options = OPTIONS;
#endif
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &unexpected_e.ct_handle));
    CHECK_RETURNVAL(APPEND(ni_logical, logical_pt_index, &unexpected_e,
                           PTL_OVERFLOW_LIST, (void *)2, &unexpected_e_handle));
    /* expecting one LINK event */
    if (verb) {
        printf("Posting the unexpected buffer to the overflow list:\n    Target-side EQ:\n");
    }
    emptyEQ(recv_eq, myself, 1);

    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    libtest_barrier();

    /* now I can communicate between ranks with ni_logical */

    /* set up the landing pad so that I can read others' values */
    sendval          = 61;
    write_md.start   = &sendval;
    write_md.length  = sizeof(sendval);
    write_md.options = PTL_MD_EVENT_CT_SEND;
    CHECK_RETURNVAL(PtlEQAlloc(ni_logical, 100, &write_md.eq_handle));
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &write_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &write_md, &write_md_handle));

    /* set rank 0's value */
    {
        ptl_ct_event_t ctc;
        CHECK_RETURNVAL(PtlPut(write_md_handle, 0, write_md.length,
                               PTL_CT_ACK_REQ, myself, logical_pt_index, 1, 0,
                               NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 1, &ctc));
        assert(ctc.failure == 0);
        assert(ctc.success == 1);
    }
    fflush(NULL);
    if (verb) {
        printf("-=-=-=-=-=-=-=-=-=-=-\nSending the unexpected put...\n    Initiator-side EQ:\n");
    }
    emptyEQ(write_md.eq_handle, myself, 1);
    assert(emptyEQ(write_md.eq_handle, myself, 0) == 0);
    if (verb) {
        printf("    Target-side EQ:\n");
    }
    emptyEQ(recv_eq, myself, 2);
    assert(emptyEQ(recv_eq, myself, 0) == 0);
    if (verb) {
        printf("-=-=-=-=-=-=-=-=-=-=-\nNow... posting the receive:\n");
    }
    fflush(NULL);
    recvval       = 0;
    recv_e.start  = &recvval;
    recv_e.length = sizeof(recvval);
    recv_e.uid    = PTL_UID_ANY;
#if INTERFACE == 1
    recv_e.match_id.rank = PTL_RANK_ANY;
    recv_e.match_bits    = 1;
    recv_e.ignore_bits   = 0;
    recv_e.options       = OPTIONS | PTL_ME_USE_ONCE;
#else
    recv_e.options = OPTIONS | PTL_LE_USE_ONCE;
#endif
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &recv_e.ct_handle));
    CHECK_RETURNVAL(APPEND(ni_logical, logical_pt_index, &recv_e,
                           PTL_PRIORITY_LIST, (void *)1, &recv_e_handle));
    {
        size_t count_events;
        if (verb) {
            printf("    Initiator-side EQ:\n");
        }
        count_events = emptyEQ(write_md.eq_handle, myself, 0);
        if (count_events != 0) {
            printf("%zu events!\n", count_events);
            abort();
        }
        if (verb) {
            printf("    Target-side EQ:\n");
        }
        count_events = emptyEQ(recv_eq, myself, 3);
        if (count_events != 3) {
            printf("%zu events!\n", count_events);
            abort();
        }
        assert(emptyEQ(recv_eq, myself, 0) == 0);
    }

    CHECK_RETURNVAL(PtlMDRelease(write_md_handle));
    CHECK_RETURNVAL(PtlCTFree(recv_e.ct_handle));
    CHECK_RETURNVAL(PtlCTFree(write_md.ct_handle));
    CHECK_RETURNVAL(PtlEQFree(write_md.eq_handle));
    // CHECK_RETURNVAL(UNLINK(unexpected_e_handle));
    CHECK_RETURNVAL(PtlCTFree(unexpected_e.ct_handle));

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlEQFree(recv_eq));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();
    free(unexpected_buf);

    return 0;
}

/* vim:set expandtab: */
