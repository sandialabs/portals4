#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

int
atomic_sync_impl( ptl_ppe_t *ctx, ptl_cqe_atomicsync_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}
