#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/pt.h"

int
le_append_impl( ptl_ppe_t *ctx, ptl_cqe_leappend_t *cmd )
{
    ptl_ppe_ni_t     *ni;
    ptl_ppe_le_t     *ppe_le;
    ptl_ppe_pt_t     *ppe_pt;
    ptl_ppe_client_t *client;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->le_handle.s.selector,
                    cmd->le_handle.s.code, cmd->le_handle.s.ni );

    client = &ctx->clients[ cmd->base.remote_id ];
    ni     = &client->nis[ cmd->le_handle.s.ni ];
    ppe_le = ni->ppe_le + cmd->le_handle.s.code;
    ppe_pt = ni->ppe_pt + cmd->pt_index;

    ppe_le->pt_index = cmd->pt_index;
    ppe_le->list     = cmd->list;
    ppe_le->user_ptr = cmd->user_ptr;

    // file in ptl_le_t data 
    ppe_le->ct_h        = (ptl_handle_generic_t)cmd->le.ct_handle;
    ppe_le->uid         = cmd->le.uid;
    ppe_le->options     = cmd->le.options;
    ppe_le->xpmem_ptr   = ppe_xpmem_attach( &client->xpmem_segments,
                                    cmd->le.start, cmd->le.length );
    assert( ppe_le->xpmem_ptr );

    pt_append_le( ppe_pt, cmd->list, ppe_le );

    return 0;
}

int
le_unlink_impl( ptl_ppe_t *ctx, ptl_cqe_leunlink_t *cmd )
{
    ptl_ppe_ni_t      *ni;
    ptl_internal_le_t *shared_le;
    ptl_ppe_le_t      *ppe_le;
    ptl_ppe_pt_t      *ppe_pt;
    ptl_ppe_client_t  *client;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->le_handle.s.selector,
                    cmd->le_handle.s.code, cmd->le_handle.s.ni );

    client    = &ctx->clients[ cmd->base.remote_id ];
    ni        = &client->nis[ cmd->le_handle.s.ni ];
    ppe_le    = ni->ppe_le + cmd->le_handle.s.code;
    shared_le = ni->client_le + cmd->le_handle.s.code;
    ppe_pt    = ni->ppe_pt + ppe_le->pt_index;

    pt_unlink_le( ppe_pt, ppe_le->list, ppe_le );

    // how do we handle a md that's involved in a xfer 
    assert( ppe_le->ref_cnt == 0 );

    // why do we hang when this is called
    //ret = ppe_xpmem_detach( &client->xpmem_segments, ppe_le->xpmem_ptr );
    //assert( 0 == ret );  

    shared_le->in_use = 0;

    return 0;
}

int
le_search_impl( ptl_ppe_t *ctx, ptl_cqe_lesearch_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}
