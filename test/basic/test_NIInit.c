#include <portals4.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "testing.h"

#if PHYSICAL_ADDR == 1
#define OPTIONS (PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL)
#else
#define OPTIONS (PTL_NI_NO_MATCHING | PTL_NI_LOGICAL)
#endif

int
main(int argc, char *argv[])
{
    ptl_ni_limits_t actual;
    ptl_handle_ni_t ni_h;

#if 0 /* Only works if arg validation is turned on */
    if (PtlNIInit
            (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
            PTL_PID_ANY,
            NULL, &actual, &ni_handle_phys) != PTL_NO_INIT) {
        return 1;
    }
#endif
    CHECK_RETURNVAL(PtlInit());
    CHECK_RETURNVAL(PtlInit());
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT,
                              OPTIONS,
                              PTL_PID_ANY,
                              NULL, &actual,
                              &ni_h));
    CHECK_RETURNVAL(PtlNIFini(ni_h));
    PtlFini();
    PtlFini();
    return 0;
}

/* vim:set expandtab: */
