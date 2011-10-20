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

static ptl_pt_index_t logical_pt_index;
static unsigned char *value;
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
#if INTERFACE == 1
        if (fields & (1 << evnt_match_bits)) {
            assert(e->match_bits == INTERFACE);
        }
#endif
        if ((fields & (1 << evnt_start)) && (fields & (1 << evnt_remote_offset))) {
            assert(((char *)e->start) - e->remote_offset == (char *)value);
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
            assert(e->rlength == BUFSIZE / 2);
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
            assert(e->mlength == BUFSIZE / 2);
        }
        if (fields & (1 << evnt_remote_offset)) {
            assert(e->remote_offset == 0);
        }
        if (fields & (1 << evnt_user_ptr)) {
            assert(e->user_ptr == NULL);
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
    ptl_process_t   myself;
    ptl_pt_index_t  logical_pt_index;
    unsigned char  *readval;
    ENTRY_T         value_e;
    HANDLE_T        value_e_handle;
    ptl_md_t        write_md;
    ptl_handle_md_t write_md_handle;
    int             my_rank, num_procs;
    ptl_handle_eq_t eq_handle;
    int             verb = 0;
    int             my_ret;

    if (getenv("VERBOSE")) {
        verb = 1;
    }
    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    my_rank   = libtest_get_rank();
    num_procs = libtest_get_size();

    value   = malloc(sizeof(unsigned char) * BUFSIZE);
    readval = malloc(sizeof(unsigned char) * BUFSIZE);

    assert(value);
    assert(readval);

    CHECK_RETURNVAL(PtlNIInit
                        (PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                        PTL_PID_ANY,
                        NULL, NULL, &ni_logical));

    my_ret = PtlGetMap(ni_logical, 0, NULL, NULL);
    if (my_ret == PTL_NO_SPACE) {
        ptl_process_t *amapping;
        amapping = libtest_get_mapping();
        CHECK_RETURNVAL(PtlSetMap(ni_logical, num_procs, amapping));
        free(amapping);
    } else {
        CHECK_RETURNVAL(my_ret);
    }

    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    assert(my_rank == myself.rank);
    CHECK_RETURNVAL(PtlEQAlloc(ni_logical, 100, &eq_handle));
    CHECK_RETURNVAL(PtlPTAlloc
                        (ni_logical, 0, eq_handle, PTL_PT_ANY,
                        &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    memset(value, 42, BUFSIZE);
    value_e.start  = value;
    value_e.length = BUFSIZE / 2;
    value_e.uid    = PTL_UID_ANY;
#if INTERFACE == 1
    value_e.match_id.rank = PTL_RANK_ANY;
    value_e.match_bits    = 1;
    value_e.ignore_bits   = 0;
#endif
    value_e.options = OPTIONS;
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &value_e.ct_handle));
    CHECK_RETURNVAL(APPEND
                        (ni_logical, 0, &value_e, PTL_PRIORITY_LIST,
                        (void *)(0xcafecafe00UL + myself.rank),
                        &value_e_handle));
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    libtest_barrier();

    /* now I can communicate between ranks with ni_logical */

    /* set up the landing pad so that I can read others' values */
    memset(readval, 61, BUFSIZE);
    write_md.start     = readval;
    write_md.length    = BUFSIZE;
    write_md.options   = PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_CT_BYTES;
    write_md.eq_handle = eq_handle;   // i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &write_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &write_md, &write_md_handle));

    /* set rank 0's value */
    {
        ptl_ct_event_t ctc;
        ptl_process_t  r0 = { .rank = 0 };
        CHECK_RETURNVAL(PtlPut
                            (write_md_handle, 0, BUFSIZE, PTL_CT_ACK_REQ, r0,
                            logical_pt_index, 1, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 1, &ctc));
        assert(ctc.failure == 0);
        assert(ctc.success == BUFSIZE / 2);
    }
    if (myself.rank == 0) {
        NO_FAILURES(value_e.ct_handle, num_procs);
        for (unsigned idx = 0; idx < BUFSIZE / 2; ++idx) {
            if (value[idx] != 61) {
                fprintf(
                        stderr,
                        "bad value at idx %u (readval[%u] = %i, should be 61)\n",
                        idx,
                        idx, value[idx]);
                abort();
            }
        }
        for (unsigned idx = BUFSIZE / 2; idx < BUFSIZE; ++idx) {
            if (value[idx] != 42) {
                fprintf(
                        stderr,
                        "bad value at idx %u (readval[%u] = %i, should be 42)\n",
                        idx,
                        idx, value[idx]);
                abort();
            }
        }
    }
    {
        int    fetched = 0;
        size_t events  = 0;
        do {
            ptl_event_t event;
            int         retval;
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
                    assert(myself.rank == 0);
                    switch (event.type) {
                        case PTL_EVENT_GET:
                        case PTL_EVENT_GET_OVERFLOW:
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
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
