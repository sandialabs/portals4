#include <portals4.h>
#include <portals4_runtime.h>

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

int main(
         int argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t myself;
    ptl_pt_index_t logical_pt_index;
    // uint64_t value, readval;
    double value, readval;
    ENTRY_T value_e;
    HANDLE_T value_e_handle;
    ptl_md_t read_md;
    ptl_handle_md_t read_md_handle;
    int my_rank, num_procs;

    CHECK_RETURNVAL(PtlInit());

    my_rank = runtime_get_rank();
    num_procs = runtime_get_size();

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_logical));
    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    assert(my_rank == myself.rank);
    CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY,
                               &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    // value = myself.rank + 0xdeadbeefc0d1f1ed;
    value = 77.5;
    if (myself.rank == 0) {
        value_e.start = &value;
        value_e.length = sizeof(value);
        value_e.ac_id.uid = PTL_UID_ANY;
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
    runtime_barrier();
    /* don't need this anymore, so free up resources */

    /* now I can communicate between ranks with ni_logical */

    /* set up the landing pad so that I can read others' values */
    readval = 1.1;
    read_md.start = &readval;
    // read_md.length = sizeof(uint64_t);
    read_md.length = sizeof(double);
    read_md.options = PTL_MD_EVENT_CT_ACK;
    read_md.eq_handle = PTL_EQ_NONE;   // i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &read_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &read_md, &read_md_handle));

    /* twiddle rank 0's value */
    {
        ptl_ct_event_t ctc;
        ptl_process_t r0 = {.rank = 0 };
        CHECK_RETURNVAL(PtlAtomic(read_md_handle, 0, sizeof(double),
                                  PTL_OC_ACK_REQ, r0, logical_pt_index, 1, 0,
                                  NULL, 0, PTL_SUM,
                                  PTL_DOUBLE));
        CHECK_RETURNVAL(PtlCTWait(read_md.ct_handle, 1, &ctc));
        assert(ctc.failure == 0);
    }
    /*printf("%i readval: %g\n", (int)myself.rank,
     *     readval);*/
    assert(readval == 1.1);

    if (myself.rank == 0) {
        NO_FAILURES(value_e.ct_handle, num_procs);
        /*printf("0 value: %g\n", value);*/
        assert(fpequal(value, 77.5 + (1.1 * num_procs)));
        CHECK_RETURNVAL(UNLINK(value_e_handle));
        CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle));
    }
    CHECK_RETURNVAL(PtlMDRelease(read_md_handle));
    CHECK_RETURNVAL(PtlCTFree(read_md.ct_handle));

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
