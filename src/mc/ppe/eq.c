#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/eq.h"

int
eq_alloc_impl( ptl_ppe_t *ctx, ptl_cqe_eqalloc_t *cmd )
{
    ptl_ppe_ni_t     *ni;
    ptl_ppe_eq_t     *ppe_eq;
    ptl_ppe_client_t *client;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->eq_handle.s.selector,
                    cmd->eq_handle.s.code, cmd->eq_handle.s.ni);

    client = &ctx->clients[ cmd->base.remote_id ];
    ni     = &client->nis[ cmd->eq_handle.s.ni ];
    ppe_eq = ni->ppe_eq + cmd->eq_handle.s.code;

    ppe_eq->xpmem_ptr = ppe_xpmem_attach( &client->xpmem_segments,
                 cmd->cb, sizeof(ptl_circular_buffer_t)  +
                        sizeof(ptl_event_t) * cmd->count );

    return 0;
}

int
eq_free_impl( ptl_ppe_t *ctx, ptl_cqe_eqfree_t *cmd )
{
    int ret;
    ptl_cqe_t *send_entry;
    int peer = cmd->base.remote_id;    
    ptl_ppe_ni_t     *ni;
    ptl_ppe_eq_t     *ppe_eq;
    ptl_ppe_client_t *client;

    PPE_DBG("selector=%d code=%d ni=%d\n", cmd->eq_handle.s.selector,
                    cmd->eq_handle.s.code, cmd->eq_handle.s.ni);

    // MJL: how do we coordiate with pending data xfers that will want to
    // use this EQ?
    client = &ctx->clients[ cmd->base.remote_id ];
    ni     = &client->nis[ cmd->eq_handle.s.ni ];
    ppe_eq = ni->ppe_eq + cmd->eq_handle.s.code;

    ret = ppe_xpmem_detach( &client->xpmem_segments, ppe_eq->xpmem_ptr );
    if (ret < 0) {
        perror("ppe_xpmem_detach");
        return -1;
    }

    ret = ptl_cq_entry_alloc(ctx->cq_h, &send_entry);
    if (ret < 0) {
        perror("ptl_cq_entry_alloc");
        return -1;
    }

    send_entry->base.type = PTLACK;
    send_entry->ack.retval_ptr = cmd->retval_ptr;
    send_entry->ack.retval = PTL_OK;

    ret = ptl_cq_entry_send(ctx->cq_h, peer,
                            send_entry, sizeof(ptl_cqe_t));
    if (ret < 0) {
        perror("ptl_cq_entry_send");
        return -1;
    }

    return 0;
}
