#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "shared/ptl_connection_manager.h"

#include "ppe.h"

static int done = 0;

ptl_ppe_t ppe_ctx;
ptl_cm_server_handle_t cm_h;
ptl_cq_info_t *info;
size_t infolen;

static int
cm_connect_cb(int remote_id)
{
    int ret;

    fprintf(stderr, "Server got connect callback %d\n", remote_id);

    ret = ptl_cm_server_send(cm_h, remote_id, info, infolen);
    if (ret < 0) {
        perror("ptl_cq_server_send");
        return -1;
    }

    return 0;
}

static int
cm_disconnect_cb(int remote_id)
{
    fprintf(stderr, "Server got disconnect callback %d\n", remote_id);
    return 0;
}

static int
cm_recv_cb(int remote_id, void *buf, size_t len) 
{
    int ret, ack = 0;
    ptl_cq_info_t *rem_info = (ptl_cq_info_t*) buf;

    fprintf(stderr, "Server got message from %d: %p,%ld\n", remote_id, buf, len);

    ret = ptl_cq_attach(ppe_ctx.cq_h, rem_info);
    if (ret < 0) {
        perror("ptl_cq_attach");
        return -1;
    }

    ret = ptl_cm_server_send(cm_h, remote_id, &ack, sizeof(ack));
    if (ret < 0) {
        perror("ptl_cq_server_send");
        return -1;
    }

    return 0;
}


static int
progress_loop( ptl_ppe_t *ctx )
{
    int ret;
    ptl_cqe_t entry;

    while (0 == done) {
        ret = ptl_cm_server_progress(cm_h);
        if (ret < 0) {
            perror("ptl_cm_server_progress");
            return -1;
        }

        ret = ptl_cq_entry_recv(ctx->cq_h, &entry);
        if (ret < 0) {
            perror("ptl_cq_entry_recv");
            return -1;
        } else if (ret == 0) {
            switch(entry.type) {
            case PTLNIINIT:
                ni_init_impl( ctx, &entry.u.niInit );
                break;

            case PTLNIFINI:
                ni_fini_impl( ctx, &entry.u.niFini );
                break;

            case PTLCTALLOC:
                ct_alloc_impl( ctx, &entry.u.ctAlloc );
                break;

            case PTLCTFREE:
                ct_free_impl( ctx, &entry.u.ctFree );
                break;

            case PTLCTSET:
                ct_set_impl( ctx, &entry.u.ctSet );
                break;

            case PTLCTINC:
                ct_inc_impl( ctx, &entry.u.ctInc );
                break;

            case PTLEQALLOC:
                eq_alloc_impl( ctx, &entry.u.eqAlloc );
                break;

            case PTLEQFREE:
                eq_free_impl( ctx, &entry.u.eqFree );
                break;

            case PTLMDBIND:
                md_bind_impl( ctx, &entry.u.mdBind );
                break;

            case PTLMDRELEASE:
                md_release_impl( ctx, &entry.u.mdRelease );
                break;

            case PTLPUT:
                put_impl( ctx, &entry.u.put );
                break;

            case PTLGET:
                get_impl( ctx, &entry.u.get );
                break;

            case PTLPTALLOC:
                pt_alloc_impl( ctx, &entry.u.ptAlloc );
                break;

            case PTLPTFREE:
                pt_free_impl( ctx, &entry.u.ptFree );
                break;

            case PTLMEAPPEND:
                me_append_impl( ctx, &entry.u.meAppend );
                break;

            case PTLMEUNLINK:
                me_unlink_impl( ctx, &entry.u.meUnlink );
                break;

            case PTLMESEARCH:
                me_search_impl( ctx, &entry.u.meSearch );
                break;

            case PTLLEAPPEND:
                le_append_impl( ctx, &entry.u.leAppend );
                break;

            case PTLLEUNLINK:
                le_unlink_impl( ctx, &entry.u.leUnlink );
                break;

            case PTLLESEARCH:
                le_search_impl( ctx, &entry.u.leSearch );
                break;

            case PTLATOMIC:
                atomic_impl( ctx, &entry.u.atomic );
                break;

            case PTLFETCHATOMIC:
                fetch_atomic_impl( ctx, &entry.u.fetchAtomic );
                break;

            case PTLSWAP:
                swap_impl( ctx, &entry.u.swap );
                break;

            case PTLATOMICSYNC:
                atomic_sync_impl( ctx, &entry.u.atomicSync );
                break;

            default:
                fprintf(stdout, "Found command queue entry of type %d\n", entry.type);
            }
        }
    }

    return PTL_OK;
}


int
main(int argc, char *argv[])
{
    int ret;
    int send_queue_size = 32; /* BWB: FIX ME */
    int recv_queue_size = 32; /* BWB: FIX ME */

    ret = ptl_cm_server_create(&cm_h);
    if (ret < 0) {
        perror("ptl_cm_server_create");
        return -1;
    }

    ret = ptl_cm_server_register_connect_cb(cm_h, cm_connect_cb);
    if (ret < 0) {
        perror("ptl_cm_server_register_connect_cb");
        return -1;
    }

    ret = ptl_cm_server_register_disconnect_cb(cm_h, cm_disconnect_cb);
    if (ret < 0) {
        perror("ptl_cm_server_register_disconnect_cb");
        return -1;
    }

    ret = ptl_cm_server_register_recv_cb(cm_h, cm_recv_cb);
    if (ret < 0) {
        perror("ptl_cm_server_register_recv_cb");
        return -1;
    }

    ret = ptl_cq_create(sizeof(ptl_cqe_t), send_queue_size, 
                        recv_queue_size, 0, &ppe_ctx.cq_h);
    if (ret < 0) {
        perror("ptl_cq_create");
        return -1;
    }

    ret = ptl_cq_info_get(ppe_ctx.cq_h, &info, &infolen);
    if (ret < 0) {
        perror("ptl_cq_info_get");
        return -1;
    }

    ret = progress_loop(&ppe_ctx);
    if (ret < 0) {
        perror("progress_loop");
        return -1;
    }

    free(info);

    ret = ptl_cq_destroy(ppe_ctx.cq_h);
    if (ret < 0) {
        perror("ptl_cq_destroy");
        return -1;
    }

    ret = ptl_cm_server_destroy(cm_h);
    if (ret < 0) {
        perror("ptl_cm_server_destroy");
        return -1;
    }

    return 0;
}
