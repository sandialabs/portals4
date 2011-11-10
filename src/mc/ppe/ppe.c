#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "shared/ptl_connection_manager.h"
#include "shared/ptl_command_queue.h"
#include "shared/ptl_command_queue_entry.h"

static int done = 0;

ptl_cm_server_handle_t cm_h;
ptl_cq_handle_t cq_h;
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

    ret = ptl_cq_attach(cq_h, rem_info);
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
progress_loop(void)
{
    int ret;
    ptl_cqe_t entry, *send_entry;

    while (0 == done) {
        ret = ptl_cm_server_progress(cm_h);
        if (ret < 0) {
            perror("ptl_cm_server_progress");
            return -1;
        }

        ret = ptl_cq_entry_recv(cq_h, &entry);
        if (ret < 0) {
            perror("ptl_cq_entry_recv");
            return -1;
        } else if (ret == 0) {
            switch(entry.type) {
            case PTLNIINIT:
                ret = ptl_cq_entry_alloc(cq_h, &send_entry);
                if (ret < 0) {
                    perror("ptl_cq_entry_alloc");
                    return -1;
                }

                send_entry->type = PTLACK;
                send_entry->u.ack.retval_ptr = entry.u.niInit.retval_ptr;
                send_entry->u.ack.retval = PTL_OK;

                ret = ptl_cq_entry_send(cq_h, entry.u.niInit.ni_handle.s.code,
                                        send_entry, sizeof(ptl_cqe_t));
                if (ret < 0) {
                    perror("ptl_cq_entry_send");
                    return -1;
                }
                break;

            case PTLNIFINI:
                ret = ptl_cq_entry_alloc(cq_h, &send_entry);
                if (ret < 0) {
                    perror("ptl_cq_entry_alloc");
                    return -1;
                }

                send_entry->type = PTLACK;
                send_entry->u.ack.retval_ptr = entry.u.niInit.retval_ptr;
                send_entry->u.ack.retval = PTL_OK;

                ret = ptl_cq_entry_send(cq_h, entry.u.niInit.ni_handle.s.code,
                                        send_entry, sizeof(ptl_cqe_t));
                if (ret < 0) {
                    perror("ptl_cq_entry_send");
                    return -1;
                }
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
                        recv_queue_size, 0, &cq_h);
    if (ret < 0) {
        perror("ptl_cq_create");
        return -1;
    }

    ret = ptl_cq_info_get(cq_h, &info, &infolen);
    if (ret < 0) {
        perror("ptl_cq_info_get");
        return -1;
    }

    ret = progress_loop();
    if (ret < 0) {
        perror("progress_loop");
        return -1;
    }

    free(info);

    ret = ptl_cq_destroy(cq_h);
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
