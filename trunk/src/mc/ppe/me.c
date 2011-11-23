#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/pt.h"
#include "ppe/matching_list_entries.h"

int
me_append_impl( ptl_ppe_t *ctx, ptl_cqe_meappend_t *cmd )
{
#if 0
    ptl_ppe_ni_t     *ni;
    ptl_ppe_me_t     *ppe_me;
    ptl_ppe_pt_t     *ppe_pt;
    ptl_ppe_client_t *client;
#endif

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->me_handle.s.selector,
                    cmd->me_handle.s.code, cmd->me_handle.s.ni );

    _PtlMEAppend( cmd->me_handle.a, cmd->pt_index, &cmd->me, cmd->ptl_list,
                            cmd->user_ptr );

#if 0
    client = &ctx->clients[ cmd->base.remote_id ];
    ni     = &client->nis[ cmd->me_handle.s.ni ];
    ppe_me = ni->ppe_me + cmd->me_handle.s.code;
    ppe_pt = ni->ppe_pt + cmd->pt_index;

    ppe_me->pt_index = cmd->pt_index;
    ppe_me->list     = cmd->list;
    ppe_me->user_ptr = cmd->user_ptr;
    ppe_me->ref_cnt  = 0;

    // file in ptl_me_t data 
    ppe_me->ct_h        = (ptl_handle_generic_t)cmd->me.ct_handle;
    ppe_me->uid         = cmd->me.uid; 
    ppe_me->options     = cmd->me.options; 
    ppe_me->match_id    = cmd->me.match_id; 
    ppe_me->match_bits  = cmd->me.match_bits; 
    ppe_me->ignore_bits = cmd->me.ignore_bits; 
    ppe_me->min_free    = cmd->me.min_free; 
    ppe_me->xpmem_ptr   = ppe_xpmem_attach( &client->xpmem_segments,
                                    cmd->me.start, cmd->me.length );
    assert( ppe_me->xpmem_ptr );

    pt_append_me( ppe_pt, cmd->list, ppe_me ); 
#endif

    return 0;
}

int
me_unlink_impl( ptl_ppe_t *ctx, ptl_cqe_meunlink_t *cmd )
{
#if 0
    int                ret;
    ptl_ppe_ni_t      *ni;
    ptl_shared_me_t *shared_me;
    ptl_ppe_me_t      *ppe_me;
    ptl_ppe_pt_t      *ppe_pt;
    ptl_ppe_client_t  *client;
#endif

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->me_handle.s.selector,
                    cmd->me_handle.s.code, cmd->me_handle.s.ni );

    _PtlMEUnlink( cmd->me_handle.a );
#if 0
    client    = &ctx->clients[ cmd->base.remote_id ];
    ni        = &client->nis[ cmd->me_handle.s.ni ];
    ppe_me    = ni->ppe_me + cmd->me_handle.s.code;
    shared_me = ni->client_me + cmd->me_handle.s.code;
    ppe_pt    = ni->ppe_pt + ppe_me->pt_index;

    pt_unlink_me( ppe_pt, ppe_me->list, ppe_me );

    // how do we handle a md that's involved in a xfer 
    assert( ppe_me->ref_cnt == 0 );
    
    // why do we hang when this is called
    ret = ppe_xpmem_detach( &client->xpmem_segments, ppe_me->xpmem_ptr );
    assert( 0 == ret );  

    shared_me->in_use = 0;
#endif
    return 0;
}

int
me_search_impl( ptl_ppe_t *ctx, ptl_cqe_mesearch_t *cmd )
{
#if 0
    ptl_ppe_ni_t     *ni;
    ptl_ppe_me_t     *ppe_me;
    ptl_ppe_client_t *client;
#endif

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ni_handle.s.selector,
                    cmd->ni_handle.s.code, cmd->ni_handle.s.ni );
    _PtlMESearch( cmd->ni_handle.a, cmd->pt_index, &cmd->me,
                        cmd->ptl_search_op, cmd->user_ptr );
    return 0;
}
