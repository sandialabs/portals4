#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

int
pt_alloc_impl( ptl_ppe_t *ctx, ptl_cqe_ptalloc_t *cmd )
{
    ptl_ppe_ni_t      *ni;
    ptl_ppe_client_t  *client;
    ptl_ppe_pt_t      *ppe_pt;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ni_handle.s.selector,
                    cmd->ni_handle.s.code, cmd->ni_handle.s.ni);

    client = &ctx->clients[ cmd->base.remote_id ];
    ni     = &client->nis[ cmd->ni_handle.s.ni ];
    ppe_pt = ni->ppe_pt + cmd->pt_index;

    ppe_pt->eq_handle = (ptl_handle_eq_t)cmd->eq_handle.a;
    ppe_pt->options   = cmd->options;
    ppe_pt->status    = 1;
    
    return 0;
}

int
pt_free_impl( ptl_ppe_t *ctx, ptl_cqe_ptfree_t *cmd )
{
    int                ret;
    ptl_ppe_ni_t      *ni;
    ptl_ppe_client_t  *client;
    ptl_internal_pt_t *shared_pt;
    ptl_ppe_pt_t      *ppe_pt;
    ptl_cqe_t         *send_entry;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->ni_handle.s.selector,
                    cmd->ni_handle.s.code, cmd->ni_handle.s.ni);

    client    = &ctx->clients[ cmd->base.remote_id ];
    ni        = &client->nis[ cmd->ni_handle.s.ni ];
    shared_pt = ni->client_pt + cmd->pt_index;
    ppe_pt = ni->ppe_pt + cmd->pt_index;

    ret = ptl_cq_entry_alloc(ctx->cq_h, &send_entry);
    if (ret < 0) {
        perror("ptl_cq_entry_alloc");
        return -1;
    }

    send_entry->base.type = PTLACK;
    send_entry->ack.retval_ptr = cmd->retval_ptr;

    if ( ppe_pt->priority.head == NULL && ppe_pt->overflow.head == NULL) {
        
        // should we merge flags?
        ppe_pt->status = 1;
        shared_pt->in_use = 0;
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
