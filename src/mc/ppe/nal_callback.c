
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



int lib_parse(ptl_hdr_t *hdr, unsigned long nal_msg_data,
          ptl_interface_t type, ptl_size_t *drop_len)
{
    ptl_ppe_t        *ppe_ctx = _p3_ni->data;
    ptl_ppe_client_t *client;
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_pt_t     *ppe_pt;

    PPE_DBG("ni=%d src=%#x,%d dst=%#x,%d match_bits=%#lx\n", hdr->ni,
                        hdr->src_id.phys.nid, hdr->src_id.phys.pid,
                        hdr->target_id.phys.nid, hdr->target_id.phys.pid,
                        hdr->match_bits);

    // get this now so it can be used for a dropped message
    foo_t *foo = malloc( sizeof( *foo ) ); 
    assert(foo);
    foo->nal_msg_data   = nal_msg_data;

    if ( ! (hdr->target_id.phys.pid < MC_PEER_COUNT ) ) {
        PPE_DBG("pid %d out of range\n", hdr->target_id.phys.pid );
        goto drop_message;
    }

    client = &ppe_ctx->clients[ hdr->target_id.phys.pid ];
    if (  ! client->connected ) {
        PPE_DBG("pid %d not connected\n", hdr->target_id.phys.pid );
        goto drop_message;
    }

    if ( ! ( hdr->ni < 4 ) ) {
        PPE_DBG("ni %d out of range\n", hdr->ni );
        goto drop_message;
    }

    ppe_ni = &client->nis[ hdr->ni ];
    if ( ! ppe_ni->limits ) {
        PPE_DBG("ni %d not initialized\n", hdr->ni );
        goto drop_message;
    }

    // MJL: range check pt_index 
    if ( ! ( hdr->pt_index <  ppe_ni->limits->max_pt_index ) ) {
        PPE_DBG("PT %d out of range\n",hdr->pt_index);
        goto drop_message;
    }

    ppe_pt = ppe_ni->ppe_pt + hdr->pt_index;
    if ( ! ppe_pt->status ) {
        PPE_DBG("PT %d not allocated\n",hdr->pt_index);
        goto drop_message;
    }

    foo->p3_ni          = _p3_ni;
    foo->ppe_ni         = ppe_ni;
    foo->u.me.ppe_pt         = ppe_pt;

    foo->hdr.match_bits = hdr->match_bits;
    foo->hdr.hdr_data   = hdr->hdr_data;
    foo->hdr.remaining  = hdr->length;
    foo->hdr.ni         = hdr->ni;
    foo->hdr.src        = hdr->src_id.phys.pid;
    foo->hdr.length     = hdr->length;
    foo->hdr.type       = hdr->type;
    foo->hdr.dest_offset = hdr->remote_offset;
    foo->hdr.entry      = NULL;
    
    PtlInternalMEDeliver( foo, ppe_pt, &foo->hdr );

    return 0;

drop_message:
    PPE_DBG("Drop message\n");

    foo->type = DROP_CTX;

    _p3_ni->nal->recv( _p3_ni, 
                        foo->nal_msg_data,  // nal_msg_data,
                        foo,                // lib_data
                        NULL,               // dst_iov
                        0,                  // iovlen
                        0,                  // offset
                        0,                  // mlen
                        hdr->length,        // rlen
                        NULL                // addrkey
                    ); 
    return 0;
}
    

static inline int lib_md_finalize( foo_t* foo )
{
    ptl_ppe_md_t *ppe_md = foo->u.md.ppe_md;
    PPE_DBG("\n");

    --ppe_md->ref_cnt;

    if ( ppe_md->ct_h.a != PTL_CT_NONE ) {
        if ( ppe_md->options & PTL_MD_EVENT_CT_SEND ) {
            ct_inc( foo->ppe_ni, ppe_md->ct_h.s.code, 1 );
        }
    }

    if ( ppe_md->eq_h.a != PTL_EQ_NONE ) {
        if ( ! (ppe_md->options & PTL_MD_EVENT_SUCCESS_DISABLE ) ) {
            ptl_event_t event;
            event.type = PTL_EVENT_SEND;
            eq_write( foo->ppe_ni, ppe_md->eq_h.s.code, &event );
        }
    }
    return 0;
}

int lib_me_init( foo_t *foo,
                void *const local_data, const size_t nbytes,
                        const  ptl_internal_header_t *hdr  )
{
    PPE_DBG("dest_addr=%p mlength=%lu rlength=%lu\n",
                                local_data,nbytes,hdr->length); 

    foo->type = ME_CTX;

    foo->u.me.mlength   = nbytes;
    foo->iovec.iov_base = foo->u.me.ppe_me->xpmem_ptr->data + 
            ( local_data - foo->u.me.ppe_me->visible.start);
    foo->iovec.iov_len  = nbytes;
    ++foo->u.me.ppe_me->ref_cnt;

    foo->p3_ni->nal->recv( foo->p3_ni, 
                        foo->nal_msg_data,
                        foo,            // lib_data
                        &foo->iovec,    // dst_iov
                        1,              // iovlen
                        0,              // offset
                        foo->u.me.mlength,   // mlen
                        hdr->length,    // rlen
                        NULL            // addrkey
                    ); 
    return 0;
}

static inline int lib_me_finalize( foo_t* foo )
{
    ptl_ppe_me_t *ppe_me = foo->u.me.ppe_me;

    PPE_DBG("\n");

    uintptr_t start = (foo->iovec.iov_base - foo->u.me.ppe_me->xpmem_ptr->data);
    start += (uintptr_t)foo->u.me.ppe_me->visible.start;
    
    --ppe_me->ref_cnt;
    PtlInternalAnnounceMEDelivery( foo, 
                                    foo->u.me.ppe_pt->EQ, 
                                    ppe_me->visible.ct_handle,
                                    ppe_me->visible.options,
                                    foo->u.me.mlength,
                                    start,
                                    ppe_me->ptl_list, 
                                    &foo->u.me.ppe_me->Qentry, //appendME_t
                                    &foo->hdr,
                                    (ptl_handle_me_t)
                                            ppe_me->Qentry.me_handle.a);

    return 0;
}

static inline int lib_le_finalize( foo_t* foo )
{
    //ptl_ppe_le_t *ppe_le = foo->u.le.ppe_le;
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
    foo_t *foo = lib_msg_data;

    switch( foo->type ) 
    {
      case ME_CTX:
        lib_me_finalize( foo );
        break;
      case LE_CTX:
        lib_le_finalize( foo );
        break;
      case MD_CTX:
        lib_md_finalize( foo );
        break;
      case DROP_CTX:
        PPE_DBG("DROP_CTX\n");
        break;

      default:
        assert(0);
    }
    free( lib_msg_data );
    return PTL_OK;
}
