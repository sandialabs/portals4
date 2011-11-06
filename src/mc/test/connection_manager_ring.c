#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "ptl_connection_manager.h"

FILE *debug;

ptl_cm_server_handle_t server_cm_h;
ptl_cm_client_handle_t client_cm_h;

static int server_done = 0;
static int server_counter = -1;
static int client_counter = -1;

static int start = 1;
static int done = 100;

static int
server_connect_cb(int remote_id)
{
    fprintf(debug, "server: connection from remote with id %d\n",
            remote_id);
    return 0;
}


static int
server_disconnect_cb(int remote_id)
{
    fprintf(debug, "server: disconnect from remote with id %d\n",
            remote_id);
    server_done = 1;
    return 0;
}


static int
server_recv_cb(int remote_id, void *buf, size_t len)
{
    int tmp, ret;

    if (sizeof(int) != len) return -1;

    tmp = *((int*) buf);
    fprintf(debug, "server: received counter %d from %d\n", tmp, remote_id);
    server_counter = tmp;

    tmp++;
    ret = ptl_cm_server_send(server_cm_h, remote_id, &tmp, sizeof(tmp));
    if (0 != ret) {
        perror("ptl_cm_server_send (cb)");
        return -1;
    }

    return 0;
}


static int
server(int fd)
{
    int ret, tmp;

    ret = ptl_cm_server_create(&server_cm_h);
    if (0 != ret) {
        perror("ptl_cm_server_create");
        return -1;
    }

    ret = ptl_cm_server_register_connect_cb(server_cm_h, server_connect_cb);
    if (0 != ret) {
        perror("ptl_cm_server_register_connect_cb");
        return -1;
    }

    ret = ptl_cm_server_register_disconnect_cb(server_cm_h, server_disconnect_cb);
    if (0 != ret) {
        perror("ptl_cm_server_register_disconnect_cb");
        return -1;
    }

    ret = ptl_cm_server_register_recv_cb(server_cm_h, server_recv_cb);
    if (0 != ret) {
        perror("ptl_cm_server_register_recv_cb");
        return -1;
    }

    ret = write(fd, &tmp, sizeof(tmp));
    if (ret < 0) {
        perror("write");
        return ret;
    }

    while (0 == server_done) {
        ret = ptl_cm_server_progress(server_cm_h);
        if (0 != ret) {
            perror("ptl_cm_server_progress");
            return -1;
        }
    }

    ret = ptl_cm_server_destroy(server_cm_h);
    if (0 != ret) {
        perror("ptl_cm_server_destroy");
        return -1;
    }

    return 0;
}


static int
client_disconnect_cb(int remote_id)
{
    fprintf(debug, "client: disconnect from remote with id %d\n",
            remote_id);
    return 1;
}


static int
client_recv_cb(int remote_id, void *buf, size_t len)
{
    int tmp;

    if (sizeof(int) != len) return -1;

    tmp = *((int*) buf);
    fprintf(debug, "client: received counter %d\n", tmp);
    client_counter = tmp;

    tmp++;
    return ptl_cm_client_send(client_cm_h, &tmp, sizeof(tmp));
}


static int
client(int fd)
{
    int ret, tmp, my_id;

    ret = read(fd, &tmp, sizeof(tmp));
    if (ret < 0) {
        perror("read");
        return ret;
    }

    ret = ptl_cm_client_connect(&client_cm_h, &my_id);
    if (0 != ret) {
        perror("ptl_cm_client_connect");
        return -1;
    }

    fprintf(debug, "client: connected with id %d\n", my_id);

    ret = ptl_cm_client_register_disconnect_cb(client_cm_h,
                                               client_disconnect_cb);
    if (0 != ret) {
        perror("ptl_cm_client_register_disconnect_cb");
        return -1;
    }

    ret = ptl_cm_client_register_recv_cb(client_cm_h,
                                         client_recv_cb);
    if (0 != ret) {
        perror("ptl_cm_client_register_recv_cb");
        return -1;
    }

    tmp = start;
    ret = ptl_cm_client_send(client_cm_h, &tmp, sizeof(tmp));
    if (0 != ret) {
        perror("ptl_cm_client_send");
        return -1;
    }

    while (client_counter < done) {
        ret = ptl_cm_client_progress(client_cm_h);
        if (0 != ret) {
            perror("ptl_cm_client_progress");
            return -1;
        }
    }

    ret = ptl_cm_client_disconnect(client_cm_h);
    if (0 != ret) {
        perror("ptl_cm_client_disconnect");
        return -1;
    }

    fprintf(debug, "client: leaving\n");
    
    return 0;
}


int
main(int argc, char *argv[])
{
    int ret;
    int fds[2];
    pid_t pid;

    if (NULL == getenv("MAKELEVEL")) {
        debug = stdout;
    } else {
        debug = fopen("/dev/null", "a+");
    }

    putenv("PTL_CM_SERVER_PORT=7654");

    ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, fds);
    if (0 != ret) {
        perror("socketpair");
        return ret;
    }

    pid = fork();
    if (pid < 0) {
        perror("pipe");
    } else if (pid == 0) {
        close(fds[1]);
        ret = client(fds[0]);
    } else {
        close(fds[0]);
        ret = server(fds[1]);
    }

    return ret;
}
