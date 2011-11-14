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


struct ptl_connection_data_t {
    int have_attached_cq;
    int state;
};
typedef struct ptl_connection_data_t ptl_connection_data_t;


static int
ppe_recv_callback(int remote_id, void *buf, size_t len, void *cb_data)
{
    ptl_cq_info_t *info;
    size_t infolen;
    int ret;
    ptl_iface_t *iface = (ptl_iface_t*) cb_data;

    if (iface->connection_data->state == 0) {
        /* state 0: PPE has noticed our connection and sent it's
           command queue information.  Send our command queue
           information and attach */
        ret = ptl_cq_info_get(iface->cq_h, &info, &infolen);
        if (ret < 0) {
            perror("ptl_cq_info_get");
            abort();
        }

        ret = ptl_cm_client_send(iface->cm_h, info, infolen);
        if (ret < 0) {
            perror("ptl_cm_client_send");
            abort();
        }

        free(info);

        info = buf;
        ret = ptl_cq_attach(iface->cq_h, info);
        if (ret < 0) {
            perror("ptl_cq_attach");
            abort();
        }
        iface->connection_data->state++;

    } else if (iface->connection_data->state == 1) {
        /* state 1: PPE has received our command queue information and
           finished its attach.  We have a working bidirectional cq */
        iface->connection_data->have_attached_cq = 1;
        __sync_synchronize();
        iface->connection_data->state++;

    } else {
        fprintf(stderr, "unexpected ppe recv callback\n");
    }

    return 0;
}


static int
ppe_disconnect_callback(int remote_id, void *cb_data)
{
    fprintf(stderr, "Lost connection to processing engine.  Aborting.\n");
    abort();
}


int
ptl_ppe_connect(ptl_iface_t *iface)
{
    int ret;
    int send_queue_size = 32; /* BWB: FIX ME */
    int recv_queue_size = 32; /* BWB: FIX ME */

    iface->connection_data = malloc(sizeof(ptl_connection_data_t));
    if (NULL == iface->connection_data) {
        return -1;
    }

    iface->connection_data->state = 0;
    iface->connection_data->have_attached_cq = 0;

    ret = ptl_cm_client_connect(&iface->cm_h, &iface->my_ppe_rank);
    if (ret < 0) {
        perror("ptl_cm_client_connect");
        return -1;
    }

    ret = ptl_cm_client_register_disconnect_cb(iface->cm_h,
                                               ppe_disconnect_callback,
                                               iface);
    if (ret < 0) {
        perror("ptl_cm_register_disconnect_cb");
        return -1;
    }

    ret = ptl_cm_client_register_recv_cb(iface->cm_h, ppe_recv_callback, iface);
    if (ret < 0) {
        perror("ptl_cm_register_recv_cb");
        return -1;
    }

    ret = ptl_cq_create(sizeof(ptl_cqe_t), send_queue_size, recv_queue_size,
                        iface->my_ppe_rank, &iface->cq_h);
    if (ret < 0) {
        perror("ptl_cq_create");
        return -1;
    }

    while (iface->connection_data->have_attached_cq == 0) {
        ret = ptl_cm_client_progress(iface->cm_h);
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

    if (NULL != iface->connection_data) {
        free(iface->connection_data);
        iface->connection_data = NULL;
    }

    return 0;
}


int
ptl_ppe_progress(ptl_iface_t *iface, int progress_cm)
{
    int ret;
    ptl_cqe_t entry;

    while (1) {
        if (progress_cm) {
            ret = ptl_cm_client_progress(iface->cm_h);
            if (ret < 0) {
                perror("ptl_cm_server_progress");
                return -1;
            }
        }

        ret = ptl_cq_entry_recv(iface->cq_h, &entry);
        if (ret < 0) {
            perror("ptl_cq_entry_recv");
            return -1;
        } else if (ret == 0) {
            if (entry.base.type != PTLACK) {
                fprintf(stdout, 
                        "Found unexpected command queue entry of type %d\n", 
                        entry.base.type);
                return PTL_FAIL;
            }
            *(entry.ack.retval_ptr) = entry.ack.retval;
            return PTL_OK;
        }
    }

    return PTL_OK;
}
