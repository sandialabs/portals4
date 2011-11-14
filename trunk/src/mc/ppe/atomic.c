#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

int
atomic_impl( ptl_ppe_t *ctx, ptl_cqe_atomic_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}

int
fetch_atomic_impl( ptl_ppe_t *ctx, ptl_cqe_fetchatomic_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}

int
swap_impl( ptl_ppe_t *ctx, ptl_cqe_swap_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}

int
atomic_sync_impl( ptl_ppe_t *ctx, ptl_cqe_atomicsync_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}
