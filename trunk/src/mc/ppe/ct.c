#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

int
ct_alloc_impl( ptl_ppe_t *ctx, ptl_cqe_ctalloc_t *cmd )
{
    PPE_DBG("selector%d code=%d ni=%d\n", cmd->ct_handle.s.selector, 
                    cmd->ct_handle.s.code, cmd->ct_handle.s.ni );

    return 0;
}

int
ct_free_impl( ptl_ppe_t *ctx, ptl_cqe_ctfree_t *cmd )
{
    PPE_DBG("selector%d code=%d ni=%d\n", cmd->ct_handle.s.selector, 
                    cmd->ct_handle.s.code, cmd->ct_handle.s.ni);
    ptl_ppe_ni_t *n = 
                &ctx->clients[cmd->base.remote_id].nis[cmd->ct_handle.s.ni];
    ptl_internal_ct_t *ct =  n->client_ct + cmd->ct_handle.s.code;

    ct->in_use = 0;
    return 0;
}

int
ct_set_impl( ptl_ppe_t *ctx, ptl_cqe_ctset_t *cmd )
{
    PPE_DBG("selector%d code=%d ni=%d\n", cmd->ct_handle.s.selector, 
                    cmd->ct_handle.s.code, cmd->ct_handle.s.ni );

    ptl_ppe_ni_t *n = 
                &ctx->clients[cmd->base.remote_id].nis[cmd->ct_handle.s.ni];
    ptl_internal_ct_t *ct =  n->client_ct + cmd->ct_handle.s.code;
    
    ct->ct_event = cmd->new_ct;
    return 0;
}

int
ct_inc_impl( ptl_ppe_t *ctx, ptl_cqe_ctinc_t *cmd )
{
    PPE_DBG("selector%d code=%d ni=%d\n", cmd->ct_handle.s.selector, 
                    cmd->ct_handle.s.code, cmd->ct_handle.s.ni );

    ptl_ppe_ni_t *n = 
                &ctx->clients[cmd->base.remote_id].nis[cmd->ct_handle.s.ni];
    ptl_internal_ct_t *ct =  n->client_ct + cmd->ct_handle.s.code;

    ct->ct_event.success += cmd->increment.success;
    return 0;
}
