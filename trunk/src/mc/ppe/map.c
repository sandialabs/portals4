#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <stddef.h>

#include "portals4.h"

#include "shared/ptl_internal_handles.h"
#include "ppe/ppe.h"
#include "ppe/dispatch.h"


int
setmap_impl(struct ptl_ppe_t *ctx, ptl_cqe_setmap_t *cmd)
{
    int ret;
    ptl_cqe_t *send_entry;
    int peer = cmd->ni_handle.s.selector;
    ptl_ppe_client_t *client;
    ptl_ppe_ni_t *ni;

    client = &ctx->clients[peer];
    ni = &client->nis[cmd->ni_handle.s.ni];

    ret = ptl_cq_entry_alloc(ctx->cq_h, &send_entry);
    if (ret < 0) {
        perror("ptl_cq_entry_alloc");
        return -1;
    }

    /* BWB: FIX ME: fill in */

    send_entry->base.type = PTLACK;
    send_entry->ack.retval_ptr = cmd->retval_ptr;
    send_entry->ack.retval = PTL_OK;

    ret = ptl_cq_entry_send( ctx->cq_h, peer,
                             send_entry, sizeof(ptl_cqe_t));
    if (ret < 0) {
        perror("ptl_cq_entry_send");
        return -1;
    }
    return 0;
}


int
getmap_impl(struct ptl_ppe_t *ctx, ptl_cqe_getmap_t *cmd)
{
    int ret;
    ptl_cqe_t *send_entry;
    int peer = cmd->ni_handle.s.selector;
    ptl_ppe_client_t *client;
    ptl_ppe_ni_t *ni;

    client = &ctx->clients[peer];
    ni = &client->nis[cmd->ni_handle.s.ni];

    ret = ptl_cq_entry_alloc(ctx->cq_h, &send_entry);
    if (ret < 0) {
        perror("ptl_cq_entry_alloc");
        return -1;
    }

    /* BWB: FIX ME: fill in */

    send_entry->base.type = PTLACK;
    send_entry->ack.retval_ptr = cmd->retval_ptr;
    send_entry->ack.retval = PTL_OK;

    ret = ptl_cq_entry_send( ctx->cq_h, peer,
                             send_entry, sizeof(ptl_cqe_t));
    if (ret < 0) {
        perror("ptl_cq_entry_send");
        return -1;
    }
    return 0;
}
