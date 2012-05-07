#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/ptl_connection_manager.h"
#include "shared/ptl_command_queue_entry.h"
#include "ppe/ppe.h"
#include "ppe/dispatch.h"

static int
cm_connect_cb(int remote_id, void *cb_data)
{
    int ret;
    ptl_ppe_t *ptl_ppe = (ptl_ppe_t*) cb_data;

    PPE_DBG("Server got connect callback %d\n", remote_id);

    ret = ptl_cm_server_send(ptl_ppe->cm_h, remote_id, 
                             ptl_ppe->info, ptl_ppe->infolen);
    if (ret < 0) {
        perror("ptl_cq_server_send");
        return -1;
    }

    return 0;
}


static int
cm_disconnect_cb(int remote_id, void *cb_data)
{
    ptl_ppe_t *ptl_ppe = (ptl_ppe_t*) cb_data;

    PPE_DBG("Server got disconnect callback %d\n", remote_id);

    ptl_ppe_teardown_peer(ptl_ppe, remote_id, 1);

    return 0;
}


static int
cm_recv_cb(int remote_id, void *buf, size_t len, void *cb_data) 
{
    int ret, ack = 0;
    ptl_cq_info_t *rem_info = (ptl_cq_info_t*) buf;
    ptl_ppe_t *ptl_ppe = (ptl_ppe_t*) cb_data;

    PPE_DBG("Server got message from %d: %p,%ld\n", remote_id, buf, len);

    ret = ptl_cq_attach(ptl_ppe->cq_h, rem_info);
    if (ret < 0) {
        perror("ptl_cq_attach");
        return -1;
    }

    ret = ptl_cm_server_send(ptl_ppe->cm_h, remote_id, &ack, sizeof(ack));
    if (ret < 0) {
        perror("ptl_cq_server_send");
        return -1;
    }

    return 0;
}


int
ptl_ppe_init(ptl_ppe_t *ptl_ppe, int send_queue_size, int recv_queue_size)
{
    int ret;

    ret = ptl_cm_server_create(&ptl_ppe->cm_h);
    if (ret < 0) {
        perror("ptl_cm_server_create");
        return -1;
    }

    ret = ptl_cm_server_register_connect_cb(ptl_ppe->cm_h, 
                                            cm_connect_cb, 
                                            ptl_ppe);
    if (ret < 0) {
        perror("ptl_cm_server_register_connect_cb");
        return -1;
    }

    ret = ptl_cm_server_register_disconnect_cb(ptl_ppe->cm_h, 
                                               cm_disconnect_cb, 
                                               ptl_ppe);
    if (ret < 0) {
        perror("ptl_cm_server_register_disconnect_cb");
        return -1;
    }

    ret = ptl_cm_server_register_recv_cb(ptl_ppe->cm_h, 
                                         cm_recv_cb, 
                                         ptl_ppe);
    if (ret < 0) {
        perror("ptl_cm_server_register_recv_cb");
        return -1;
    }

    ret = ptl_cq_create(sizeof(ptl_cqe_t), send_queue_size, 
                        recv_queue_size, 0, &ptl_ppe->cq_h);
    if (ret < 0) {
        perror("ptl_cq_create");
        return -1;
    }

    ret = ptl_cq_info_get(ptl_ppe->cq_h, &ptl_ppe->info, &ptl_ppe->infolen);
    if (ret < 0) {
        perror("ptl_cq_info_get");
        return -1;
    }

    return 0;
}


int
ptl_ppe_fini(ptl_ppe_t *ptl_ppe)
{
    int ret;
    
    free(ptl_ppe->info);

    ret = ptl_cq_destroy(ptl_ppe->cq_h);
    if (ret < 0) {
        perror("ptl_cq_destroy");
        return -1;
    }

    ret = ptl_cm_server_destroy(ptl_ppe->cm_h);
    if (ret < 0) {
        perror("ptl_cm_server_destroy");
        return -1;
    }

    return 0;
}

int
ptl_ppe_teardown_peer(ptl_ppe_t *ptl_ppe, int remote_id, int forced)
{
    int ret;
    ptl_ppe_client_t *client = &ptl_ppe->clients[remote_id];

    client->connected = 0;
    ret = ppe_xpmem_fini(&client->xpmem_segments);
    ptl_ppe->pids[client->pid] = -1;

    return ret;;
}


int
proc_attach_impl(ptl_ppe_t *ptl_ppe, ptl_cqe_proc_attach_t *attach)
{
    int ret;
    ptl_ppe_client_t *client = &ptl_ppe->clients[attach->proc_id];

    memset(client, 0, sizeof(ptl_ppe_client_t));
    client->pid = -1;

    client->segid = attach->segid;

    ret = ppe_xpmem_init(&client->xpmem_segments, 
                         client->segid, 
                         ptl_ppe->page_size);
    if (0 != ret) return -1;

    client->connected = 1;

    return 0;
}
