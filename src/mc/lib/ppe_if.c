#include "ppe_if.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "shared/ptl_command_queue.h"
#include "shared/ptl_command_queue_entry.h"
#include "shared/ptl_connection_manager.h"

static int have_attached_cq = 0;

struct ppe_if ppe_if_data; 

static int
ppe_recv_callback(int remote_id, void *buf, size_t len)
{
    ptl_cq_info_t *info;
    size_t infolen;
    int ret;

    printf("ppe_recv_callback start\n");

    ret = ptl_cq_info_get(ppe_if_data.cq_h, &info, &infolen);
    if (ret < 0) {
        perror("ptl_cq_info_get");
        abort();
    }

    ret = ptl_cm_client_send(ppe_if_data.cm_h, info, infolen);
    if (ret < 0) {
        perror("ptl_cm_client_send");
        abort();
    }

    free(info);

    info = buf;
    ret = ptl_cq_attach(ppe_if_data.cq_h, info);
    if (ret < 0) {
        perror("ptl_cq_attach");
        abort();
    }

    have_attached_cq = 1;

    printf("ppe_recv_callback end\n");

    return 0;
}


static int
ppe_disconnect_callback(int remote_id)
{
    fprintf(stderr, "Lost connection to processing engine.  Aborting.\n");
    abort();
}


void ppe_if_init()
{
    int ret;
    int send_queue_size = 32; /* BWB: FIX ME */
    int recv_queue_size = 32; /* BWB: FIX ME */


    ppe_if_data.sharedBase =  malloc( sizeof( ptl_md_t ) * ppe_if_data.limits.max_mds );
    
    ppe_if_data.mdBase = ppe_if_data.sharedBase;  
    ppe_if_data.mdFreeHint = 0;

    ret = ptl_cm_client_connect(&ppe_if_data.cm_h, &ppe_if_data.my_ppe_rank);
    if (ret < 0) {
        perror("ptl_cm_client_connect");
        abort();
    }

    ret = ptl_cm_client_register_disconnect_cb(ppe_if_data.cm_h, ppe_disconnect_callback);
    if (ret < 0) {
        perror("ptl_cm_register_disconnect_cb");
        abort();
    }

    ret = ptl_cm_client_register_recv_cb(ppe_if_data.cm_h, ppe_recv_callback);
    if (ret < 0) {
        perror("ptl_cm_register_recv_cb");
        abort();
    }

    ret = ptl_cq_create(sizeof(ptl_cqe_t), send_queue_size, recv_queue_size,
                        ppe_if_data.my_ppe_rank, &ppe_if_data.cq_h);
    if (ret < 0) {
        perror("ptl_cq_create");
        abort();
    }

    while (have_attached_cq == 0) {
        ret = ptl_cm_client_progress(ppe_if_data.cm_h);
        if (ret < 0) {
            perror("ptl_cm_client_progress");
            abort();
        }
    }
}
