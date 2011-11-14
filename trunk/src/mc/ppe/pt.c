#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

int
pt_alloc_impl( ptl_ppe_t *ctx, ptl_cqe_ptalloc_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}

int
pt_free_impl( ptl_ppe_t *ctx, ptl_cqe_ptfree_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}
