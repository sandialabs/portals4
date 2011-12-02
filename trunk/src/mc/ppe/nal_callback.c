
#include <assert.h>

#include "ppe/nal.h"
#include "ppe/ct.h"
#include "ppe/eq.h"
#include "ppe/matching_list_entries.h"
#include "ppe/list_entries.h"

#include "shared/ptl_internal_handles.h"

#include "nal/p3.3/include/p3/process.h"

#include "nal/p3.3/include/p3api/types.h"
#include "nal/p3.3/include/p3lib/types.h"
#include "nal/p3.3/include/p3lib/p3lib.h"
#include "nal/p3.3/include/p3lib/p3lib_support.h"

#include "ppe/data_movement.h"

int lib_le_recv( nal_ctx_t *nal_ctx,
                void *const local_data, const size_t nbytes,
                        const  ptl_internal_header_t *hdr  );

static int process_ack( ptl_ppe_ni_t *, nal_ctx_t *,ptl_hdr_t * );

int lib_parse( ptl_nid_t src_nid, ptl_hdr_t *hdr, unsigned long nal_msg_data,
          ptl_interface_t type, ptl_size_t *drop_len)
{
    ptl_ppe_t        *ppe_ctx = _p3_ni->data;
    ptl_ppe_client_t *client;

    PPE_DBG("ni=%d src=%#x,%d dst=%d type=%#x match_bits=%#lx\n", hdr->ni,
                        src_nid, hdr->src.pid, hdr->target.pid,
                        hdr->type, hdr->match_bits);

    nal_ctx_t *nal_ctx = malloc( sizeof( *nal_ctx ) ); 
    assert(nal_ctx);
    nal_ctx->nal_msg_data   = nal_msg_data;
    nal_ctx->p3_ni          = _p3_ni;
    nal_ctx->src_nid        = src_nid;
    nal_ctx->hdr            = *hdr;

    if ( ! (hdr->target.pid < MC_PEER_COUNT ) ) {
        PPE_DBG("pid %d out of range\n", hdr->target.pid );
        goto drop_message;
    }

    client = &ppe_ctx->clients[ hdr->target.pid ];
    if (  ! client->connected ) {
        PPE_DBG("pid %d not connected\n", hdr->target.pid );
        goto drop_message;
    }

    if ( ! ( hdr->ni < 4 ) ) {
        PPE_DBG("ni %d out of range\n", hdr->ni );
        goto drop_message;
    }

    nal_ctx->ppe_ni = &client->nis[ hdr->ni ];
    if ( ! nal_ctx->ppe_ni->limits ) {
        PPE_DBG("ni %d not initialized\n", hdr->ni );
        goto drop_message;
    }

    if ( hdr->type & HDR_TYPE_ACKFLAG ) {
        if ( process_ack( nal_ctx->ppe_ni, nal_ctx, hdr ) ) {
            PPE_DBG("drop ACK\n");
            goto drop_message;
        } 
        return 0;
    }

    if ( ! ( hdr->pt_index <  nal_ctx->ppe_ni->limits->max_pt_index ) ) {
        PPE_DBG("PT %d out of range\n",hdr->pt_index);
        goto drop_message;
    }

    ptl_ppe_pt_t *ppe_pt = nal_ctx->ppe_ni->ppe_pt + hdr->pt_index;
    if ( ! ppe_pt->status ) {
        PPE_DBG("PT %d not allocated\n",hdr->pt_index);
        goto drop_message;
    }

    switch( hdr->ni ) {
      case 0:
      case 2:
        PtlInternalMEDeliver( nal_ctx, ppe_pt, &nal_ctx->hdr );
        break;

      case 1:
      case 3:
        PtlInternalLEDeliver( nal_ctx, ppe_pt, &nal_ctx->hdr );
        break;
    }
    PPE_DBG("\n");

    return 0;

drop_message:
    PPE_DBG("Drop message\n");

    nal_ctx->type = DROP_CTX;

    _p3_ni->nal->recv( _p3_ni, 
                        nal_ctx->nal_msg_data,  // nal_msg_data,
                        nal_ctx,                // lib_data
                        NULL,               // dst_iov
                        0,                  // iovlen
                        0,                  // offset
                        0,                  // mlen
                        hdr->length,        // rlen
                        NULL                // addrkey
                    ); 
    return 0;
}
    

static int process_ack( ptl_ppe_ni_t *ppe_ni, nal_ctx_t *nal_ctx, ptl_hdr_t *hdr )
{
    ptl_size_t rlen = hdr->length, mlen = 0, offset = 0;
    ack_ctx_t ack_ctx;

    PPE_DBG("type=%#x ack_ctx_key=%d\n",hdr->type, hdr->ack_ctx_key);

    if ( hdr->ack_ctx_key == 0 ) {
        PPE_DBG("invalid ack_ctx_key %d\n", hdr->ack_ctx_key );
        return 1; 
    }

    if ( hdr->ack_ctx_key < 0 ) {
        free_ack_ctx( hdr->ack_ctx_key * 1, &ack_ctx );
        return 1;
    } else {
        free_ack_ctx( hdr->ack_ctx_key, &ack_ctx );
    }

    nal_ctx->type = MD_CTX;
    nal_ctx->u.md.user_ptr = ack_ctx.user_ptr;

    if ( ( hdr->type & HDR_TYPE_BASICMASK ) == HDR_TYPE_GET ) {


        int md_index = ack_ctx.md_h.s.code;
        // MJL: do we use a ref count and keep the md around until acks come back
        // or do we use a inuse flag and drop the ack if the md was free'd
        if (  ! ( md_index < nal_ctx->ppe_ni->limits->max_mds ) ) {
            PPE_DBG("md index %d is out of range\n", md_index );
            return 1; 
        }
        nal_ctx->u.md.ppe_md = &ppe_ni->ppe_md[ md_index ]; 

        offset = ack_ctx.local_offset;

        if ( offset + mlen > nal_ctx->u.md.ppe_md->xpmem_ptr->length ) {
            PPE_DBG("local_offest=%lu mlen=%lu me_length=%lu\n",  offset,
                    mlen, nal_ctx->u.md.ppe_md->xpmem_ptr->length );
            return 1;
        }
        mlen =  hdr->length;

        nal_ctx->iovec.iov_base = nal_ctx->u.md.ppe_md->xpmem_ptr->data;
        nal_ctx->iovec.iov_len = nal_ctx->u.md.ppe_md->xpmem_ptr->length;
        PPE_DBG("iov_base=%p\n",nal_ctx->iovec.iov_base);
    }

    PPE_DBG("mlen=%lu rlen=%lu offset=%lu\n",mlen,rlen,offset);

    nal_ctx->p3_ni->nal->recv( nal_ctx->p3_ni, 
                        nal_ctx->nal_msg_data,
                        nal_ctx,            // lib_data
                        &nal_ctx->iovec,    // dst_iov
                        1,              // iovlen
                        offset,              // offset
                        mlen,           // mlen
                        rlen,           // rlen
                        NULL            // addrkey
                    ); 
    return 0;
}

static inline int finalize_md_send( nal_ctx_t* nal_ctx )
{
    ptl_ppe_md_t *ppe_md = nal_ctx->u.md.ppe_md;
    PPE_DBG("\n");
    if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_PUT ) {

        --ppe_md->ref_cnt;

        if ( ppe_md->ct_h.a != PTL_CT_NONE ) {
            if ( ppe_md->options & PTL_MD_EVENT_CT_SEND ) {
                
                if ( ( ppe_md->options & PTL_MD_EVENT_CT_BYTES ) == 0 ) {
                    PtlInternalCTSuccessInc( nal_ctx->ppe_ni, ppe_md->ct_h.a, 1 );
                } else {
                    PtlInternalCTSuccessInc( nal_ctx->ppe_ni, ppe_md->ct_h.a, 
                                                    nal_ctx->hdr.length );
                }
            }
        }

        if ( ppe_md->eq_h.a != PTL_EQ_NONE ) {
            if ( ! (ppe_md->options & PTL_MD_EVENT_SUCCESS_DISABLE ) ) {
                PtlInternalEQPushESEND( nal_ctx->ppe_ni, ppe_md->eq_h.a,
                            nal_ctx->hdr.length,
                            nal_ctx->hdr.dest_offset,
                            nal_ctx->u.md.user_ptr );
            }
        }
    }
    return 0;
}

static inline int finalize_md_recv( nal_ctx_t* nal_ctx )
{
    ptl_ppe_md_t *ppe_md = nal_ctx->u.md.ppe_md;
    PPE_DBG("\n");

    if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_GET ) {
        --ppe_md->ref_cnt;
    }
    PPE_DBG("\n");

    if ( ppe_md->ct_h.a != PTL_CT_NONE ) {
        if ( ( ppe_md->options & PTL_MD_EVENT_CT_ACK ) || 
             ( ppe_md->options & PTL_MD_EVENT_CT_REPLY ) ) {
            if ( ( ppe_md->options & PTL_MD_EVENT_CT_BYTES ) == 0 ) {
                PtlInternalCTSuccessInc( nal_ctx->ppe_ni, ppe_md->ct_h.a, 1 );
            } else {
                PtlInternalCTSuccessInc( nal_ctx->ppe_ni, ppe_md->ct_h.a, 
                                                    nal_ctx->hdr.length );
            }
        }
    }

    PPE_DBG("\n");
    if ( ppe_md->eq_h.a != PTL_EQ_NONE ) {
        if ( ! (ppe_md->options & PTL_MD_EVENT_SUCCESS_DISABLE ) ) {
            ptl_event_t event;
             
            if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_PUT ) {
                event.type = PTL_EVENT_ACK;
            } else {
                event.type = PTL_EVENT_REPLY;
            }
            event.mlength       = nal_ctx->hdr.length;
            event.remote_offset = nal_ctx->hdr.dest_offset;
            event.user_ptr      = nal_ctx->u.md.user_ptr;
            event.ni_fail_type  = PTL_NI_OK;
            PtlInternalEQPush( nal_ctx->ppe_ni, ppe_md->eq_h.a, &event );
    PPE_DBG("\n");
        }
    }
    PPE_DBG("\n");

    return 0;
}

static inline int finalize_md( nal_ctx_t* nal_ctx )
{
    PPE_DBG("type %#x\n",nal_ctx->hdr.type);

    if ( ! ( nal_ctx->hdr.type & HDR_TYPE_ACKFLAG ) ) {
        return finalize_md_send( nal_ctx );
    } else  {
        return finalize_md_recv( nal_ctx );
    }
}

// "local_data" is were PUT data goes and where GET data comes from
int lib_le_recv( nal_ctx_t *nal_ctx,
                void *const local_data, const size_t nbytes,
                        const  ptl_internal_header_t *hdr  )
{
    int mlen = 0, rlen = 0;

    PPE_DBG("type=%#x addr=%p mlength=%lu rlength=%lu\n", hdr->type,
                                local_data, nbytes, hdr->length); 

    nal_ctx->type = LE_CTX;
    nal_ctx->u.le.ppe_le->ref_cnt++;
    nal_ctx->u.le.mlength   = nbytes;
    nal_ctx->iovec.iov_base = nal_ctx->u.le.ppe_le->xpmem_ptr->data + 
            ( local_data - nal_ctx->u.le.ppe_le->visible.start);
    nal_ctx->iovec.iov_len  = nal_ctx->u.le.ppe_le->visible.length;

    if ( ( hdr->type & HDR_TYPE_BASICMASK ) == HDR_TYPE_PUT ) {
        mlen = nal_ctx->u.le.mlength;
        rlen = hdr->length;
    }

    nal_ctx->p3_ni->nal->recv( nal_ctx->p3_ni, 
                        nal_ctx->nal_msg_data,
                        nal_ctx,            // lib_data
                        &nal_ctx->iovec,    // dst_iov
                        1,              // iovlen
                        0,              // offset
                        mlen,           // mlen
                        rlen,           // rlen
                        NULL            // addrkey
                    ); 
    return 0;
}


static inline int send_ack( nal_ctx_t *nal_ctx )
{
    ptl_process_id_t dst;

    nal_ctx_t *nal_ctx2 = malloc( sizeof(*nal_ctx2) );
    assert(nal_ctx2);

    // MJL: do we need to copy the whole thing?
    *nal_ctx2 = *nal_ctx;

    PPE_DBG("ack_ctx_key=%d\n",nal_ctx->hdr.ack_ctx_key);
    nal_ctx2->hdr.type      |= HDR_TYPE_ACKFLAG;
    nal_ctx2->hdr.src.pid    = nal_ctx->hdr.target.pid;
    nal_ctx2->hdr.target.pid = nal_ctx->hdr.src.pid;
    nal_ctx2->hdr.length     = nal_ctx->u.le.mlength;

    PPE_DBG("%#x\n",nal_ctx->u.le.ppe_le->visible.options);
    if ( nal_ctx->u.le.ppe_le->visible.options & PTL_ME_ACK_DISABLE ) {
        // a negative key tells the initiator to free the ack_ctx without 
        // generating events 
        nal_ctx2->hdr.ack_ctx_key *= -1;
        PPE_DBG("ME disable ack %#x\n",nal_ctx2->hdr.type);
    }

    dst.nid = nal_ctx->src_nid;
    dst.pid = nal_ctx2->hdr.src.pid;

    PPE_DBG("send %#x to %#x,%d\n", nal_ctx2->hdr.type, dst.nid, dst.pid );
    
    nal_ctx2->p3_ni->nal->send( nal_ctx2->p3_ni, 
                        &nal_ctx2->nal_msg_data,
                        nal_ctx2,            // lib_data
                        dst,
                        (lib_mem_t*) &nal_ctx2->hdr,
                        sizeof(nal_ctx2->hdr),
                        &nal_ctx2->iovec,    // dst_iov
                        1,              // iovlen
                        0,              // offset
                        nal_ctx2->u.le.mlength,   // len
                        NULL            // addrkey
                    ); 
    return 0;
}

static inline int deliver_le_events( nal_ctx_t *nal_ctx )
{
    ptl_ppe_me_t *ppe_me = nal_ctx->u.le.ppe_le;
    uintptr_t start = (nal_ctx->iovec.iov_base - nal_ctx->u.le.ppe_le->xpmem_ptr->data);
    start += (uintptr_t)nal_ctx->u.le.ppe_le->visible.start;

    PPE_DBG("\n");

    --ppe_me->ref_cnt;

    if ( nal_ctx->u.le.ppe_le->Qentry.unlinked ) {
        PPE_DBG("unlinked me=%#x\n",ppe_me->Qentry.handle.a);
        nal_ctx->u.le.ppe_le->shared_le->in_use = 0;
    }
    ptl_ppe_pt_t *ppe_pt = nal_ctx->ppe_ni->ppe_pt + 
                                nal_ctx->u.le.ppe_le->pt_index;
    PtlInternalAnnounceMEDelivery( nal_ctx, 
                                    ppe_pt->EQ, 
                                    ppe_me->visible.ct_handle,
                                    ppe_me->visible.options,
                                    nal_ctx->u.le.mlength,
                                    start,
                                    ppe_me->ptl_list, 
                                    nal_ctx->u.le.ppe_le->Qentry.user_ptr, 
                                    &nal_ctx->hdr );
    return 0;
}

static inline int finalize_le_recv( nal_ctx_t* nal_ctx )
{
    PPE_DBG("hdr.type=%#x\n",nal_ctx->hdr.type);
    if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_GET ) {
        send_ack( nal_ctx );
    } else {
        deliver_le_events( nal_ctx );

        if ( nal_ctx->hdr.ack_req == PTL_ACK_REQ ) {
            send_ack( nal_ctx );
        }
    }
    return 0;
}
static inline int finalize_le_send( nal_ctx_t* nal_ctx )
{
    PPE_DBG("\n");
    if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_GET ) {
        deliver_le_events( nal_ctx );
    } else {
    }
    return 0;
}


// we can get here for:
//    PUT recv completion
//    GET recv completion
//    PUT-ACK sent completion
//    GET-REPLY sent completion
static inline int finalize_le( nal_ctx_t* nal_ctx )
{
    PPE_DBG("hdr type %#x\n", nal_ctx->hdr.type );

    if ( ! ( nal_ctx->hdr.type & HDR_TYPE_ACKFLAG ) ) {
        return finalize_le_recv( nal_ctx );
    } else {
        return finalize_le_send( nal_ctx );
    } 
}

int lib_finalize(lib_ni_t *ni, void *lib_msg_data, ptl_ni_fail_t fail_type)
{
    nal_ctx_t *nal_ctx = lib_msg_data;

    switch( nal_ctx->type ) 
    {
      case LE_CTX:
        finalize_le( nal_ctx );
        break;

      case MD_CTX:
        finalize_md( nal_ctx );
        break;

      case DROP_CTX:
        PPE_DBG("DROP_CTX\n");
        break;

      default:
        assert(0);
    }

    free( nal_ctx );
    return PTL_OK;
}
