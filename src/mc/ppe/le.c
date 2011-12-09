#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/list_entries.h"

int
le_append_impl( ptl_ppe_t *ctx, ptl_cqe_leappend_t *cmd )
{
    ptl_ppe_le_t     *ppe_le;
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;
    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->le_handle.s.ni ];
    ppe_le = ppe_ni->ppe_le + cmd->le_handle.s.code;
    ppe_le->shared_le = ppe_ni->client_le + cmd->le_handle.s.code;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->le_handle.s.selector,
                    cmd->le_handle.s.code, cmd->le_handle.s.ni );

    ppe_le->xpmem_ptr   = ppe_xpmem_attach( &client->xpmem_segments,
                                    cmd->le.start, cmd->le.length );
    if ( ppe_le->xpmem_ptr == NULL ) {
        perror("ppe_xpmem_attach"); 
        return -1;
    }

    _PtlLEAppend( ppe_ni, cmd->le_handle.a, cmd->pt_index, &cmd->le,
                        cmd->ptl_list, cmd->user_ptr );

    return 0;
}

int
le_unlink_impl( ptl_ppe_t *ctx, ptl_cqe_leunlink_t *cmd )
{
    int              ret;
    ptl_ppe_le_t     *ppe_le;
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;
    ptl_cqe_t *send_entry;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->le_handle.s.selector,
                    cmd->le_handle.s.code, cmd->le_handle.s.ni );

    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->le_handle.s.ni ];
    ppe_le = ppe_ni->ppe_le + cmd->le_handle.s.code;

    ret = ptl_cq_entry_alloc(ctx->cq_h, &send_entry);
    if (ret < 0) {
        perror("ptl_cq_entry_alloc");
        return -1;
    }

    send_entry->base.type = PTLACK;
    send_entry->ack.retval_ptr = cmd->retval_ptr;

    if ( ppe_le->ref_cnt == 0 ) {
        send_entry->ack.retval = _PtlLEUnlink( ppe_ni, cmd->le_handle.a );

        if ( send_entry->ack.retval == PTL_OK ) {

            ret = ppe_xpmem_detach( &client->xpmem_segments, ppe_le->xpmem_ptr );
            if ( ret < 0 ) {
                perror("ppe_xpmem_detach");
                return -1;
            }

            // this is a blocking call do we need to set this?
            // who does the ppe_xpmem_detach and clears this flag for auto unlink
            ppe_le->shared_le->in_use = 0;
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
le_search_impl( ptl_ppe_t *ctx, ptl_cqe_lesearch_t *cmd )
{
    ptl_ppe_ni_t     *ppe_ni;
    ptl_ppe_client_t *client;
    
    client = &ctx->clients[ cmd->base.remote_id ];
    ppe_ni = &client->nis[ cmd->ni_handle.s.ni ];
    
    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ni_handle.s.selector,
                    cmd->ni_handle.s.code, cmd->ni_handle.s.ni );
    _PtlLESearch( ppe_ni, cmd->ni_handle.s.ni, cmd->pt_index, &cmd->le,
                        cmd->ptl_search_op, cmd->user_ptr );
    return 0;
}
