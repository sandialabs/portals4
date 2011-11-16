#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/nal.h"

int
put_impl( ptl_ppe_t *ctx, ptl_cqe_put_t *cmd )
{
    PPE_DBG("\n");
    int retval;
    dm_ctx_t *dm_ctx = malloc( sizeof( dm_ctx_t ) );


    dm_ctx->user_ptr            = cmd->user_ptr;
    dm_ctx->hdr.length          = cmd->length;
    dm_ctx->hdr.ack_req         = cmd->ack_req;
    dm_ctx->hdr.target_id       = cmd->target_id;
    dm_ctx->hdr.match_bits      = cmd->match_bits;
    dm_ctx->hdr.remote_offset   = cmd->remote_offset;
    dm_ctx->hdr.pt_index        = cmd->pt_index;
    dm_ctx->hdr.hdr_data        = cmd->hdr_data;

    ptl_process_id_t dst;
    dst.nid = cmd->target_id.phys.nid;
    dst.pid = cmd->target_id.phys.pid;

    // for testing lets send out of some PPE memory
    dm_ctx->iovec.iov_base = malloc ( cmd->length );
    dm_ctx->iovec.iov_len = cmd->length;

    PPE_DBG("nid=%#x pid=%d length=%lu\n",dst.nid,dst.pid, cmd->length );

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
