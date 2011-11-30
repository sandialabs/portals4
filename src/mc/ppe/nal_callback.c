
#include <assert.h>

#include "ppe/nal.h"
#include "ppe/ct.h"
#include "ppe/eq.h"
#include "ppe/matching_list_entries.h"

#include "shared/ptl_internal_handles.h"

#include "nal/p3.3/include/p3/process.h"

#include "nal/p3.3/include/p3api/types.h"
#include "nal/p3.3/include/p3lib/types.h"
#include "nal/p3.3/include/p3lib/p3lib.h"
#include "nal/p3.3/include/p3lib/p3lib_support.h"


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

    nal_ctx->u.me.ppe_pt = nal_ctx->ppe_ni->ppe_pt + hdr->pt_index;
    if ( ! nal_ctx->u.me.ppe_pt->status ) {
        PPE_DBG("PT %d not allocated\n",hdr->pt_index);
        goto drop_message;
    }

    PtlInternalMEDeliver( nal_ctx, nal_ctx->u.me.ppe_pt, &nal_ctx->hdr );

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
    ptl_size_t rlen = hdr->length, mlen = 0;;
    PPE_DBG("md_index=%i\n", hdr->md_index);

    // do we use a ref count and keep the md around until acks come back
    // or do we use a inuse flag and drop the ack if the md was free'd
    if (  ! ( hdr->md_index < nal_ctx->ppe_ni->limits->max_mds ) ) {
        PPE_DBG("md index %d is out of range\n", hdr->md_index );
        return 1; 
    }
    nal_ctx->u.md.ppe_md = &ppe_ni->ppe_md[ hdr->md_index ]; 

    nal_ctx->type = MD_CTX;
    nal_ctx->iovec.iov_base = nal_ctx->u.md.ppe_md->xpmem_ptr->data;
    nal_ctx->iovec.iov_len = nal_ctx->u.md.ppe_md->xpmem_ptr->length;

    if ( ( hdr->type & HDR_TYPE_PUT ) == HDR_TYPE_GET ) {
        // we need the Get local offset
        // we need to do bounds checking    
        mlen =  hdr->length;
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

static inline int finalize_md_send( nal_ctx_t* nal_ctx )
{
    ptl_ppe_md_t *ppe_md = nal_ctx->u.md.ppe_md;
    PPE_DBG("\n");
    if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_PUT ) {
        // if no pending ACK 
        if ( nal_ctx->hdr.ack_req == PTL_NO_ACK_REQ) {
            --ppe_md->ref_cnt;
        }

        if ( ppe_md->ct_h.a != PTL_CT_NONE ) {
            if ( ppe_md->options & PTL_MD_EVENT_CT_SEND ) {
                ct_inc( nal_ctx->ppe_ni, ppe_md->ct_h.s.code, 1 );
            }
        }

        if ( ppe_md->eq_h.a != PTL_EQ_NONE ) {
            if ( ! (ppe_md->options & PTL_MD_EVENT_SUCCESS_DISABLE ) ) {
                ptl_event_t event;
                event.type = PTL_EVENT_SEND;
                eq_write( nal_ctx->ppe_ni, ppe_md->eq_h.s.code, &event );
            }
        }
    }
    return 0;
}

static inline int finalize_md_recv( nal_ctx_t* nal_ctx )
{
    ptl_ppe_md_t *ppe_md = nal_ctx->u.md.ppe_md;
    PPE_DBG("\n");

    --ppe_md->ref_cnt;
    if ( ppe_md->ct_h.a != PTL_CT_NONE ) {
        if ( ( ppe_md->options & PTL_MD_EVENT_CT_ACK ) || 
             ( ppe_md->options & PTL_MD_EVENT_CT_REPLY ) ) {
            ct_inc( nal_ctx->ppe_ni, ppe_md->ct_h.s.code, 1 );
        }
    }

    if ( ppe_md->eq_h.a != PTL_EQ_NONE ) {
        if ( ! (ppe_md->options & PTL_MD_EVENT_SUCCESS_DISABLE ) ) {
            ptl_event_t event;
            if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_PUT ) {
                event.type = PTL_EVENT_ACK;
            } else {
                event.type = PTL_EVENT_REPLY;
            }
            eq_write( nal_ctx->ppe_ni, ppe_md->eq_h.s.code, &event );
        }
    }

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

// local is were PUT data goes and where GET data comes from
int lib_me_recv( nal_ctx_t *nal_ctx,
                void *const local_data, const size_t nbytes,
                        const  ptl_internal_header_t *hdr  )
{
    int mlen = 0, rlen = 0;

    PPE_DBG("type=%#xaddr=%p mlength=%lu rlength=%lu\n", hdr->type,
                                local_data, nbytes, hdr->length); 

    nal_ctx->type = ME_CTX;
    nal_ctx->u.me.ppe_me->ref_cnt++;
    nal_ctx->u.me.mlength   = nbytes;
    nal_ctx->iovec.iov_base = nal_ctx->u.me.ppe_me->xpmem_ptr->data + 
            ( local_data - nal_ctx->u.me.ppe_me->visible.start);
    nal_ctx->iovec.iov_len  = nal_ctx->u.me.ppe_me->visible.length;

    if ( ( hdr->type & HDR_TYPE_BASICMASK ) == HDR_TYPE_PUT ) {
        mlen = nal_ctx->u.me.mlength;
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

    PPE_DBG("\n");
    nal_ctx_t *nal_ctx2 = malloc( sizeof(*nal_ctx2) );
    assert(nal_ctx2);

    // do we need to copy the whole thing?
    *nal_ctx2 = *nal_ctx;

    nal_ctx2->hdr.type      |= HDR_TYPE_ACKFLAG;
    nal_ctx2->hdr.src.pid    = nal_ctx->hdr.target.pid;
    nal_ctx2->hdr.target.pid = nal_ctx->hdr.src.pid;

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
                        nal_ctx2->u.me.mlength,   // len
                        NULL            // addrkey
                    ); 
    return 0;
}

static inline int deliver_me_events( nal_ctx_t *nal_ctx )
{
    ptl_ppe_me_t *ppe_me = nal_ctx->u.me.ppe_me;
    uintptr_t start = (nal_ctx->iovec.iov_base - nal_ctx->u.me.ppe_me->xpmem_ptr->data);
    start += (uintptr_t)nal_ctx->u.me.ppe_me->visible.start;
    
    PPE_DBG("\n");
    --ppe_me->ref_cnt;
    PtlInternalAnnounceMEDelivery( nal_ctx, 
                                    nal_ctx->u.me.ppe_pt->EQ, 
                                    ppe_me->visible.ct_handle,
                                    ppe_me->visible.options,
                                    nal_ctx->u.me.mlength,
                                    start,
                                    ppe_me->ptl_list, 
                                    &nal_ctx->u.me.ppe_me->Qentry, //appendME_t
                                    &nal_ctx->hdr,
                                    (ptl_handle_me_t)
                                            ppe_me->Qentry.me_handle.a);
    return 0;
}

static inline int finalize_me_recv( nal_ctx_t* nal_ctx )
{
    PPE_DBG("\n");
    if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_GET ) {
        send_ack( nal_ctx );
    } else {
        deliver_me_events( nal_ctx );
        if ( nal_ctx->hdr.ack_req == PTL_ACK_REQ ) {
            send_ack( nal_ctx );
        }
    }
    return 0;
}
static inline int finalize_me_send( nal_ctx_t* nal_ctx )
{
    PPE_DBG("\n");
    if ( ( nal_ctx->hdr.type & HDR_TYPE_BASICMASK ) == HDR_TYPE_GET ) {
        deliver_me_events( nal_ctx );
    } else {
    }
    return 0;
}


// we can get here for:
//    PUT recv completion
//    GET recv completion
//    PUT-ACK sent completion
//    GET-REPLY sent completion
static inline int finalize_me( nal_ctx_t* nal_ctx )
{
    PPE_DBG("hdr type %#x\n", nal_ctx->hdr.type );

    if ( ! ( nal_ctx->hdr.type & HDR_TYPE_ACKFLAG ) ) {
        return finalize_me_recv( nal_ctx );
    } else {
        return finalize_me_send( nal_ctx );
    } 
}


static inline int finalize_le( nal_ctx_t* nal_ctx )
{
    //ptl_ppe_le_t *ppe_le = nal_ctx->u.le.ppe_le;
    PPE_DBG("\n");

#if 0
    --ppe_le->ref_cnt;

    if ( ppe_le->ct_h.a != PTL_CT_NONE ) {
        if ( ppe_le->options & PTL_ME_EVENT_CT_COMM ) {
            ct_inc( dm_ctx->ppe_ni, ppe_le->ct_h.s.code, 1 );
        }
    }
#endif

    return 0;
}

int lib_finalize(lib_ni_t *ni, void *lib_msg_data, ptl_ni_fail_t fail_type)
{
    nal_ctx_t *nal_ctx = lib_msg_data;

    switch( nal_ctx->type ) 
    {
      case ME_CTX:
        finalize_me( nal_ctx );
        break;

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
