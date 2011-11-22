#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

int
md_bind_impl( ptl_ppe_t *ctx, ptl_cqe_mdbind_t *cmd )
{
    ptl_ppe_ni_t     *ni;
    ptl_ppe_md_t     *ppe_md;
    ptl_ppe_client_t *client;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->md_handle.s.selector,
                    cmd->md_handle.s.code, cmd->md_handle.s.ni );

    client = &ctx->clients[ cmd->base.remote_id ]; 
    ni     = &client->nis[ cmd->md_handle.s.ni ];
    ppe_md = ni->ppe_md + cmd->md_handle.s.code;

    ppe_md->options   = cmd->md.options;
    ppe_md->eq_h      = (ptl_handle_generic_t) cmd->md.eq_handle;
    ppe_md->ct_h      = (ptl_handle_generic_t) cmd->md.ct_handle;
    ppe_md->ref_cnt   = 0;
    ppe_md->xpmem_ptr = ppe_xpmem_attach( &client->xpmem_segments,
                                    cmd->md.start, cmd->md.length );

    // should we return a failure or assert?
    assert( ppe_md->xpmem_ptr );

    return 0;
}

int
md_release_impl( ptl_ppe_t *ctx, ptl_cqe_mdrelease_t *cmd )
{
    int                 ret;
    ptl_ppe_ni_t       *ni;
    ptl_ppe_md_t       *ppe_md; 
    ptl_ppe_client_t   *client;
    ptl_internal_md_t  *shared_md; 

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->md_handle.s.selector,
                    cmd->md_handle.s.code, cmd->md_handle.s.ni );

    client    = &ctx->clients[ cmd->base.remote_id ]; 
    ni        = &client->nis[ cmd->md_handle.s.ni ];
    ppe_md    = ni->ppe_md + cmd->md_handle.s.code;
    shared_md = ni->client_md + cmd->md_handle.s.code;

    // how do we handle a md that's involved in a xfer 
    assert( ppe_md->ref_cnt == 0 );

    // why do we hang when this is called
    ret = ppe_xpmem_detach( &client->xpmem_segments, ppe_md->xpmem_ptr );
    assert( 0 == ret );  

    shared_md->in_use = 0;

    return 0;
}
