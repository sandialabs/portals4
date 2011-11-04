#include "config.h"

#include "portals4.h"
#include "alloc.h"

size_t    num_siblings           = 0;
ptl_pid_t proc_number            = PTL_PID_ANY;

size_t LARGE_FRAG_PAYLOAD = 0;

int
PtlInit(void)
{
    alloc_init();

    return PTL_OK;
}


void
PtlFini(void)
{
    alloc_fini();
}
