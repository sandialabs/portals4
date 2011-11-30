#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/nal.h"

int
put_impl( ptl_ppe_t *ctx, ptl_cqe_put_t *cmd )
{
    int                 retval;
    nal_ctx_t           *nal_ctx;
    ptl_ppe_ni_t       *ppe_ni;
    ptl_ppe_md_t       *ppe_md;
    ptl_process_id_t    dst;

    PPE_DBG("remote_id=%d\n",cmd->base.remote_id);

    ppe_ni = &ctx->clients[cmd->base.remote_id].nis[cmd->md_handle.s.ni];
    ppe_md = ppe_ni->ppe_md + cmd->md_handle.s.code; 

    nal_ctx = malloc( sizeof( *nal_ctx ) );
    assert(nal_ctx);

    nal_ctx->u.md.local_offset   = cmd->local_offset;
    nal_ctx->u.md.user_ptr       = cmd->user_ptr;
    nal_ctx->hdr.length          = cmd->length;
    nal_ctx->hdr.ack_req         = cmd->ack_req;

    // who should do the phys vs logical check app or engine? 
    nal_ctx->hdr.src.pid          = cmd->base.remote_id;
    nal_ctx->hdr.target.pid       = cmd->target_id.phys.pid;

    nal_ctx->hdr.match_bits      = cmd->match_bits;
    nal_ctx->hdr.dest_offset     = cmd->remote_offset;
    nal_ctx->hdr.remaining       = cmd->length;
    nal_ctx->hdr.pt_index        = cmd->pt_index;
    nal_ctx->hdr.hdr_data        = cmd->hdr_data;
    nal_ctx->hdr.ni              = cmd->md_handle.s.ni;
    nal_ctx->hdr.type            = HDR_TYPE_PUT;
    nal_ctx->hdr.md_index        = cmd->md_handle.s.code;

    nal_ctx->hdr.user_ptr        = cmd->user_ptr;

    dst.nid = cmd->target_id.phys.nid;
    dst.pid = cmd->target_id.phys.pid;

    nal_ctx->iovec.iov_base = ppe_md->xpmem_ptr->data + cmd->local_offset;
    nal_ctx->iovec.iov_len = ppe_md->xpmem_ptr->length;
    nal_ctx->type = MD_CTX;
    nal_ctx->u.md.ppe_md = ppe_md;
    nal_ctx->ppe_ni = ppe_ni;
    
    ++ppe_md->ref_cnt;

    PPE_DBG("dst nid=%#x pid=%d length=%lu\n",dst.nid,dst.pid, cmd->length );

    retval = ctx->ni.nal->send( &ctx->ni,
                                &nal_ctx->nal_msg_data,      // nal_msg_data 
                                nal_ctx,                     // lib_data
                                dst,                        // dest 
                                (lib_mem_t *) &nal_ctx->hdr, // hdr 
                                sizeof(nal_ctx->hdr),        // hdrlen 
                                &nal_ctx->iovec,             // iov
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
    int                 retval;
    nal_ctx_t           *nal_ctx;
    ptl_ppe_ni_t       *ppe_ni;
    ptl_ppe_md_t       *ppe_md;
    ptl_process_id_t    dst;

    PPE_DBG("remote_id=%lu\n",cmd->base.remote_id);

    ppe_ni = &ctx->clients[cmd->base.remote_id].nis[cmd->md_handle.s.ni];
    ppe_md = ppe_ni->ppe_md + cmd->md_handle.s.code; 

    nal_ctx = malloc( sizeof( *nal_ctx ) );
    assert(nal_ctx);

    nal_ctx->u.md.local_offset   = cmd->local_offset;
    nal_ctx->u.md.user_ptr       = cmd->user_ptr;
    nal_ctx->hdr.length          = cmd->length;

    // who should do the phys vs logical check app or engine? 
    nal_ctx->hdr.src.pid          = cmd->base.remote_id;
    nal_ctx->hdr.target.pid       = cmd->target_id.phys.pid;

    nal_ctx->hdr.match_bits      = cmd->match_bits;
    nal_ctx->hdr.dest_offset     = cmd->remote_offset;
    nal_ctx->hdr.pt_index        = cmd->pt_index;
    nal_ctx->hdr.ni              = cmd->md_handle.s.ni;
    nal_ctx->hdr.type            = HDR_TYPE_GET;
    nal_ctx->hdr.md_index        = cmd->md_handle.s.code;

    nal_ctx->hdr.user_ptr        = cmd->user_ptr;

    dst.nid = cmd->target_id.phys.nid;
    dst.pid = cmd->target_id.phys.pid;

    nal_ctx->iovec.iov_base = ppe_md->xpmem_ptr->data + cmd->local_offset;
    nal_ctx->iovec.iov_len = ppe_md->xpmem_ptr->length;
    nal_ctx->type = MD_CTX;
    nal_ctx->u.md.ppe_md = ppe_md;
    nal_ctx->ppe_ni = ppe_ni;
    
    ++ppe_md->ref_cnt;

    PPE_DBG("dst nid=%#x pid=%d length=%lu\n",dst.nid,dst.pid, cmd->length );

    retval = ctx->ni.nal->send( &ctx->ni,
                                &nal_ctx->nal_msg_data,         // nal_msg_data 
                                nal_ctx,                        // lib_data
                                dst,                        // dest 
                                (lib_mem_t *) &nal_ctx->hdr,   // hdr 
                                sizeof(nal_ctx->hdr),          // hdrlen 
                                NULL,                       // iov
                                0,                          // iovlen
                                0,                          // offset
                                0,                          // len
                                NULL                        // addrkey
                             );
    return 0;
}
