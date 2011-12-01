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

    // MJL: should we return a failure or assert?
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
    ptl_cqe_t          *send_entry;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->md_handle.s.selector,
                    cmd->md_handle.s.code, cmd->md_handle.s.ni );

    client    = &ctx->clients[ cmd->base.remote_id ]; 
    ni        = &client->nis[ cmd->md_handle.s.ni ];
    ppe_md    = ni->ppe_md + cmd->md_handle.s.code;
    shared_md = ni->client_md + cmd->md_handle.s.code;

    ret = ptl_cq_entry_alloc(ctx->cq_h, &send_entry);
    if (ret < 0) {
        perror("ptl_cq_entry_alloc");
        return -1;
    }

    send_entry->base.type = PTLACK;
    send_entry->ack.retval_ptr = cmd->retval_ptr;

    if ( ppe_md->ref_cnt == 0 ) {

        if ( send_entry->ack.retval == PTL_OK ) {

            ret = ppe_xpmem_detach( &client->xpmem_segments, ppe_md->xpmem_ptr );
            assert( 0 == ret );

            // MJL: we could get rid of the shared key because this is a blocking
            // call and the engine doesn't unlink MD's 
            shared_md->in_use = 0;
        }
    } else {
        send_entry->ack.retval = PTL_IN_USE;
    }

    ret = ptl_cq_entry_send(ctx->cq_h, cmd->base.remote_id,
                            send_entry, sizeof(ptl_cqe_t));
    if (ret < 0) {
        perror("ptl_cq_entry_send");
        return -1;
    }

    return 0;
}
