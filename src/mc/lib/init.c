#include "config.h"

#include "portals4.h"
#include "alloc.h"

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
