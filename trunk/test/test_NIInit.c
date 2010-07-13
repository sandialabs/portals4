#include <portals4.h>

#include <assert.h>
#include <stddef.h>

int main(
    int argc,
    char *argv[])
{
    ptl_ni_limits_t actual;
    ptl_handle_ni_t ni_handle_phys;

    assert(PtlNIInit
	   (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
	    PTL_PID_ANY, NULL, &actual, 0, NULL, NULL,
	    &ni_handle_phys) == PTL_NO_INIT);
    assert(PtlInit() == PTL_OK);
    assert(PtlInit() == PTL_OK);
    assert(PtlNIInit
	   (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
	    PTL_PID_ANY, NULL, &actual, 0, NULL, NULL,
	    &ni_handle_phys) == PTL_OK);
    assert(PtlNIFini(ni_handle_phys) == PTL_OK);
    PtlFini();
    PtlFini();
    return 0;
}
