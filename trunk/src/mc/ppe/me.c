#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/pt.h"
#include "ppe/matching_list_entries.h"

int
me_append_impl( ptl_ppe_t *ctx, ptl_cqe_meappend_t *cmd )
{
    ptl_ppe_me_t     *ppe_me;
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;
    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->me_handle.s.ni ];
    ppe_me = ppe_ni->ppe_me + cmd->me_handle.s.code;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->me_handle.s.selector,
                    cmd->me_handle.s.code, cmd->me_handle.s.ni );

    ppe_me->xpmem_ptr   = ppe_xpmem_attach( &client->xpmem_segments,
                                    cmd->me.start, cmd->me.length );
    assert( ppe_me->xpmem_ptr );

    _PtlMEAppend( ppe_ni, cmd->me_handle.a, cmd->pt_index, &cmd->me, 
                        cmd->ptl_list, cmd->user_ptr );

    return 0;
}

int
me_unlink_impl( ptl_ppe_t *ctx, ptl_cqe_meunlink_t *cmd )
{
    int              ret;
    ptl_shared_me_t *shared_me;
    ptl_ppe_me_t     *ppe_me;
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;
    ptl_cqe_t *send_entry;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->me_handle.s.selector,
                    cmd->me_handle.s.code, cmd->me_handle.s.ni );

    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->me_handle.s.ni ];
    ppe_me = ppe_ni->ppe_me + cmd->me_handle.s.code;
    shared_me = ppe_ni->client_me + cmd->me_handle.s.code;

    ret = ptl_cq_entry_alloc(ctx->cq_h, &send_entry);
    if (ret < 0) {
        perror("ptl_cq_entry_alloc");
        return -1;
    }

    send_entry->base.type = PTLACK;
    send_entry->ack.retval_ptr = cmd->retval_ptr;
    if ( ppe_me->ref_cnt == 0 ) {
        send_entry->ack.retval = _PtlMEUnlink( ppe_ni, cmd->me_handle.a );
        
        if ( send_entry->ack.retval == PTL_OK ) {
        
            ret = ppe_xpmem_detach( &client->xpmem_segments, ppe_me->xpmem_ptr );
            assert( 0 == ret );  

            // this is a blocking call do we need to set this?
            // who does the ppe_xpmem_detach and clears this flag for auto unlink
            shared_me->in_use = 0;
        }
    } else {
        send_entry->ack.retval = PTL_IN_USE;
    }

    ret = ptl_cq_entry_send(ctx->cq_h, cmd->base.remote_id,
                            send_entry, sizeof(ptl_cqe_t));
    if (ret < 0) {
        perror("ptl_cq_entry_send");
        return -1;
    }

    return 0;
}

int
me_search_impl( ptl_ppe_t *ctx, ptl_cqe_mesearch_t *cmd )
{
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;

    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->ni_handle.s.ni ];

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ni_handle.s.selector,
                    cmd->ni_handle.s.code, cmd->ni_handle.s.ni );
    _PtlMESearch( ppe_ni, cmd->ni_handle.s.ni, cmd->pt_index, &cmd->me,
                        cmd->ptl_search_op, cmd->user_ptr );
    return 0;
}
