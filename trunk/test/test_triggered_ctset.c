#include <portals4.h>
#include <support/support.h>

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
    int             num_procs;
    ptl_handle_ct_t trigger, target;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    num_procs = libtest_get_size();

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_logical));

    {
        ptl_process_t *amapping;
        amapping = libtest_get_mapping();
        CHECK_RETURNVAL(PtlSetMap(ni_logical, num_procs, amapping));
        free(amapping);
    }

    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &trigger));
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &target));
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    libtest_barrier();

#ifdef ORDERED
    CHECK_RETURNVAL(PtlTriggeredCTSet(target, (ptl_ct_event_t) { 1, 0 }, trigger, 1));
    CHECK_RETURNVAL(PtlTriggeredCTSet(target, (ptl_ct_event_t) { 7, 0 }, trigger, 2));
#else
    CHECK_RETURNVAL(PtlTriggeredCTSet(target, (ptl_ct_event_t) { 7, 0 }, trigger, 2));
    CHECK_RETURNVAL(PtlTriggeredCTSet(target, (ptl_ct_event_t) { 1, 0 }, trigger, 1));
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
    CHECK_RETURNVAL(PtlCTInc(trigger, (ptl_ct_event_t) { 1, 0 }));
    /* Check the target */
    {
        ptl_ct_event_t test;
        CHECK_RETURNVAL(PtlCTWait(target, 1, &test));
        assert(test.success == 1);
        assert(test.failure == 0);
    }
    /* Increment the trigger again */
    CHECK_RETURNVAL(PtlCTInc(trigger, (ptl_ct_event_t) { 1, 0 }));
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
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
