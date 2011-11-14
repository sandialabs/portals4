#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "shared/ptl_connection_manager.h"

FILE *debug;

static int server_done = 0;


static int
server_connect_cb(int remote_id, void *data)
{
    fprintf(debug, "server: connection from remote with id %d\n",
            remote_id);
    return 0;
}


static int
server_disconnect_cb(int remote_id, void *data)
{
    fprintf(debug, "server: disconnect from remote with id %d\n",
            remote_id);
    server_done = 1;
    return 0;
}


static int
server(int fd)
{
    int ret, tmp;
    ptl_cm_server_handle_t cm_h;

    ret = ptl_cm_server_create(&cm_h);
    if (0 != ret) {
        perror("ptl_cm_server_create");
        return -1;
    }

    ret = ptl_cm_server_register_connect_cb(cm_h, server_connect_cb, NULL);
    if (0 != ret) {
        perror("ptl_cm_server_register_connect_cb");
        return -1;
    }

    ret = ptl_cm_server_register_disconnect_cb(cm_h, server_disconnect_cb, NULL);
    if (0 != ret) {
        perror("ptl_cm_server_register_disconnect_cb");
        return -1;
    }

    ret = write(fd, &tmp, sizeof(tmp));
    if (ret < 0) {
        perror("write");
        return ret;
    }

    while (0 == server_done) {
        ret = ptl_cm_server_progress(cm_h);
        if (0 != ret) {
            perror("ptl_cm_server_progress");
            return -1;
        }
    }

    ret = ptl_cm_server_destroy(cm_h);
    if (0 != ret) {
        perror("ptl_cm_server_destroy");
        return -1;
    }

    return 0;
}


static int
client_disconnect_cb(int remote_id, void *data)
{
    fprintf(debug, "client: disconnect from remote with id %d\n",
            remote_id);
    return 1;
}


static int
client(int fd)
{
    int ret, tmp, my_id;
    ptl_cm_client_handle_t cm_h;

    ret = read(fd, &tmp, sizeof(tmp));
    if (ret < 0) {
        perror("read");
        return ret;
    }

    ret = ptl_cm_client_connect(&cm_h, &my_id);
    if (0 != ret) {
        perror("ptl_cm_client_connect");
        return -1;
    }

    fprintf(debug, "client: connected with id %d\n", my_id);

    ret = ptl_cm_client_register_disconnect_cb(cm_h,
                                               client_disconnect_cb,
                                               NULL);
    if (0 != ret) {
        perror("ptl_cm_client_register_disconnect_cb");
        return -1;
    }

    ret = ptl_cm_client_progress(cm_h);
    if (0 != ret) {
        perror("ptl_cm_client_progress");
        return -1;
    }

    ret = ptl_cm_client_disconnect(cm_h);
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
