#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "testing.h"

#if INTERFACE == 1
# define ENTRY_T  ptl_me_t
# define HANDLE_T ptl_handle_me_t
# define NI_TYPE  PTL_NI_MATCHING

# define OPTIONS  (PTL_ME_OP_PUT | PTL_ME_OP_GET | PTL_ME_EVENT_CT_COMM)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define OPTIONS  (PTL_LE_OP_PUT | PTL_LE_OP_GET | PTL_LE_EVENT_CT_COMM)
# define APPEND   PtlLEAppend
# define UNLINK   PtlLEUnlink
#endif /* if INTERFACE == 1 */

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t   myself;
    ptl_pt_index_t  logical_pt_index;
    uint64_t        value, readval;
    ENTRY_T         value_e;
    HANDLE_T        value_e_handle;
    ptl_md_t        read_md;
    ptl_handle_md_t read_md_handle;
    int             num_procs;
    ptl_handle_ct_t  trigger;
    ptl_ni_limits_t  ni_limits;
    int              max_triggered_ops;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    num_procs = libtest_get_size();

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, &ni_limits, &ni_logical));

    CHECK_RETURNVAL(PtlSetMap(ni_logical, num_procs,
                              libtest_get_mapping(ni_logical)));
    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY,
                               &logical_pt_index));
    assert(logical_pt_index == 0);
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &trigger));
    /* Now do the initial setup on ni_logical */
    value = myself.rank + 0xdeadbeefc0d1f1ed;
    if (myself.rank == 0) {
        value_e.start   = &value;
        value_e.length  = sizeof(value);
        value_e.uid     = PTL_UID_ANY;
        value_e.options = OPTIONS;
#if INTERFACE == 1
        value_e.match_id.rank = PTL_RANK_ANY;
        value_e.match_bits    = 1;
        value_e.ignore_bits   = 0;
#endif
        CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &value_e.ct_handle));
        CHECK_RETURNVAL(APPEND(ni_logical, 0, &value_e, PTL_PRIORITY_LIST,
                               NULL, &value_e_handle));
    }
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    libtest_barrier();

    /* now I can communicate between ranks with ni_logical */


    
    max_triggered_ops = ni_limits.max_triggered_ops;
    fprintf(stdout, "#### max triggered ops = %d\n", max_triggered_ops);

    /* check the trigger, make sure it's zero */
    {
        ptl_ct_event_t test;
        CHECK_RETURNVAL(PtlCTGet(trigger, &test));
        assert(test.success == 0);
        assert(test.failure == 0);
    }
    
    /* set up the landing pad so that I can read others' values */
    readval           = myself.rank;
    read_md.start     = &readval;
    read_md.length    = sizeof(uint64_t);
    read_md.options   = PTL_MD_EVENT_CT_REPLY;
    read_md.eq_handle = PTL_EQ_NONE;   // i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &read_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &read_md, &read_md_handle));

    /* twiddle rank 0's value */
    {
        ptl_ct_event_t ctc;
        ptl_process_t  r0 = { .rank = 0 };
        int assertval;
        int ret;
        for (int i = 0; i < max_triggered_ops + 1; i++) {
            assertval = (i == max_triggered_ops) ? PTL_NO_SPACE : PTL_OK;
            ret = PtlTriggeredSwap(read_md_handle, 0,
                                   read_md_handle, 0,
                                   sizeof(uint64_t),
                                   r0, logical_pt_index,
                                   1, 0,
                                   NULL, 0,
                                   NULL, PTL_SWAP,
                                   PTL_UINT64_T,
                                   trigger, 1);
            assert(ret == assertval);
        }
    }
    /* Increment the trigger */
    CHECK_RETURNVAL(PtlCTInc(trigger, (ptl_ct_event_t) { 1 , 0 }));
    /* CHECK_RETURNVAL(PtlCTWait(read_md.ct_handle, 1, &ctc)); */

    if (myself.rank == 0) {
        NO_FAILURES(value_e.ct_handle, num_procs);
        // printf("0 value: %llx\n", (unsigned long long)value);
        /* assert(value < num_procs); */
        /* CHECK_RETURNVAL(UNLINK(value_e_handle)); */
        /* CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle)); */
    }
    /* CHECK_RETURNVAL(PtlMDRelease(read_md_handle)); */
    /* CHECK_RETURNVAL(PtlCTFree(read_md.ct_handle)); */

    /* cleanup */
    /* CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index)); */
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
