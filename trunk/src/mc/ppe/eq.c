#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/eq.h"

int
eq_alloc_impl( ptl_ppe_t *ctx, ptl_cqe_eqalloc_t *cmd )
{
    ptl_ppe_ni_t     *ni;
    ptl_ppe_eq_t     *ppe_eq;
    ptl_ppe_client_t *client;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->eq_handle.s.selector,
                    cmd->eq_handle.s.code, cmd->eq_handle.s.ni);

    client = &ctx->clients[ cmd->base.remote_id ];
    ni     = &client->nis[ cmd->eq_handle.s.ni ];
    ppe_eq = ni->ppe_eq + cmd->eq_handle.s.code;

    ppe_eq->xpmem_ptr = ppe_xpmem_attach( &client->xpmem_segments,
                 cmd->cb, sizeof(ptl_circular_buffer_t)  +
                        sizeof(ptl_event_t) * cmd->count );

    return 0;
}

int
eq_free_impl( ptl_ppe_t *ctx, ptl_cqe_eqfree_t *cmd )
{
    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->eq_handle.s.selector,
                    cmd->eq_handle.s.code, cmd->eq_handle.s.ni);
    return 0;
}
