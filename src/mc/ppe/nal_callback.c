
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
    ptl_process_id_t dst;
    ptl_ppe_t        *ppe_ctx;
    ptl_ppe_client_t *client;
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_pt_t     *ppe_pt;

    PPE_DBG("ni=%d target nid=%#x pid=%d match_bits=%#lx\n", hdr->ni,
                        hdr->target_id.phys.nid, hdr->target_id.phys.pid,
                        hdr->match_bits);
    PPE_DBG("src nid=%#x pid=%d\n",
                 hdr->src_id.phys.nid, hdr->src_id.phys.pid );

    dst.nid = hdr->target_id.phys.nid;
    dst.pid = hdr->target_id.phys.pid;

    ppe_ctx = _p3_ni->data;
    client = &ppe_ctx->clients[ hdr->target_id.phys.pid ];
    ppe_ni = &client->nis[ hdr->ni ];
    ppe_pt = ppe_ni->ppe_pt + hdr->pt_index;

    foo_t *foo = malloc( sizeof( *foo ) ); 

    foo->p3_ni          = _p3_ni;
    foo->ppe_ni         = ppe_ni;
    foo->ppe_pt         = ppe_pt;
    foo->nal_msg_data   = nal_msg_data;

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
#if 0
    
    ptl_ppe_me_t *ppe_me  = NULL;
    ptl_ppe_le_t *ppe_le  = NULL;

    dm_ctx_t *dm_ctx = malloc( sizeof( *dm_ctx ) );
    assert( dm_ctx );



if ((hdr->ni == 0) || (hdr->ni == 2)) { // must be a matching NI

    ppe_me = (ptl_ppe_me_t*) ppe_pt->list[PTL_PRIORITY_LIST].head;
    
    for ( ; ppe_me ; ppe_me = (ptl_ppe_me_t*) ppe_me->base.next ) {
        PPE_DBG( "nid=%#x pid=%d match_bits=%#lx\n", 
                            ppe_me->match_id.phys.nid,
                            ppe_me->match_id.phys.pid,
                            ppe_me->match_bits );

        if (((hdr->match_bits ^ ppe_me->match_bits) & 
                            ~(ppe_me->ignore_bits)) != 0) 
        {
            continue;
        }
        if ( hdr->ni <= 1) {                 // Logical
            if ((ppe_me->match_id.rank != PTL_RANK_ANY) &&
                (ppe_me->match_id.rank != hdr->target_id.rank)) {
                continue;
            }
        } else {                       // Physical 
            if ((ppe_me->match_id.phys.nid != PTL_NID_ANY) &&
                (ppe_me->match_id.phys.nid != hdr->src_id.phys.nid)) {
                continue;
            }
            if ((ppe_me->match_id.phys.pid != PTL_PID_ANY) &&
                (ppe_me->match_id.phys.pid != hdr->src_id.phys.pid)) {
                continue;
            }
        }        
        break;
    }

    if ( ppe_me ) {
        dm_ctx->user_ptr = ppe_me->user_ptr; 
        dm_ctx->iovec.iov_base = ppe_me->xpmem_ptr->data;
        dm_ctx->u.ppe_me = ppe_me; 
        ++dm_ctx->u.ppe_me->ref_cnt;
        dm_ctx->id = ME_CTX;
    }
} else {
    ppe_le = (ptl_ppe_le_t*) ppe_pt->list[PTL_PRIORITY_LIST].head;
    if ( ppe_le ) {
        dm_ctx->user_ptr = ppe_le->user_ptr; 
        dm_ctx->iovec.iov_base = ppe_le->xpmem_ptr->data;
        dm_ctx->u.ppe_le = ppe_le; 
        ++dm_ctx->u.ppe_le->ref_cnt;
        dm_ctx->id = LE_CTX;
    }
}

    if ( ! ppe_me && ! ppe_le ) {
        free ( dm_ctx );
        return 0;
    }

    dm_ctx->nal_msg_data = nal_msg_data;
    dm_ctx->hdr = *hdr;
    dm_ctx->iovec.iov_len = hdr->length;
    dm_ctx->ppe_ni = ppe_ni;
    dm_ctx->ppe_pt = ppe_pt;

    _p3_ni->nal->recv( _p3_ni, 
                        nal_msg_data,
                        dm_ctx,         // lib_data
                        &dm_ctx->iovec, // dst_iov
                        1,              // iovlen
                        0,              // offset
                        hdr->length,    // mlen
                        hdr->length,    // rlen
                        NULL            // addrkey
                    ); 
#endif
    
    return PTL_OK;
}

static inline int lib_md_finalize( dm_ctx_t* dm_ctx )
{
    ptl_ppe_md_t *ppe_md = dm_ctx->u.ppe_md;
    PPE_DBG("\n");

    --ppe_md->ref_cnt;

    if ( ppe_md->ct_h.a != PTL_CT_NONE ) {
        if ( ppe_md->options & PTL_MD_EVENT_CT_SEND ) {
            ct_inc( dm_ctx->ppe_ni, ppe_md->ct_h.s.code, 1 );
        }
    }

    if ( ppe_md->eq_h.a != PTL_EQ_NONE ) {
        if ( ! (ppe_md->options & PTL_MD_EVENT_SUCCESS_DISABLE ) ) {
            ptl_event_t event;
            event.type = PTL_EVENT_SEND;
            eq_write( dm_ctx->ppe_ni, ppe_md->eq_h.s.code, &event );
        }
    }
    return 0;
}

int lib_me_init( foo_t *foo,
                void *const local_data, const size_t nbytes,
                         ptl_internal_header_t *hdr  )
{
    PPE_DBG("dest_addr=%p mlength=%lu rlength=%lu\n",
                                local_data,nbytes,hdr->length); 

    foo->type = ME_CTX;

    foo->mlength = nbytes;
    foo->iovec.iov_base = local_data,
    foo->iovec.iov_len  = nbytes;
    ++foo->u.ppe_me->ref_cnt;

    foo->p3_ni->nal->recv( foo->p3_ni, 
                        foo->nal_msg_data,
                        foo,         // lib_data
                        &foo->iovec, // dst_iov
                        1,              // iovlen
                        0,              // offset
                        foo->mlength,   // mlen
   //                     0,   // mlen
                        hdr->length,    // rlen
                        NULL            // addrkey
                    ); 
    return 0;
}

static inline int lib_me_finalize( foo_t* foo )
{
    ptl_ppe_me_t *ppe_me = foo->u.ppe_me;
    PPE_DBG("\n");

    --ppe_me->ref_cnt;
    PtlInternalAnnounceMEDelivery( foo, 
                                    foo->ppe_pt->EQ, // eq_handle
                                    ppe_me->visible.ct_handle,
                                    ppe_me->visible.options,
                                    foo->mlength, // mlength
                                    (uintptr_t)foo->iovec.iov_base, // start,
                                    ppe_me->ptl_list, // list
                                    &foo->u.ppe_me->Qentry, //appendME_t
                                    &foo->hdr, // hdr 
                                    (ptl_handle_me_t)
                                            ppe_me->Qentry.me_handle.a);

#if 0
    --ppe_me->ref_cnt;

    if ( ppe_me->ct_h.a != PTL_CT_NONE ) {
        if ( ppe_me->options & PTL_ME_EVENT_CT_COMM ) {
            ct_inc( dm_ctx->ppe_ni, ppe_me->ct_h.s.code, 1 );
        }
    }

    if ( ppe_pt->eq_h.a != PTL_EQ_NONE ) {
        if ( ppe_me->options & PTL_ME_EVENT_CT_COMM ) {
            ptl_event_t event;
            event.type = PTL_EVENT_PUT;
            eq_write( dm_ctx->ppe_ni, ppe_pt->eq_h.s.code, &event );
        }
    } 
#endif

    return 0;
}

static inline int lib_le_finalize( dm_ctx_t* dm_ctx )
{
    ptl_ppe_le_t *ppe_le = dm_ctx->u.ppe_le;
    PPE_DBG("\n");

    --ppe_le->ref_cnt;

    if ( ppe_le->ct_h.a != PTL_CT_NONE ) {
        if ( ppe_le->options & PTL_ME_EVENT_CT_COMM ) {
            ct_inc( dm_ctx->ppe_ni, ppe_le->ct_h.s.code, 1 );
        }
    }

    return 0;
}

int lib_finalize(lib_ni_t *ni, void *lib_msg_data, ptl_ni_fail_t fail_type)

{
//    dm_ctx_t *dm_ctx = lib_msg_data;
    foo_t *foo = lib_msg_data;

    PPE_DBG("%d\n",foo->type);
    if ( foo->type == ME_CTX ) {
        lib_me_finalize( foo );
    } else if ( foo->type == LE_CTX ) {
//        lib_le_finalize( dm_ctx );
    } else {
//        lib_md_finalize( dm_ctx );
    }
    //free( lib_msg_data );
    return PTL_OK;
}
