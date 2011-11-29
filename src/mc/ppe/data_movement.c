#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/nal.h"
//#include "ppe/data_movement2.h"

int
put_impl( ptl_ppe_t *ctx, ptl_cqe_put_t *cmd )
{
    int                 retval;
    foo_t           *foo;
    ptl_ppe_ni_t       *ppe_ni;
    ptl_ppe_md_t       *ppe_md;
    ptl_process_id_t    dst;

    PPE_DBG("remote_id=%d\n",cmd->base.remote_id);

    ppe_ni = &ctx->clients[cmd->base.remote_id].nis[cmd->md_handle.s.ni];
    ppe_md = ppe_ni->ppe_md + cmd->md_handle.s.code; 

#if 0
    _PtlPut( ppe_ni, 
                cmd->md_handle.a,
                cmd->local_offset,
                cmd->length,
                cmd->ack_req,
                cmd->target_id,
                cmd->pt_index,
                cmd->match_bits,
                cmd->remote_offset,
                cmd->user_ptr,
                cmd->hdr_data );
#endif
                    

    foo = malloc( sizeof( *foo ) );
    foo->u.md.user_ptr            = cmd->user_ptr;
    foo->_hdr.length          = cmd->length;
    foo->_hdr.ack_req         = cmd->ack_req;
    foo->_hdr.src_id.phys.pid   = cmd->base.remote_id;
    foo->_hdr.src_id.phys.nid   = ctx->nid;
    foo->_hdr.target_id       = cmd->target_id;
    foo->_hdr.match_bits      = cmd->match_bits;
    foo->_hdr.remote_offset   = cmd->remote_offset;
    foo->_hdr.pt_index        = cmd->pt_index;
    foo->_hdr.hdr_data        = cmd->hdr_data;
    foo->_hdr.ni              = cmd->md_handle.s.ni;
    foo->_hdr.type            = HDR_TYPE_PUT;

    dst.nid = cmd->target_id.phys.nid;
    dst.pid = cmd->target_id.phys.pid;

    foo->iovec.iov_base = ppe_md->xpmem_ptr->data + cmd->local_offset;
    foo->iovec.iov_len = cmd->length;
    foo->type = MD_CTX;
    foo->u.md.ppe_md = ppe_md;
    foo->ppe_ni = ppe_ni;
    
    ++ppe_md->ref_cnt;

    PPE_DBG("dst nid=%#x pid=%d length=%lu\n",dst.nid,dst.pid, cmd->length );

    retval = ctx->ni.nal->send( &ctx->ni,
                                &foo->nal_msg_data,      // nal_msg_data 
                                foo,                     // lib_data
                                dst,                        // dest 
                                (lib_mem_t *) &foo->_hdr, // hdr 
                                sizeof(foo->_hdr),        // hdrlen 
                                &foo->iovec,             // iov
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
    foo_t           *foo;
    ptl_ppe_ni_t       *ppe_ni;
    ptl_ppe_md_t       *ppe_md;
    ptl_process_id_t    dst;

    PPE_DBG("remote_id=%d\n",cmd->base.remote_id);

    ppe_ni = &ctx->clients[cmd->base.remote_id].nis[cmd->md_handle.s.ni];
    ppe_md = ppe_ni->ppe_md + cmd->md_handle.s.code; 

    foo = malloc( sizeof( *foo ) );
    PPE_DBG("foo=%p\n",foo);
    foo->u.md.user_ptr        = cmd->user_ptr;
    foo->_hdr.length          = cmd->length;
    foo->_hdr.src_id.phys.pid = cmd->base.remote_id;
    foo->_hdr.src_id.phys.nid = ctx->nid;
    foo->_hdr.target_id       = cmd->target_id;
    foo->_hdr.match_bits      = cmd->match_bits;
    foo->_hdr.remote_offset   = cmd->remote_offset;
    foo->_hdr.pt_index        = cmd->pt_index;
    foo->_hdr.ni              = cmd->md_handle.s.ni;
    foo->_hdr.type            = HDR_TYPE_GET;
    foo->_hdr.key             = foo;

    dst.nid = cmd->target_id.phys.nid;
    dst.pid = cmd->target_id.phys.pid;

    foo->iovec.iov_base = ppe_md->xpmem_ptr->data + cmd->local_offset;
    foo->iovec.iov_len = cmd->length;
    foo->type = MD_CTX;
    foo->u.md.ppe_md = ppe_md;
    foo->ppe_ni = ppe_ni;
    
    ++ppe_md->ref_cnt;

    PPE_DBG("dst nid=%#x pid=%d length=%lu\n",dst.nid,dst.pid, cmd->length );

    retval = ctx->ni.nal->send( &ctx->ni,
                                &foo->nal_msg_data,         // nal_msg_data 
                                foo,                        // lib_data
                                dst,                        // dest 
                                (lib_mem_t *) &foo->_hdr,   // hdr 
                                sizeof(foo->_hdr),          // hdrlen 
                                NULL,                       // iov
                                0,                          // iovlen
                                0,                          // offset
                                0,                          // len
                                NULL                        // addrkey
                             );
    return 0;
}
