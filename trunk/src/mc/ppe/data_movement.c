#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

int
put_impl( ptl_ppe_t *ctx, ptl_cqe_put_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}

int
get_impl( ptl_ppe_t *ctx, ptl_cqe_get_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}
