#include <portals4.h>
#include <support/support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "testing.h"

#if INTERFACE == 1
# define ENTRY_T        ptl_me_t
# define HANDLE_T       ptl_handle_me_t
# define NI_TYPE        PTL_NI_MATCHING
# define OPTIONS        (PTL_ME_OP_PUT | PTL_ME_OP_GET | PTL_ME_EVENT_CT_COMM)
# define APPEND         PtlMEAppend
# define UNLINK         PtlMEUnlink
#else
# define ENTRY_T        ptl_le_t
# define HANDLE_T       ptl_handle_le_t
# define NI_TYPE        PTL_NI_NO_MATCHING
# define OPTIONS        (PTL_LE_OP_PUT | PTL_LE_OP_GET | PTL_LE_EVENT_CT_COMM)
# define APPEND         PtlLEAppend
# define UNLINK         PtlLEUnlink
#endif /* if INTERFACE == 1 */

#define BUFSIZE 10000

int main(
         int argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t myself;
    ptl_pt_index_t logical_pt_index;
    ptl_process_t *amapping;
    uint64_t *value, *readval, tmp;
    ENTRY_T value_e;
    HANDLE_T value_e_handle;
    ptl_md_t read_md;
    ptl_handle_md_t read_md_handle;
    int my_rank, num_procs;
    ptl_ni_limits_t actual = {0};
    ptl_ni_limits_t desired = {
                                //.max_fetch_atomic_size = UINT32_MAX,
                                .max_fetch_atomic_size = BUFSIZE * sizeof(*value),
                              };

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    my_rank = libtest_get_rank();
    num_procs = libtest_get_size();

    amapping = malloc(sizeof(ptl_process_t) * num_procs);
    value = malloc(sizeof(*value) * BUFSIZE);
    readval = malloc(sizeof(*readval) * BUFSIZE);

    assert(amapping);
    assert(value);
    assert(readval);

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, &desired, &actual,
                              &ni_logical));

    assert(actual.max_atomic_size >= BUFSIZE * sizeof(*value));

    CHECK_RETURNVAL(PtlSetMap(ni_logical, num_procs,
                              libtest_get_mapping()));

    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    assert(my_rank == myself.rank);
    CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY,
                               &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    for (unsigned idx=0; idx<BUFSIZE; idx++) {
        value[idx] = myself.rank + 0xdeadbeefc0d1f1ed;
    }
    if (myself.rank == 0) {
        value_e.start = value;
        value_e.length = BUFSIZE * sizeof(*value);
        value_e.uid = PTL_UID_ANY;
        value_e.options = OPTIONS;
#if INTERFACE == 1
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits = 1;
        value_e.ignore_bits = 0;
#endif
        CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &value_e.ct_handle));
        CHECK_RETURNVAL(APPEND(ni_logical, 0, &value_e, PTL_PRIORITY_LIST,
                               NULL, &value_e_handle));
    }
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    libtest_barrier();
    /* don't need this anymore, so free up resources */
    free(amapping);

    /* now I can communicate between ranks with ni_logical */

    /* set up the landing pad so that I can read others' values */
    for (unsigned idx=0; idx<BUFSIZE; idx++) {
        readval[idx] = 1;
    }
    read_md.start = readval;
    read_md.length = BUFSIZE * sizeof(*readval);
    read_md.options = PTL_MD_EVENT_CT_REPLY;
    read_md.eq_handle = PTL_EQ_NONE;   // i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &read_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &read_md, &read_md_handle));

    /* twiddle rank 0's value */
    {
        ptl_ct_event_t ctc;
        ptl_process_t r0 = {.rank = 0 };
        CHECK_RETURNVAL(PtlFetchAtomic(read_md_handle, 0, read_md_handle, 0,
                                       BUFSIZE*sizeof(*readval), r0,
                                       logical_pt_index, 1, 0, NULL, 0,
                                       PTL_SUM, PTL_UINT64_T));
        CHECK_RETURNVAL(PtlCTWait(read_md.ct_handle, 1, &ctc));
        assert(ctc.failure == 0);
    }
    /*printf("%i readval: %llx\n", (int)myself.rank,
     *     (unsigned long long)readval);*/
    //assert(
    //       readval >= 0xdeadbeefc0d1f1ed && readval < 0xdeadbeefc0d1f1ed +
    //       num_procs);
    tmp = readval[0];
    for (unsigned idx=0; idx<BUFSIZE; idx++) {
        if (readval[idx] < 0xdeadbeefc0d1f1ed ||
            readval[idx] >= (0xdeadbeefc0d1f1ed + num_procs) ) {
            fprintf(stderr, "bad value at idx %u (readval[%u] = 0x%lx)\n",
                            idx, idx, (unsigned long)readval[idx]);
            abort();
        }
        // check that all entries are identical to one another
        if (tmp != readval[idx]) {
            fprintf(stderr, "bad value at idx %u (readval[%u] = 0x%lx), "
                            "expected %lu everywhere (are atomics atomic?)\n",
                            idx, idx, (unsigned long)readval[idx], (unsigned long)tmp);
            abort();
        }
    }

    if (myself.rank == 0) {
        int failed = 0;
        NO_FAILURES(value_e.ct_handle, num_procs);
        tmp = value[0];
        for (unsigned idx=0; idx<BUFSIZE; idx++) {
            // printf("0 value: %llx\n", (unsigned long long)value);
            //assert(value == 0xdeadbeefc0d1f1ed + num_procs);
            if (value[idx] != (0xdeadbeefc0d1f1ed + num_procs)) {
                fprintf(stderr, "bad result value at idx %u (value[%u] = 0x%lx)"
                        ", expected 0x%lx\n",
                        idx, idx, (unsigned long)value[idx], 0xdeadbeefc0d1f1edUL + num_procs);
                failed = 1;
            }
            // check that all entries are identical to one another
            if (tmp != value[idx]) {
                fprintf(stderr,
                        "bad result value at idx %u (value[%u] = 0x%lx), "
                        "expected %lu everywhere (are atomics atomic?)\n",
                        idx, idx, (unsigned long)value[idx], (unsigned long)tmp);
                failed = 1;
            }
        }
        if (failed) {
            abort();
        }
        CHECK_RETURNVAL(UNLINK(value_e_handle));
        CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle));
    }
    CHECK_RETURNVAL(PtlMDRelease(read_md_handle));
    CHECK_RETURNVAL(PtlCTFree(read_md.ct_handle));

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
