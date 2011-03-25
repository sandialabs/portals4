#include <portals4.h>
#include <portals4_runtime.h>

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
# define OPTIONS  (PTL_ME_OP_GET | PTL_ME_EVENT_CT_COMM)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define OPTIONS  (PTL_LE_OP_GET | PTL_LE_EVENT_CT_COMM)
# define APPEND   PtlLEAppend
# define UNLINK   PtlLEUnlink
#endif /* if INTERFACE == 1 */

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t   myself;
    int             my_rank, num_procs;
    ptl_handle_ct_t trigger, target;

    CHECK_RETURNVAL(PtlInit());

    my_rank   = runtime_get_rank();
    num_procs = runtime_get_size();

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, 0, NULL,
                              NULL, &ni_logical));
    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    assert(my_rank == myself.rank);
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &trigger));
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &target));
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    runtime_barrier();

    ptl_ct_event_t inc  = { 1, 0 };
    ptl_ct_event_t inc2 = { 7, 0 };
#ifdef ORDERED
    CHECK_RETURNVAL(PtlTriggeredCTSet(target, inc, trigger, 1));
    CHECK_RETURNVAL(PtlTriggeredCTSet(target, inc2, trigger, 2));
#else
    CHECK_RETURNVAL(PtlTriggeredCTSet(target, inc2, trigger, 2));
    CHECK_RETURNVAL(PtlTriggeredCTSet(target, inc, trigger, 1));
#endif

    /* check the target and trigger, make sure they're both zero */
    {
        ptl_ct_event_t test;
        CHECK_RETURNVAL(PtlCTGet(target, &test));
        assert(test.success == 0);
        assert(test.failure == 0);
        CHECK_RETURNVAL(PtlCTGet(trigger, &test));
        assert(test.success == 0);
        assert(test.failure == 0);
    }
    /* Increment the trigger */
    CHECK_RETURNVAL(PtlCTInc(trigger, inc));
    /* Check the target */
    {
        ptl_ct_event_t test;
        CHECK_RETURNVAL(PtlCTWait(target, 1, &test));
        assert(test.success == 1);
        assert(test.failure == 0);
    }
    /* Increment the trigger again */
    CHECK_RETURNVAL(PtlCTInc(trigger, inc));
    {
        ptl_ct_event_t test;
        CHECK_RETURNVAL(PtlCTWait(target, 7, &test));
        assert(test.success == 7);
        assert(test.failure == 0);
    }

    /* cleanup */
    CHECK_RETURNVAL(PtlCTFree(trigger));
    CHECK_RETURNVAL(PtlCTFree(target));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
