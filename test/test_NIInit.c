#include <portals4.h>

#include <assert.h>
#include <stddef.h>

int main(
    int argc,
    char *argv[])
{
    ptl_ni_limits_t actual;
    ptl_process_id_t desired_mapping;
    ptl_process_id_t actual_mapping;
    ptl_handle_ni_t ni_handle;

    assert(PtlInit() == PTL_OK);
    assert(PtlInit() == PTL_OK);
    assert(PtlNIInit
	   (0, PTL_NI_MATCHING | PTL_NI_LOGICAL, 0, NULL, &actual, 1,
	    &desired_mapping, &actual_mapping, &ni_handle) == PTL_OK);
    assert(PtlNIFini(ni_handle) == PTL_OK);
    PtlFini();
    PtlFini();
    return 0;
}
