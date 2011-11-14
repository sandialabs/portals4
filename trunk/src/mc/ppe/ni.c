#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <stddef.h>

#include "portals4.h"

#include "shared/ptl_internal_handles.h"
#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#define OFFSETPTR(base, off) ((void*) ((char*) base + off))

int
ni_init_impl( ptl_ppe_t *ctx, ptl_cqe_niinit_t *cmd )
{
    int ret, i;
    ptl_cqe_t *send_entry;
    int peer = cmd->ni_handle.s.selector;
    ptl_ppe_client_t *client;
    ptl_ppe_ni_t *ni;
    ptl_process_t *phys_proc;

    client = &ctx->clients[peer];
    ni = &client->nis[cmd->ni_handle.s.ni];

    /* reserve pid */
    if (PTL_PID_ANY == cmd->pid) {
        if (-1 == client->pid) {
            for (i = 0 ; i < PTL_PID_MAX ; ++i) {
                if (-1 == ctx->pids[i]) {
                    client->pid = i;
                    ctx->pids[i] = peer;
                    break;
                }
            }
            if (-1 == client->pid) {
                ret = PTL_NO_SPACE;
                goto cleanup;
            }
        }
    } else {
        if (-1 == client->pid) {
            if (-1 == ctx->pids[cmd->pid]) {
                client->pid = cmd->pid;
                ctx->pids[cmd->pid] = peer;
            } else {
                ret = PTL_PID_IN_USE;
                goto cleanup;
            }
        } else if (cmd->pid != client->pid) {
            ret = PTL_ARG_INVALID;
            goto cleanup;
        }
    }

    /* map in and process limits structure */
    ni->limits_ptr = ppe_xpmem_attach(&client->xpmem_segments, 
                                      cmd->limits,
                                      sizeof(ptl_ni_limits_t));
    if (NULL == ni->limits_ptr) {
        ret = PTL_FAIL;
        goto cleanup;
    }
    /* BWB: FIX ME: check these limits... */
    ni->limits = ni->limits_ptr->data;
    ni->limits->max_iovecs = 1;
    ni->limits->max_msg_size = INT_MAX;
    ni->limits->max_atomic_size = ctx->page_size;
    ni->limits->max_waw_ordered_size = ctx->page_size;
    ni->limits->max_war_ordered_size = ctx->page_size;
    ni->limits->max_volatile_size = 0;
    ni->limits->features = 0;

    /* Map in shared data */
    ni->client_ptr = ppe_xpmem_attach(&client->xpmem_segments, 
                                      cmd->shared_data, 
                                      cmd->shared_data_length);
    if (NULL == ni->client_ptr) {
        ret = PTL_FAIL;
        goto cleanup;
    }

    phys_proc = OFFSETPTR(ni->client_ptr->data, cmd->phys_addr);
    ni->status_registers = OFFSETPTR(ni->client_ptr->data, cmd->status_reg);
    ni->les = OFFSETPTR(ni->client_ptr->data, cmd->les);
    ni->mds = OFFSETPTR(ni->client_ptr->data, cmd->mds);
    ni->mes = OFFSETPTR(ni->client_ptr->data, cmd->mes);
    ni->cts = OFFSETPTR(ni->client_ptr->data, cmd->cts);
    ni->eqs = OFFSETPTR(ni->client_ptr->data, cmd->eqs);
    ni->pts = OFFSETPTR(ni->client_ptr->data, cmd->pts);

    /* set client's physical process */
    phys_proc->phys.nid = ctx->nid;
    phys_proc->phys.pid = client->pid;

    goto send_retval;

 cleanup:
    if (NULL != ni->limits_ptr) {
        ppe_xpmem_detach(&client->xpmem_segments, ni->limits_ptr);
        ni->limits_ptr = NULL;
    }
    if (NULL != ni->client_ptr) {
        ppe_xpmem_detach(&client->xpmem_segments, ni->client_ptr);
        ni->client_ptr = NULL;
    }

 send_retval:
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


int 
ni_fini_impl( ptl_ppe_t *ctx, ptl_cqe_nifini_t *cmd )
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

    if (NULL != ni->limits_ptr) {
        ppe_xpmem_detach(&client->xpmem_segments, ni->limits_ptr);
    }
    if (NULL != ni->client_ptr) {
        ppe_xpmem_detach(&client->xpmem_segments, ni->client_ptr);
    }
    memset(ni, 0, sizeof(ptl_ppe_ni_t));

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
