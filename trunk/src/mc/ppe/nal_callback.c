
#include <assert.h>

#include "ppe/nal.h"
#include "ppe/ct.h"

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

    ptl_ppe_me_t *ppe_me = (ptl_ppe_me_t*) ppe_pt->list[PTL_PRIORITY_LIST].head;
    
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

    if ( ! ppe_me ) return 0;

    dm_ctx_t *dm_ctx = malloc( sizeof( *dm_ctx ) );
    assert( dm_ctx );
    dm_ctx->nal_msg_data = nal_msg_data;

    dm_ctx->hdr = *hdr;
    dm_ctx->user_ptr = ppe_me->user_ptr; 
    dm_ctx->u.ppe_me = ppe_me; 

    dm_ctx->iovec.iov_base = ppe_me->xpmem_ptr->data;
    dm_ctx->iovec.iov_len = hdr->length;

    dm_ctx->id = ME_CTX;
    dm_ctx->ni = ppe_ni;

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
    
    return PTL_OK;
}

static inline int lib_md_finalize( dm_ctx_t* dm_ctx )
{
    ptl_ppe_md_t *ppe_md = dm_ctx->u.ppe_md;
    PPE_DBG("\n");

    --ppe_md->ref_cnt;

    if ( ppe_md->ct_h.a != PTL_CT_NONE ) {
        if ( ppe_md->options & PTL_MD_EVENT_CT_SEND ) {
            ct_inc( dm_ctx->ni, ppe_md->ct_h.s.code, 1 );
        }
    }
    return 0;
}

static inline int lib_me_finalize( dm_ctx_t* dm_ctx )
{
    ptl_ppe_me_t *ppe_me = dm_ctx->u.ppe_me;
    PPE_DBG("\n");

    --ppe_me->ref_cnt;

    if ( ppe_me->ct_h.a != PTL_CT_NONE ) {
        if ( ppe_me->options & PTL_ME_EVENT_CT_COMM ) {
            ct_inc( dm_ctx->ni, ppe_me->ct_h.s.code, 1 );
        }
    }

    return 0;
}


int lib_finalize(lib_ni_t *ni, void *lib_msg_data, ptl_ni_fail_t fail_type)
{
    dm_ctx_t *dm_ctx = lib_msg_data;

    if ( dm_ctx->id == ME_CTX ) {
        lib_me_finalize( dm_ctx );
    } else {
        lib_md_finalize( dm_ctx );
    }
    free( lib_msg_data );
    return PTL_OK;
}
