#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/matching_list_entries.h"
#include "ppe/list_entries.h"

int
list_append_impl( ptl_ppe_t *ctx, ptl_cqe_list_append_t *cmd )
{
    ptl_ppe_me_t     *ppe_me;
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;
    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->entry_handle.s.ni ];

    if ( cmd->base.type == PTLMEAPPEND ) {
        ppe_me = ppe_ni->ppe_me + cmd->entry_handle.s.code;
    } else {
        ppe_me = ppe_ni->ppe_le + cmd->entry_handle.s.code;
    }

    ppe_me->shared_le = ppe_ni->client_me + cmd->entry_handle.s.code;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->entry_handle.s.selector,
                    cmd->entry_handle.s.code, cmd->entry_handle.s.ni );

    ppe_me->xpmem_ptr   = ppe_xpmem_attach( &client->xpmem_segments,
                                    cmd->me.le.start, cmd->me.le.length );

    if( ppe_me->xpmem_ptr == NULL ) {
        perror("ppe_xpmem_attach");
        return -1;
    }

    if ( cmd->base.type == PTLMEAPPEND ) {
        _PtlMEAppend( ppe_ni, cmd->entry_handle.a, cmd->pt_index, &cmd->me, 
                        cmd->ptl_list, cmd->user_ptr );
    } else {
        _PtlLEAppend( ppe_ni, cmd->entry_handle.a, cmd->pt_index, &cmd->me.le, 
                        cmd->ptl_list, cmd->user_ptr );
    }

    return 0;
}

int
list_unlink_impl( ptl_ppe_t *ctx, ptl_cqe_list_unlink_t *cmd )
{
    int              ret;
    ptl_ppe_me_t     *ppe_me;
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;
    ptl_cqe_t *send_entry;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->entry_handle.s.selector,
                    cmd->entry_handle.s.code, cmd->entry_handle.s.ni );

    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->entry_handle.s.ni ];

    if ( cmd->base.type == PTLMEUNLINK ) {
        ppe_me = ppe_ni->ppe_me + cmd->entry_handle.s.code;
    } else {
        ppe_me = ppe_ni->ppe_le + cmd->entry_handle.s.code;
    }

    ret = ptl_cq_entry_alloc(ctx->cq_h, &send_entry);
    if (ret < 0) {
        perror("ptl_cq_entry_alloc");
        return -1;
    }

    send_entry->base.type = PTLACK;
    send_entry->ack.retval_ptr = cmd->retval_ptr;

    if ( ppe_me->ref_cnt == 0 ) {
        if ( cmd->base.type == PTLMEUNLINK ) {
            send_entry->ack.retval = _PtlMEUnlink( ppe_ni, cmd->entry_handle.a );
        } else {
            send_entry->ack.retval = _PtlLEUnlink( ppe_ni, cmd->entry_handle.a );
        }
        
        if ( send_entry->ack.retval == PTL_OK ) {
        
            ret = ppe_xpmem_detach( &client->xpmem_segments, ppe_me->xpmem_ptr );
            if ( ret < 0 ) {
                perror("ppe_xpmem_detach");
                return -1;
            }

            ppe_me->shared_le->in_use = 0;
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
list_search_impl( ptl_ppe_t *ctx, ptl_cqe_list_search_t *cmd )
{
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;

    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->ni_handle.s.ni ];

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ni_handle.s.selector,
                    cmd->ni_handle.s.code, cmd->ni_handle.s.ni );

    if ( cmd->base.type == PTLMESEARCH ) {
        _PtlMESearch( ppe_ni, cmd->ni_handle.s.ni, cmd->pt_index, &cmd->me,
                        cmd->ptl_search_op, cmd->user_ptr );
    } else  {
        _PtlLESearch( ppe_ni, cmd->ni_handle.s.ni, cmd->pt_index, &cmd->me.le,
                        cmd->ptl_search_op, cmd->user_ptr );
    }
    return 0;
}
