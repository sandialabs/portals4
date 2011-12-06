#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/ct.h"

int
ct_alloc_impl( ptl_ppe_t *ctx, ptl_cqe_ctalloc_t *cmd )
{
    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ct_handle.s.selector, 
                    cmd->ct_handle.s.code, cmd->ct_handle.s.ni );

    return 0;
}

int
ct_free_impl( ptl_ppe_t *ctx, ptl_cqe_ctfree_t *cmd )
{
    ptl_ppe_ni_t      *ni;
    ptl_internal_ct_t *ct;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ct_handle.s.selector, 
                    cmd->ct_handle.s.code, cmd->ct_handle.s.ni);
    ni = &ctx->clients[ cmd->base.remote_id ].nis[ cmd->ct_handle.s.ni ];
    ct =  ni->client_ct + cmd->ct_handle.s.code;

    // MJL: how do we coordiate with pending data xfers that will want to
    // use this CT?

    ct->in_use = 0;
    return 0;
}

int
ct_set_impl( ptl_ppe_t *ctx, ptl_cqe_ctop_t *cmd )
{
    ptl_ppe_ni_t      *ni;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ct_handle.s.selector, 
                    cmd->ct_handle.s.code, cmd->ct_handle.s.ni );

    ni = &ctx->clients[cmd->base.remote_id].nis[cmd->ct_handle.s.ni];

    if ( cmd->base.type == PTLCTSET ) {
        ct_set( ni, cmd->ct_handle.s.code, cmd->ct_event );
    } else {
        PPE_DBG("\n"); 
    }

    return 0;
}

int
ct_inc_impl( ptl_ppe_t *ctx, ptl_cqe_ctop_t *cmd )
{
    ptl_ppe_ni_t      *ni;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ct_handle.s.selector, 
                    cmd->ct_handle.s.code, cmd->ct_handle.s.ni );

    ni = &ctx->clients[cmd->base.remote_id].nis[cmd->ct_handle.s.ni];

    if ( cmd->base.type == PTLCTINC ) {
        PtlInternalCTSuccessInc( ni, cmd->ct_handle.a, cmd->ct_event.success );
    } else {
        PPE_DBG("\n"); 
    }
    
    return 0;
}
