#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "ptl_command_queue.h"

struct ptl_cqe_t {
    int count;
};

int start = 10;
int loops = 10;

static int
server(int fd)
{
    int ret, i;
    ptl_cq_handle_t cq_h;
    ptl_cq_info_t *info, *remote_info;
    size_t info_len;
    ptl_cqe_t *send_entry, recv_entry;

    ret = ptl_cq_create(sizeof(ptl_cqe_t), 1, 2, 0, &cq_h);
    if (ret != 0) {
        fprintf(stderr, "server: ptl_cq_create: %d\n", ret);
        return ret;
    }

    ret = ptl_cq_info_get(cq_h, &info, &info_len);
    if (ret != 0) {
        fprintf(stderr, "server: ptl_cq_info_get: %d\n", ret);
        return ret;
    }

    ret = write(fd, info, info_len);
    if (ret < 0) {
        perror("write");
        return ret;
    }
    remote_info = malloc(info_len);
    if (NULL == remote_info) {
        perror("malloc");
        return 1;
    }
    ret = read(fd, remote_info, info_len);
    if (ret < 0) {
        perror("read");
        return ret;
    }

    ret = ptl_cq_attach(cq_h, remote_info);
    if (ret != 0) {
        fprintf(stderr, "server: ptl_cq_info_get: %d\n", ret);
        return ret;
    }

    recv_entry.count = start;

    for (i = 0 ; i < loops ; ++i) {
        do {
            ret = ptl_cq_entry_alloc(cq_h, &send_entry);
        } while (ret == 1);
        if (ret != 0) {
            fprintf(stderr, "server(%d): ptl_cq_entry_alloc: %d\n", i, ret);
            return ret;
        }

        send_entry->count = recv_entry.count + 1;
        fprintf(stderr, "server(%d): send_entry->count=%d\n", i, send_entry->count);
        ret = ptl_cq_entry_send(cq_h, 1, send_entry, sizeof(ptl_cqe_t));
        if (ret != 0) {
            fprintf(stderr, "server(%d): ptl_cq_entry_send: %d\n", i, ret);
            return ret;
        }

        do {
            ret = ptl_cq_entry_recv(cq_h, &recv_entry);
        } while (ret == 1);
        if (ret != 0) {
            fprintf(stderr, "server(%d): ptl_cq_entry_recv: %d\n", i, ret);
            return ret;
        }
        fprintf(stderr, "server(%d): recv_entry->count=%d\n", i, recv_entry.count);
    }

    ret = 0;
    ret = write(fd, &ret, sizeof(ret));
    if (ret < 0) {
        perror("write completion");
        return ret;
    }

    ptl_cq_destroy(cq_h);

    return (recv_entry.count == (start + loops * 2));
}


static int
client(int fd)
{
    int ret, i;
    ptl_cq_handle_t cq_h;
    ptl_cq_info_t *info, *remote_info;
    size_t info_len;
    ptl_cqe_t *send_entry, recv_entry;
    int count;

    ret = ptl_cq_create(sizeof(ptl_cqe_t), 1, 2, 1, &cq_h);
    if (ret != 0) {
        fprintf(stderr, "client: ptl_cq_create: %d\n", ret);
        return ret;
    }

    ret = ptl_cq_info_get(cq_h, &info, &info_len);
    if (ret != 0) {
        fprintf(stderr, "client: ptl_cq_info_get: %d\n", ret);
        return ret;
    }

    remote_info = malloc(info_len);
    if (NULL == remote_info) {
        perror("malloc");
        return 1;
    }
    ret = read(fd, remote_info, info_len);
    if (ret < 0) {
        perror("read");
        return ret;
    }
    ret = write(fd, info, info_len);
    if (ret < 0) {
        perror("write");
        return ret;
    }

    ret = ptl_cq_attach(cq_h, remote_info);
    if (ret != 0) {
        fprintf(stderr, "client: ptl_cq_info_get: %d\n", ret);
        return ret;
    }

    for (i = 0 ; i < loops ; ++i) {
        do {
            ret = ptl_cq_entry_recv(cq_h, &recv_entry);
        } while (ret == 1);
        if (ret != 0) {
            fprintf(stderr, "client(%d): ptl_cq_entry_recv: %d\n", i, ret);
            return ret;
        }
        count = recv_entry.count;
        fprintf(stderr, "client(%d): recv_entry->count=%d\n", i, count);

        do {
            ret = ptl_cq_entry_alloc(cq_h, &send_entry);
        } while (ret == 1);
        if (ret != 0) {
            fprintf(stderr, "client(%d): ptl_cq_entry_alloc: %d\n", i, ret);
            return ret;
        }

        send_entry->count = count + 1;
        printf("client(%d): send_entry->count=%d\n", i, send_entry->count);
        ret = ptl_cq_entry_send(cq_h, 0, send_entry, sizeof(ptl_cqe_t));
        if (ret != 0) {
            fprintf(stderr, "client(%d): ptl_cq_entry_send: %d\n", i, ret);
            return ret;
        }
    }

    ret = read(fd, &ret, sizeof(ret));
    if (ret < 0) {
        perror("read completion");
        return ret;
    }

    ptl_cq_destroy(cq_h);

    return 0;    
}


int
main(int argc, char *argv[])
{
    int ret;
    int fds[2];
    pid_t pid;

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
