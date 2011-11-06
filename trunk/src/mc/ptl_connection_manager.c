#include "config.h"

#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ptl_connection_manager.h"

#define MAX_CONNECTIONS 64


struct ptl_cm_connection_t {
    int fd;
};
typedef struct ptl_cm_connection_t ptl_cm_connection_t;

struct ptl_cm_server_t {
    int listenfd;
    ptl_cm_connect_cb_t connect_cb;
    ptl_cm_disconnect_cb_t disconnect_cb;
    ptl_cm_connection_t connections[MAX_CONNECTIONS];
};
typedef struct ptl_cm_server_t ptl_cm_server_t;

struct ptl_cm_client_t {

};
typedef struct ptl_cm_client_t ptl_cm_client_t;


static int
connect_cb(int my_id)
{
    errno = EINVAL;
    return -1;
}

static int
disconnect_cb(int my_id)
{
    return 0;
}


int
ptl_cm_server_create(ptl_cm_server_handle_t *cm_h, int *my_id)
{
    ptl_cm_server_t *cm;
    struct sockaddr_in addr;
    int i, ret, port = 1423;

    if (NULL != getenv("PTL_CM_SERVER_PORT")) {
        port = atoi(getenv("PTL_CM_SERVER_PORT"));
    }

    cm = malloc(sizeof(ptl_cm_server_t));
    if (NULL == cm) return -1;

    cm->listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cm->listenfd < 0) goto cleanup;

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = port;

    ret = bind(cm->listenfd, (struct sockaddr*) &addr, sizeof(addr));
    if (ret < 0) goto cleanup;

    ret = listen(cm->listenfd, 64);
    if (ret < 0) goto cleanup;

    cm->connect_cb = connect_cb;
    cm->disconnect_cb = disconnect_cb;

    for (i = 0 ; i < MAX_CONNECTIONS ; ++i) {
        cm->connections[i].fd = -1;
    }

    *cm_h = cm;

    return 0;

 cleanup:
    {
        int err = errno;
        if (cm->listenfd >= 0) close(cm->listenfd);
        if (NULL != cm) free(cm);
        errno = err;
    }
    return -1;
}


int
ptl_cm_server_destroy(ptl_cm_server_handle_t cm_h)
{
    int i;

    for (i = 0 ; i < MAX_CONNECTIONS ; ++i) {
        if (cm_h->connections[i].fd != -1) {
            close(cm_h->connections[i].fd);
        }
    }

    free(cm_h);

    return 0;
}


int
ptl_cm_server_register_connect_cb(ptl_cm_server_handle_t cm_h, 
                                  ptl_cm_connect_cb_t cb)
{
    errno = ENOSYS;
    return -1;
}


int
ptl_cm_server_register_disconnect_cb(ptl_cm_server_handle_t cm_h, 
                                     ptl_cm_disconnect_cb_t cb)
{
    errno = ENOSYS;
    return -1;
}


int
ptl_cm_server_progress(ptl_cm_server_handle_t cm_h)
{
    errno = ENOSYS;
    return -1;
}


int
ptl_cm_client_connect(ptl_cm_client_handle_t *cm_h, int *my_id)
{
    errno = ENOSYS;
    return -1;
}


int
ptl_cm_client_disconnect(ptl_cm_client_handle_t cm_h)
{
    errno = ENOSYS;
    return -1;
}


int
ptl_cm_client_register_disconnect_cb(ptl_cm_client_handle_t cm_h,
                                     ptl_cm_disconnect_cb_t cb)
{
    errno = ENOSYS;
    return -1;
}


int
ptl_cm_client_progress(ptl_cm_client_handle_t cm_h)
{
    errno = ENOSYS;
    return -1;
}

