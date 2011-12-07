#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/nal.h"
#include "ppe/data_movement.h"

int
data_movement_impl( ptl_ppe_t *ctx, ptl_cqe_data_movement_t *cmd )
{
    int                 retval;
    nal_ctx_t           *nal_ctx;
    ptl_ppe_ni_t       *ppe_ni;
    ptl_ppe_md_t       *ppe_md;
    ptl_process_id_t    dst;
    ptl_size_t          nal_offset = 0, nal_len = 0;
    int                 ack = PTL_NO_ACK_REQ;

    PPE_DBG("remote_id=%d type=%d\n",cmd->base.remote_id,cmd->base.type);

    ppe_ni = &ctx->clients[cmd->base.remote_id].nis[cmd->args.md_handle.s.ni];
    ppe_md = ppe_ni->ppe_md + cmd->args.md_handle.s.code; 

    nal_ctx = malloc( sizeof( *nal_ctx ) );
    assert(nal_ctx);

    nal_ctx->type           = MD_CTX;
    nal_ctx->ppe_ni         = ppe_ni;
    nal_ctx->u.md.ppe_md    = ppe_md;
    nal_ctx->u.md.user_ptr  = cmd->args.user_ptr;
    nal_ctx->iovec.iov_base = ppe_md->xpmem_ptr->data;
    nal_ctx->iovec.iov_len  = ppe_md->xpmem_ptr->length;

    // MJL who should do the phys vs logical check app or engine? 
    nal_ctx->hdr.src.pid         = cmd->base.remote_id;
    nal_ctx->hdr.target.pid      = cmd->args.target_id.phys.pid;

    nal_ctx->hdr.length          = cmd->args.length;
    nal_ctx->hdr.match_bits      = cmd->args.match_bits;
    nal_ctx->hdr.dest_offset     = cmd->args.remote_offset;
    nal_ctx->hdr.remaining       = cmd->args.length;
    nal_ctx->hdr.pt_index        = cmd->args.pt_index;
    nal_ctx->hdr.hdr_data        = cmd->args.hdr_data;
    nal_ctx->hdr.ni              = cmd->args.md_handle.s.ni;
    nal_ctx->hdr.atomic_operation = cmd->atomic_args.operation;
    nal_ctx->hdr.atomic_datatype  = cmd->atomic_args.datatype;

    // MJL: can we structure things such that the cmd type can be used in the
    // ptl hdr? 
    switch ( cmd->base.type ) {
      case PTLPUT: 
        nal_ctx->hdr.type   = HDR_TYPE_PUT;
        ack = cmd->args.ack_req;
        break;
      case PTLGET: 
        nal_ctx->hdr.type   = HDR_TYPE_GET;
        ack = PTL_ACK_REQ;
        break;
      case PTLATOMIC: 
        nal_ctx->hdr.type   = HDR_TYPE_ATOMIC;
        ack = cmd->args.ack_req;
        break;
      case PTLFETCHATOMIC: 
        nal_ctx->hdr.type   = HDR_TYPE_FETCHATOMIC;
        ack = PTL_ACK_REQ;
        break;
      case PTLSWAP: 
        nal_ctx->hdr.type   = HDR_TYPE_SWAP;
        ack = PTL_ACK_REQ;
        break;
    }

    if ( cmd->base.type != PTLGET ) {
        nal_ctx->hdr.ack_req         = cmd->args.ack_req;
        nal_len    = cmd->args.length;
        nal_offset = cmd->args.local_offset;
    }

    if ( ack == PTL_ACK_REQ ) {
        nal_ctx->hdr.ack_ctx_key = 
            alloc_ack_ctx( cmd->args.md_handle, cmd->args.local_offset, 
                    cmd->args.user_ptr );
        assert( nal_ctx->hdr.ack_ctx_key ); 
    }

    dst.nid = cmd->args.target_id.phys.nid;
    dst.pid = cmd->args.target_id.phys.pid;
    
    ++ppe_md->ref_cnt;

    PPE_DBG("dst nid=%#x pid=%d length=%lu\n",dst.nid,dst.pid, 
                                                    cmd->args.length );
    PPE_DBG("nal_offset=%lu nal_len=%lu\n",nal_offset,nal_len);

    retval = ctx->ni.nal->send( &ctx->ni,
                                &nal_ctx->nal_msg_data,      // nal_msg_data 
                                nal_ctx,                     // lib_data
                                dst,                        // dest 
                                (lib_mem_t *) &nal_ctx->hdr, // hdr 
                                sizeof(nal_ctx->hdr),        // hdrlen 
                                &nal_ctx->iovec,             // iov
                                1,                          // iovlen
                                nal_offset,                 // offset
                                nal_len,                    // len
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
