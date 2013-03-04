#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "testing.h"

static ptl_pt_index_t logical_pt_index;
static uint64_t       value;
static ptl_process_t  myself;
static char           verb = 0;

enum {
    evnt_atomic_type,
    evnt_atomic_operation,
    evnt_ni_fail_type,
    evnt_hdr_data,
    evnt_user_ptr,
    evnt_start,
    evnt_remote_offset,
    evnt_mlength,
    evnt_rlength,
    evnt_match_bits,
    evnt_uid,
    evnt_pt_index,
    evnt_initiator,
    evnt_type
};

enum side_e {TARGET, INITIATOR};

static void validate_event(ptl_event_t *e,
                           uint16_t     fields,
                           enum side_e  side)
{
    for (int i = 14; i >= 0; i--) {
        if (fields & (1 << i)) {
            if (verb) {
                switch(i) {
                    case 0:  printf("atomic_type(%u)",
                                    (unsigned)e->atomic_type); break;
                    case 1:  printf("atomic_operation(%u)%s",
                                    (unsigned)e->atomic_operation,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 2:  printf("ni_fail_type(%u)%s",
                                    (unsigned)e->ni_fail_type,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 3:  printf("hdr_data(%u)%s",
                                    (unsigned)e->hdr_data,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 4:  printf("user_ptr(%p)%s",
                                    e->user_ptr,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 5:  printf("start(%p,%p)%s",
                                    e->start,
                                    &value,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 6:  printf("remote_offset(%u)%s",
                                    (unsigned)e->remote_offset,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 7:  printf("mlength(%u)%s",
                                    (unsigned)e->mlength,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 8:  printf("rlength(%u)%s",
                                    (unsigned)e->rlength,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 9:  printf("match_bits(%u)%s",
                                    (unsigned)e->match_bits,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 10: printf("uid(%u)%s",
                                    (unsigned)e->uid,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 11: printf("pt_index(%u)%s",
                                    (unsigned)e->pt_index,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                    case 12: printf("initiator(%u)%s",
                                    (unsigned)e->initiator.rank,
                                    (fields & ((1 << i) - 1)) ? ", " : ""); break;
                }
            }
        }
    }
    if (side == TARGET) {
        if ((fields & (1 << evnt_start)) && (fields & (1 << evnt_remote_offset))) {
            assert(((char *)e->start) - e->remote_offset == (char *)&value);
        }
        if (fields & (1 << evnt_pt_index)) {
            assert(e->pt_index == logical_pt_index);
        }
        if (fields & (1 << evnt_ni_fail_type)) {
            assert(e->ni_fail_type == PTL_NI_OK);
        }
        if ((fields & (1 << evnt_mlength)) && (fields & (1 << evnt_rlength))) {
            assert(e->mlength == e->rlength);
        }
        if ((fields & (1 << evnt_rlength)) && (fields & (1 << evnt_remote_offset))) {
            assert(e->rlength == sizeof(uint64_t) - e->remote_offset);
        }
        if ((fields & (1 << evnt_remote_offset)) && (fields & (1 << evnt_initiator))) {
            assert(e->remote_offset == e->initiator.rank % sizeof(uint64_t));
        }
        if (fields & (1 << evnt_user_ptr)) {
            assert(e->user_ptr == (void *)(0xcafecafe00UL + myself.rank));
        }
        if (fields & (1 << evnt_hdr_data)) {
            assert(e->hdr_data == 0);
        }
    } else { // INITIATOR
        if (fields & (1 << evnt_mlength)) {
            assert(e->mlength == sizeof(uint64_t) - (myself.rank % sizeof(uint64_t)));
        }
        if (fields & (1 << evnt_remote_offset)) {
            assert(e->remote_offset == myself.rank % sizeof(uint64_t));
        }
        if (fields & (1 << evnt_user_ptr)) {
            assert(e->user_ptr == (void *)(uintptr_t)(myself.rank + 1));
        }
        if (fields & (1 << evnt_ni_fail_type)) {
            assert(e->ni_fail_type == PTL_NI_OK);
        }
    }
}

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    uint64_t        readval;
    ptl_le_t        value_le;
    ptl_handle_le_t value_le_handle;
    ptl_md_t        read_md;
    ptl_handle_md_t read_md_handle;
    ptl_handle_eq_t pt_eq_handle;
    int             num_procs;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    num_procs = libtest_get_size();

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING |
                              PTL_NI_LOGICAL, PTL_PID_ANY, NULL, NULL,
                              &ni_logical));

    CHECK_RETURNVAL(PtlSetMap(ni_logical, num_procs,
                              libtest_get_mapping(ni_logical)));

    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    CHECK_RETURNVAL(PtlEQAlloc(ni_logical, 100, &pt_eq_handle));
    CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, pt_eq_handle, PTL_PT_ANY,
                               &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    value            = myself.rank + 0xdeadbeefc0d1f1edUL;
    value_le.start   = &value;
    value_le.length  = sizeof(uint64_t);
    value_le.uid     = PTL_UID_ANY;
    value_le.options = PTL_LE_OP_GET | PTL_LE_EVENT_CT_COMM;
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &value_le.ct_handle));
    CHECK_RETURNVAL(PtlLEAppend(ni_logical, 0, &value_le, PTL_PRIORITY_LIST,
                                (void *)(0xcafecafe00UL + myself.rank),
                                &value_le_handle));
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    libtest_barrier();

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
                            case PTL_EVENT_LINK:
                                printf("LINK: ");
                                break;
                        }
                    }

                    switch (event.type) {
                        case PTL_EVENT_GET:
                        case PTL_EVENT_GET_OVERFLOW:
                            assert(myself.rank == 0);
                            validate_event(&event, 0x3ff4, TARGET); break;
                        case PTL_EVENT_PUT:
                        case PTL_EVENT_PUT_OVERFLOW:
                            validate_event(&event, 0x3ffc, TARGET); break;
                        case PTL_EVENT_SEARCH:
                        case PTL_EVENT_ATOMIC:
                        case PTL_EVENT_ATOMIC_OVERFLOW:
                        case PTL_EVENT_FETCH_ATOMIC:
                        case PTL_EVENT_FETCH_ATOMIC_OVERFLOW:
                            validate_event(&event, 0x3fff, TARGET); break;
                        case PTL_EVENT_PT_DISABLED:
                            validate_event(&event, 0x2804, TARGET); break;
                        case PTL_EVENT_AUTO_UNLINK:
                        case PTL_EVENT_AUTO_FREE:
                        case PTL_EVENT_LINK:
                            validate_event(&event, 0x2814, TARGET); break;
                        case PTL_EVENT_REPLY:
                        case PTL_EVENT_ACK:
                            validate_event(&event, 0x20d4, INITIATOR); break;
                        case PTL_EVENT_SEND:
                            validate_event(&event, 0x2014, INITIATOR); break;
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
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
