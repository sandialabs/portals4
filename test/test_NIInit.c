#include <portals4.h>

#include <assert.h>
#include <stddef.h>

int main(
         int argc,
         char *argv[])
{
    ptl_ni_limits_t actual;
    ptl_handle_ni_t ni_handle_phys;

#if 0 /* Only works if arg validation is turned on */
    if (PtlNIInit
            (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
            PTL_PID_ANY,
            NULL, &actual, &ni_handle_phys) != PTL_NO_INIT) {
        return 1;
    }
#endif
    if (PtlInit() != PTL_OK) {
        return 1;
    }
    if (PtlInit() != PTL_OK) {
        return 1;
    }
    if (PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
                  PTL_PID_ANY, NULL, &actual, &ni_handle_phys) != PTL_OK) {
        return 1;
    }
    if (PtlNIFini(ni_handle_phys) != PTL_OK) {
        return 1;
    }
    PtlFini();
    PtlFini();
    return 0;
}

/* vim:set expandtab: */
