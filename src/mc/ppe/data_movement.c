#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/nal.h"

int
put_impl( ptl_ppe_t *ctx, ptl_cqe_put_t *cmd )
{
    int                 retval;
    dm_ctx_t           *dm_ctx;
    ptl_ppe_ni_t       *ni;
    ptl_ppe_md_t       *ppe_md;
    ptl_process_id_t    dst;

    PPE_DBG("remote_id=%d\n",cmd->base.remote_id);

    ni = &ctx->clients[cmd->base.remote_id].nis[cmd->md_handle.s.ni];
    ppe_md = ni->ppe_md + cmd->md_handle.s.code; 

    dm_ctx = malloc( sizeof( dm_ctx_t ) );
    dm_ctx->user_ptr            = cmd->user_ptr;
    dm_ctx->hdr.length          = cmd->length;
    dm_ctx->hdr.ack_req         = cmd->ack_req;
    dm_ctx->hdr.src_id.phys.pid   = cmd->base.remote_id;
    dm_ctx->hdr.src_id.phys.nid   = ctx->nid;
    dm_ctx->hdr.target_id       = cmd->target_id;
    dm_ctx->hdr.match_bits      = cmd->match_bits;
    dm_ctx->hdr.remote_offset   = cmd->remote_offset;
    dm_ctx->hdr.pt_index        = cmd->pt_index;
    dm_ctx->hdr.hdr_data        = cmd->hdr_data;
    dm_ctx->hdr.ni              = cmd->md_handle.s.ni;

    dst.nid = cmd->target_id.phys.nid;
    dst.pid = cmd->target_id.phys.pid;

    dm_ctx->iovec.iov_base = ppe_md->xpmem_ptr->data + cmd->local_offset;
    dm_ctx->iovec.iov_len = cmd->length;
    dm_ctx->id = MD_CTX;
    dm_ctx->u.ppe_md = ppe_md;
    dm_ctx->ppe_ni = ni;
    
    ++ppe_md->ref_cnt;

    PPE_DBG("dst nid=%#x pid=%d length=%lu\n",dst.nid,dst.pid, cmd->length );

    retval = ctx->ni.nal->send( &ctx->ni,
                                &dm_ctx->nal_msg_data,      // nal_msg_data 
                                dm_ctx,                     // lib_data
                                dst,                        // dest 
                                (lib_mem_t *) &dm_ctx->hdr, // hdr 
                                sizeof(dm_ctx->hdr),        // hdrlen 
                                &dm_ctx->iovec,             // iov
                                1,                          // iovlen
                                0,                          // offset
                                cmd->length,                // len
                                NULL                        // addrkey
                             );
    return 0;
}

int
get_impl( ptl_ppe_t *ctx, ptl_cqe_get_t *cmd )
{
    PPE_DBG("\n");
    return 0;
}
