#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

int
pt_alloc_impl( ptl_ppe_t *ctx, ptl_cqe_ptalloc_t *cmd )
{
    ptl_ppe_ni_t      *ni;
    ptl_ppe_client_t  *client;
    ptl_ppe_pt_t      *ppe_pt;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ni_handle.s.selector,
                    cmd->ni_handle.s.code, cmd->ni_handle.s.ni);

    client = &ctx->clients[ cmd->base.remote_id ];
    ni     = &client->nis[ cmd->ni_handle.s.ni ];
    ppe_pt = ni->ppe_pt + cmd->pt_index;

    //ppe_pt->eq_h      = cmd->eq_handle;
    ppe_pt->EQ      = (ptl_handle_eq_t)cmd->eq_handle.a;
    ppe_pt->options   = cmd->options;
    
    return 0;
}

int
pt_free_impl( ptl_ppe_t *ctx, ptl_cqe_ptfree_t *cmd )
{
    ptl_ppe_ni_t      *ni;
    ptl_ppe_client_t  *client;
    ptl_internal_pt_t *shared_pt;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ni_handle.s.selector,
                    cmd->ni_handle.s.code, cmd->ni_handle.s.ni);

    client    = &ctx->clients[ cmd->base.remote_id ];
    ni        = &client->nis[ cmd->ni_handle.s.ni ];
    shared_pt = ni->client_pt + cmd->pt_index;

    // if lists are not empty return fail?

    shared_pt->in_use = 0;
    return 0;
}
