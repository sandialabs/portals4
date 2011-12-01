#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/nal.h"
#include "ppe/data_movement.h"

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

    nal_ctx->type = MD_CTX;
    nal_ctx->ppe_ni = ppe_ni;
    nal_ctx->u.md.ppe_md = ppe_md;
    nal_ctx->iovec.iov_base = ppe_md->xpmem_ptr->data;
    nal_ctx->iovec.iov_len  = ppe_md->xpmem_ptr->length;

    nal_ctx->hdr.length          = cmd->length;
    nal_ctx->hdr.ack_req         = cmd->ack_req;

    // MJL who should do the phys vs logical check app or engine? 
    nal_ctx->hdr.src.pid         = cmd->base.remote_id;
    nal_ctx->hdr.target.pid      = cmd->target_id.phys.pid;

    nal_ctx->hdr.match_bits      = cmd->match_bits;
    nal_ctx->hdr.dest_offset     = cmd->remote_offset;
    nal_ctx->hdr.remaining       = cmd->length;
    nal_ctx->hdr.pt_index        = cmd->pt_index;
    nal_ctx->hdr.hdr_data        = cmd->hdr_data;
    nal_ctx->hdr.ni              = cmd->md_handle.s.ni;
    nal_ctx->hdr.type            = HDR_TYPE_PUT;

    nal_ctx->u.md.user_ptr = cmd->user_ptr;

    if ( cmd->ack_req == PTL_ACK_REQ ) {
        nal_ctx->hdr.ack_ctx_key = 
            alloc_ack_ctx( cmd->md_handle, cmd->local_offset, cmd->user_ptr );
        assert( nal_ctx->hdr.ack_ctx_key ); 
    }

    dst.nid = cmd->target_id.phys.nid;
    dst.pid = cmd->target_id.phys.pid;
    
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
                                cmd->local_offset,          // offset
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

    PPE_DBG("remote_id=%d\n",cmd->base.remote_id);

    ppe_ni = &ctx->clients[cmd->base.remote_id].nis[cmd->md_handle.s.ni];
    ppe_md = ppe_ni->ppe_md + cmd->md_handle.s.code; 

    nal_ctx = malloc( sizeof( *nal_ctx ) );
    assert(nal_ctx);

    nal_ctx->type = MD_CTX;
    nal_ctx->ppe_ni = ppe_ni;
    nal_ctx->u.md.ppe_md = ppe_md;
    nal_ctx->iovec.iov_base = ppe_md->xpmem_ptr->data + cmd->local_offset;
    nal_ctx->iovec.iov_len  = ppe_md->xpmem_ptr->length;

    nal_ctx->hdr.length          = cmd->length;

    // MJL who should do the phys vs logical check app or engine? 
    nal_ctx->hdr.src.pid          = cmd->base.remote_id;
    nal_ctx->hdr.target.pid       = cmd->target_id.phys.pid;

    nal_ctx->hdr.match_bits      = cmd->match_bits;
    nal_ctx->hdr.dest_offset     = cmd->remote_offset;
    nal_ctx->hdr.remaining       = cmd->length;
    nal_ctx->hdr.pt_index        = cmd->pt_index;
    nal_ctx->hdr.ni              = cmd->md_handle.s.ni;
    nal_ctx->hdr.type            = HDR_TYPE_GET;
    nal_ctx->hdr.ack_ctx_key     = 
            alloc_ack_ctx( cmd->md_handle, cmd->local_offset,  cmd->user_ptr );
    assert( nal_ctx->hdr.ack_ctx_key ); 

    PPE_DBG("get_ctx_key %d\n",nal_ctx->hdr.ack_ctx_key);
    dst.nid = cmd->target_id.phys.nid;
    dst.pid = cmd->target_id.phys.pid;

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


#define NUM_ACK_CTX 10
static ack_ctx_t _ack_ctx[NUM_ACK_CTX] = 
            { [0 ...( NUM_ACK_CTX - 1)].md_h.a = PTL_INVALID_HANDLE };

int 
alloc_ack_ctx( ptl_handle_generic_t md_h, ptl_size_t local_offset, 
                void *user_ptr )
{
    int i;
    for ( i = 0; i < NUM_ACK_CTX; i++ ) {
        if ( _ack_ctx[i].md_h.a == PTL_INVALID_HANDLE ) {
            _ack_ctx[i].md_h         = md_h;
            _ack_ctx[i].local_offset = local_offset;
            _ack_ctx[i].user_ptr     = user_ptr;
            return i + 1;
        }
    }
    return 0;
}

void 
free_ack_ctx( int key, ack_ctx_t *ctx )
{
    --key;
    assert( key < NUM_ACK_CTX ); 
    assert( _ack_ctx[key].md_h.a != PTL_INVALID_HANDLE );
    *ctx = _ack_ctx[key];
    _ack_ctx[key].md_h.a = PTL_INVALID_HANDLE;
}
