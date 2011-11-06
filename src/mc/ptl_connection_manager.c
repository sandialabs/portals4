#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
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
    ptl_cm_recv_cb_t recv_cb;
    struct pollfd *fds;
    nfds_t nfds;
    ptl_cm_connection_t connections[MAX_CONNECTIONS];
};
typedef struct ptl_cm_server_t ptl_cm_server_t;

struct ptl_cm_client_t {
    int fd;
    int32_t my_rank;
    ptl_cm_disconnect_cb_t disconnect_cb;
    ptl_cm_recv_cb_t recv_cb;
    struct pollfd *fds;
    nfds_t nfds;
};
typedef struct ptl_cm_client_t ptl_cm_client_t;


static int
cm_server_rebuild_pollfds(ptl_cm_server_handle_t cm_h)
{
    int i, cnt;

    if (NULL != cm_h->fds) free(cm_h->fds);

    for (i = 0, cnt = 0 ; i < MAX_CONNECTIONS ; ++i) {
        if (cm_h->connections[i].fd >= 0) {
            cnt++;
        }
    }

    cm_h->nfds = cnt + 1;
    cm_h->fds = malloc(sizeof(struct pollfd) * cm_h->nfds);
    if (NULL == cm_h->fds) return -1;

    cm_h->fds[0].fd = cm_h->listenfd;
    cm_h->fds[0].events = POLLIN;
    cnt = 1;

    for (i = 0 ; i < MAX_CONNECTIONS ; ++i) {
        if (cm_h->connections[i].fd >= 0) {
            cm_h->fds[cnt].fd = cm_h->connections[i].fd;
            cm_h->fds[cnt].events = POLLIN;
            cnt++;
        }
    }

    return 0;
}

static int
cm_server_accept_connection(ptl_cm_server_handle_t cm_h)
{
    int fd, i;
    int32_t rank = -1;
    ssize_t nwr;

    fd = accept(cm_h->listenfd, NULL, NULL);
    if (fd < 0) return fd;

    for (i = 0 ; i < MAX_CONNECTIONS ; ++i) {
        if (cm_h->connections[i].fd < 0) {
            cm_h->connections[i].fd = fd;
            rank = i  + 1;
            break;
        }
    }

    nwr = write(cm_h->connections[rank - 1].fd, &rank, sizeof(rank));
    if (nwr <= 0) return -1;

    return 0;
}


static int
cm_send(int fd, void *buf, size_t len)
{
    uint64_t send_len = len;
    ssize_t nwr;
    struct iovec iov[2];

    iov[0].iov_base = &send_len;
    iov[0].iov_len = sizeof(send_len);
    iov[1].iov_base = buf;
    iov[1].iov_len = send_len;
        
    nwr = writev(fd, iov, 2);
    if (nwr < 0) return -1;

    return 0;
}

static int
cm_handle_recv(int fd, ptl_cm_recv_cb_t recv_cb, int remote_rank)
{
    uint64_t recv_len = 0;
    ssize_t nrd;
    void *buf;
    int ret = 0;

    nrd = read(fd, &recv_len, sizeof(recv_len));
    if (nrd < 0) return nrd;
    if (nrd == 0) return 1;

    buf = malloc(recv_len);
    /* BWB: FIX ME: need to suck down the message... */
    if (NULL == buf) return -1;

    nrd = read(fd, buf, recv_len);
    if (nrd < 0) return -1;

    if (NULL != recv_cb) {
        ret = recv_cb(remote_rank, buf, recv_len);
    }

    free(buf);

    return ret;
}


int
ptl_cm_server_create(ptl_cm_server_handle_t *cm_h)
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

    cm->connect_cb = NULL;
    cm->disconnect_cb = NULL;
    cm->recv_cb = NULL;

    cm->nfds = 1;
    cm->fds = malloc(sizeof(struct pollfd) * cm->nfds);
    if (NULL == cm->fds) goto cleanup;
    cm->fds[0].fd = cm->listenfd;
    cm->fds[0].events = POLLIN;

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

    free(cm_h->fds);

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
    cm_h->connect_cb = cb;
    return 0;
}


int
ptl_cm_server_register_disconnect_cb(ptl_cm_server_handle_t cm_h, 
                                     ptl_cm_disconnect_cb_t cb)
{
    cm_h->disconnect_cb = cb;
    return 0;
}


int
ptl_cm_server_register_recv_cb(ptl_cm_server_handle_t cm_h, 
                               ptl_cm_recv_cb_t cb)
{
    cm_h->recv_cb = cb;
    return 0;
}


int
ptl_cm_server_progress(ptl_cm_server_handle_t cm_h)
{
    int ret, cnt, i, j, need_rebuild = 0;
    int err;

    cnt = poll(cm_h->fds, cm_h->nfds, 0);
    if (cnt == -1) return cnt;

    for (i = 0 ; i < cm_h->nfds && cnt > 0 ; ++i) {
        if (cm_h->fds[i].revents == 0) continue;

        if (i == 0) {
            ret = cm_server_accept_connection(cm_h);
            if (ret < 0) return ret;
            need_rebuild = 1;
        } else {
            for (j = 0 ; j < MAX_CONNECTIONS ; ++j) {
                if (cm_h->fds[i].fd != cm_h->connections[j].fd) continue;
                if (cm_h->fds[i].revents & POLLERR ||
                    cm_h->fds[i].revents & POLLHUP) {
                    ret = cm_h->disconnect_cb(j + 1);
                    err = errno;
                    close(cm_h->connections[j].fd);
                    cm_h->connections[j].fd = -1;
                    errno = err;
                    if (ret < 0) return ret;
                    need_rebuild = 1;
                } else if (cm_h->fds[i].revents & POLLIN) {
                    ret = cm_handle_recv(cm_h->fds[i].fd, cm_h->recv_cb, j + 1);
                    if (ret < 0) return ret;
                    if (ret == 1) {
                        ret = cm_h->disconnect_cb(j + 1);
                        err = errno;
                        close(cm_h->connections[j].fd);
                        cm_h->connections[j].fd = -1;
                        errno = err;
                        if (ret < 0) return ret;
                        need_rebuild = 1;
                    }
                }
            }
        }
        --cnt;
    }

    if (need_rebuild) {
        ret = cm_server_rebuild_pollfds(cm_h);
        if (ret < 0) return ret;
    }

    return 0;
}


int
ptl_cm_server_send(ptl_cm_server_handle_t cm_h, int remote_id,
                   void *buf, size_t len)
{
    int peerfd;

    peerfd = cm_h->connections[remote_id].fd;
    if (peerfd < 0) {
        errno = EINVAL;
        return -1;
    }

    return cm_send(peerfd, buf, len);
}


int
ptl_cm_client_connect(ptl_cm_client_handle_t *cm_h, int *my_id)
{
    ptl_cm_client_t *cm;
    struct sockaddr_in addr;
    int ret, port = 1423;
    ssize_t nrd;

    if (NULL != getenv("PTL_CM_SERVER_PORT")) {
        port = atoi(getenv("PTL_CM_SERVER_PORT"));
    }

    cm = malloc(sizeof(ptl_cm_client_t));
    if (NULL == cm) return -1;

    cm->fds = NULL;

    cm->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cm->fd < 0) goto cleanup;

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = port;

    ret = connect(cm->fd, (struct sockaddr*) &addr, sizeof(addr));
    if (ret < 0) goto cleanup;

    cm->disconnect_cb = NULL;
    cm->recv_cb = NULL;

    cm->nfds = 1;
    cm->fds = malloc(sizeof(struct pollfd) * cm->nfds);
    if (NULL == cm->fds) goto cleanup;
    cm->fds[0].fd = cm->fd;
    cm->fds[0].events = POLLIN;

    nrd = read(cm->fd, &cm->my_rank, sizeof(cm->my_rank));
    if (nrd < 0) goto cleanup;

    if (cm->my_rank <= 0) {
        errno = EBUSY;
        return -1;
    }
    
    *cm_h = cm;
    *my_id = cm->my_rank;

    return 0;

 cleanup:
    {
        int err = errno;
        if (NULL != cm->fds) free(cm->fds);
        if (cm->fd >= 0) close(cm->fd);
        if (NULL != cm) free(cm);
        errno = err;
    }
    return -1;
}


int
ptl_cm_client_disconnect(ptl_cm_client_handle_t cm_h)
{
    if (NULL != cm_h->fds) free(cm_h->fds);
    if (cm_h->fd >= 0) close(cm_h->fd);
    if (NULL != cm_h) free(cm_h);

    return 0;
}


int
ptl_cm_client_register_disconnect_cb(ptl_cm_client_handle_t cm_h,
                                     ptl_cm_disconnect_cb_t cb)
{
    cm_h->disconnect_cb = cb;
    return 0;
}

int
ptl_cm_client_register_recv_cb(ptl_cm_client_handle_t cm_h, 
                               ptl_cm_recv_cb_t cb)
{
    cm_h->recv_cb = cb;
    return 0;
}


int
ptl_cm_client_progress(ptl_cm_client_handle_t cm_h)
{
    int ret, cnt;

    cnt = poll(cm_h->fds, cm_h->nfds, 0);
    if (cnt < 1) return cnt;

    if (cm_h->fds[0].revents == 0) return 0;

    if (cm_h->fds[0].revents & POLLERR ||
        cm_h->fds[0].revents & POLLHUP) {
        int err;
        ret = cm_h->disconnect_cb(0);
        err = errno;
        close(cm_h->fd);
        cm_h->fd = -1;
        errno = err;
        if (ret < 0) return ret;
    } else if (cm_h->fds[0].revents & POLLIN) {
        ret = cm_handle_recv(cm_h->fds[0].fd, cm_h->recv_cb, 0);
        if (ret < 0) return ret;
    }

    return 0;
}


int
ptl_cm_client_send(ptl_cm_client_handle_t cm_h,
                   void *buf, size_t len)
{
    return cm_send(cm_h->fd, buf, len);
}

