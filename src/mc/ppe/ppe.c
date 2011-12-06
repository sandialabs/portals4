#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "shared/ptl_connection_manager.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/nal.h"

static ptl_ppe_t ptl_ppe;

static int
progress_loop(ptl_ppe_t *ctx)
{
    int ret, i;
    ptl_cqe_t entry;

    while (0 == ctx->shutdown) {
        ret = ptl_cm_server_progress(ctx->cm_h);
        if (ret < 0) {
            perror("ptl_cm_server_progress");
            return -1;
        }

        for (i = 0 ; i < 100 ; ++i) {
            ctx->ni.nal->progress( &ctx->ni );

            ret = ptl_cq_entry_recv(ctx->cq_h, &entry);
            if (ret < 0) {
                perror("ptl_cq_entry_recv");
                return -1;
            } else if (ret == 1) {
                break;
            }
            switch(entry.base.type) {
            case PTLPROCATTACH:
                proc_attach_impl(ctx, &entry.procAttach);
                break;

            case PTLNIINIT:
                ni_init_impl( ctx, &entry.niInit );
                break;

            case PTLNIFINI:
                ni_fini_impl( ctx, &entry.niFini );
                break;

            case PTLSETMAP:
                setmap_impl(ctx, &entry.setMap);
                break;

            case PTLGETMAP:
                getmap_impl(ctx, &entry.getMap);
                break;

            case PTLCTALLOC:
                ct_alloc_impl( ctx, &entry.ctAlloc );
                break;

            case PTLCTFREE:
                ct_free_impl( ctx, &entry.ctFree );
                break;

            case PTLCTSET:
                ct_set_impl( ctx, &entry.ctSet );
                break;

            case PTLCTINC:
                ct_inc_impl( ctx, &entry.ctInc );
                break;

            case PTLEQALLOC:
                eq_alloc_impl( ctx, &entry.eqAlloc );
                break;

            case PTLEQFREE:
                eq_free_impl( ctx, &entry.eqFree );
                break;

            case PTLMDBIND:
                md_bind_impl( ctx, &entry.mdBind );
                break;

            case PTLMDRELEASE:
                md_release_impl( ctx, &entry.mdRelease );
                break;

            case PTLSWAP:
            case PTLPUT:
            case PTLGET:
            case PTLATOMIC:
            case PTLFETCHATOMIC:
                data_movement_impl( ctx, &entry.put );
                break;

            case PTLPTALLOC:
                pt_alloc_impl( ctx, &entry.ptAlloc );
                break;

            case PTLPTFREE:
                pt_free_impl( ctx, &entry.ptFree );
                break;

            case PTLMEAPPEND:
                me_append_impl( ctx, &entry.meAppend );
                break;

            case PTLMEUNLINK:
                me_unlink_impl( ctx, &entry.meUnlink );
                break;

            case PTLMESEARCH:
                me_search_impl( ctx, &entry.meSearch );
                break;

            case PTLLEAPPEND:
                le_append_impl( ctx, &entry.leAppend );
                break;

            case PTLLEUNLINK:
                le_unlink_impl( ctx, &entry.leUnlink );
                break;

            case PTLLESEARCH:
                le_search_impl( ctx, &entry.leSearch );
                break;

            case PTLATOMICSYNC:
                atomic_sync_impl( ctx, &entry.atomicSync );
                break;

            default:
                fprintf(stdout, "Found command queue entry of type %d\n", 
                        entry.base.type);
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

    memset(&ptl_ppe, 0, sizeof(ptl_ppe));
    memset(ptl_ppe.pids, -1, PTL_PID_MAX);

    ptl_ppe.page_size = sysconf(_SC_PAGESIZE);

    nal_init(&ptl_ppe);

    ret = ptl_ppe_init(&ptl_ppe, send_queue_size, recv_queue_size);
    if (ret < 0) {
        perror("ptl_ppe_init");
        return 1;
    }

    ret = progress_loop(&ptl_ppe);
    if (ret < 0) {
        perror("progress_loop");
        return 1;
    }

    ret = ptl_ppe_fini(&ptl_ppe);
    if (ret < 0) {
        perror("ptl_ppe_fini");
        return 1;
    }

    return 0;
}
