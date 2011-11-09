#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "portals4.h"
#include "ptl_internal_global.h"
#include "ptl_internal_startup.h"
#include "shared/ptl_command_queue.h"
#include "shared/ptl_command_queue_entry.h"
#include "shared/ptl_connection_manager.h"

static int have_attached_cq = 0;
static int state = 0;

static int
ppe_recv_callback(int remote_id, void *buf, size_t len)
{
    ptl_cq_info_t *info;
    size_t infolen;
    int ret;

    if (state == 0) {
        ret = ptl_cq_info_get(ptl_iface.cq_h, &info, &infolen);
        if (ret < 0) {
            perror("ptl_cq_info_get");
            abort();
        }

        ret = ptl_cm_client_send(ptl_iface.cm_h, info, infolen);
        if (ret < 0) {
            perror("ptl_cm_client_send");
            abort();
        }

        free(info);

        info = buf;
        ret = ptl_cq_attach(ptl_iface.cq_h, info);
        if (ret < 0) {
            perror("ptl_cq_attach");
            abort();
        }
        state++;
    } else if (state == 1) {
        have_attached_cq = 1;
        state++;
    } else {
        fprintf(stderr, "unexpected ppe recv callback\n");
    }

    return 0;
}


static int
ppe_disconnect_callback(int remote_id)
{
    fprintf(stderr, "Lost connection to processing engine.  Aborting.\n");
    abort();
}


int
ptl_ppe_global_init(ptl_iface_t *iface)
{
    return 0;
}


int
ptl_ppe_global_setup(ptl_iface_t *iface)
{
    return 0;
}


int
ptl_ppe_global_fini(ptl_iface_t *iface)
{
    return 0;
}


int
ptl_ppe_connect(ptl_iface_t *iface)
{
    int ret;
    int send_queue_size = 32; /* BWB: FIX ME */
    int recv_queue_size = 32; /* BWB: FIX ME */

    ret = ptl_cm_client_connect(&ptl_iface.cm_h, &ptl_iface.my_ppe_rank);
    if (ret < 0) {
        perror("ptl_cm_client_connect");
        return -1;
    }

    ret = ptl_cm_client_register_disconnect_cb(ptl_iface.cm_h,
                                               ppe_disconnect_callback);
    if (ret < 0) {
        perror("ptl_cm_register_disconnect_cb");
        return -1;
    }

    ret = ptl_cm_client_register_recv_cb(ptl_iface.cm_h, ppe_recv_callback);
    if (ret < 0) {
        perror("ptl_cm_register_recv_cb");
        return -1;
    }

    ret = ptl_cq_create(sizeof(ptl_cqe_t), send_queue_size, recv_queue_size,
                        ptl_iface.my_ppe_rank, &ptl_iface.cq_h);
    if (ret < 0) {
        perror("ptl_cq_create");
        return -1;
    }

    while (have_attached_cq == 0) {
        ret = ptl_cm_client_progress(ptl_iface.cm_h);
        if (ret < 0) {
            perror("ptl_cm_client_progress");
            return -1;
        }
    }

    return 0;
}


int
ptl_ppe_disconnect(ptl_iface_t *iface)
{
    int ret;

    ret = ptl_cm_client_disconnect(iface->cm_h);
    if (ret < 0) {
        perror("ptl_cm_client_disconnect");
        return -1;
    }

    ret = ptl_cq_destroy(iface->cq_h);
    if (ret < 0) {
        perror("ptl_cq_destroy");
        return -1;
    }

    return 0;
}
