#include "config.h"

#include <stddef.h>
#include <stdio.h>

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_iface.h"
#include "ptl_internal_startup.h"
#include "shared/ptl_command_queue_entry.h"

ptl_iface_t ptl_iface = { 0, 0 };

int
PtlInit(void)
{
    if (0 == __sync_fetch_and_add(&ptl_iface.init_count, 1)) {
        ptl_iface.connection_count = 0;
        ptl_iface.my_ppe_rank = -1;
    }
    
#if 0
    printf("sizeof(ptl_cqe_t) = %ld\n", sizeof(ptl_cqe_t));
    printf("sizeof(ptl_ni_limits_t) = %ld\n", sizeof(ptl_ni_limits_t));
#endif

    return PTL_OK;
}


void
PtlFini(void)
{
    if (0 == __sync_fetch_and_sub(&ptl_iface.init_count, 1)) {
        if (0 != ptl_iface.connection_count) {
            ptl_ppe_disconnect(&ptl_iface);
        }
    }
}
