/*
 * ptl_buf - work request
 */

#ifndef PTL_BUF_H
#define PTL_BUF_H

struct xi;
struct xt;

extern obj_type_t *type_buf;

typedef ptl_handle_any_t ptl_handle_buf_t;

typedef enum {
	BUF_SEND,
	BUF_RECV,
	BUF_RDMA
} buf_type_t;

typedef struct buf {
	PTL_BASE_OBJ

	/* To hang on NI's send_list or recv_list. */
	struct list_head	list;

	struct xi		*xi;
	struct xt		*xt;

	struct xremote *dest;

	unsigned int		size;
	unsigned int		length;
	uint8_t			data[512];
	mr_t			*mr;

	union {
		struct ibv_send_wr	send_wr;
		struct ibv_recv_wr	recv_wr;
	};

	buf_type_t		type;

	struct ibv_sge		sg_list[1];
} buf_t;

void buf_init(void *arg);
void buf_release(void *arg);

static inline int buf_alloc(ni_t *ni, buf_t **buf_p)
{
	return obj_alloc(type_buf, (obj_t *)ni, (obj_t **)buf_p);
}

static inline int buf_get(ptl_handle_buf_t buf_handle, buf_t **buf_p)
{
	return obj_get(type_buf, (ptl_handle_any_t)buf_handle, (obj_t **)buf_p);
}

static inline void buf_ref(buf_t *buf)
{
	obj_ref((obj_t *)buf);
}

static inline int buf_put(buf_t *buf)
{
	return obj_put((obj_t *)buf);
}

static inline ptl_handle_buf_t buf_to_handle(buf_t *buf)
{
        return (ptl_handle_buf_t)buf->obj_handle;
}

int post_recv(ni_t *ni);

void buf_dump(buf_t *buf);

#endif /* PTL_BUF_H */
