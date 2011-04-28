/*
 * ptl_buf - work request
 */

#ifndef PTL_BUF_H
#define PTL_BUF_H

struct xi;
struct xt;

typedef ptl_handle_any_t ptl_handle_buf_t;

typedef enum {
	BUF_SEND,
	BUF_RECV,
	BUF_RDMA
} buf_type_t;

typedef struct buf {
	obj_t			obj;

	/* To hang on NI's send_list or recv_list. */
	struct list_head	list;

	struct xi		*xi;
	struct xt		*xt;

	struct xremote *dest;

	unsigned int		size;
	unsigned int		length;
	uint8_t			data[512];

	union {
		struct ibv_send_wr	send_wr;
		struct ibv_recv_wr	recv_wr;
	};

	buf_type_t		type;

	struct ibv_sge		sg_list[1];
} buf_t;

int buf_init(void *arg, void *parm);
void buf_release(void *arg);

static inline int buf_alloc(ni_t *ni, buf_t **buf_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->buf_pool, &obj);
	if (err) {
		*buf_p = NULL;
		return err;
	}

	*buf_p = container_of(obj, buf_t, obj);
	return PTL_OK;
}

static inline int buf_get(ptl_handle_buf_t buf_handle, buf_t **buf_p)
{
	int err;
	obj_t *obj;

	err = obj_get(POOL_BUF, (ptl_handle_any_t)buf_handle, &obj);
	if (err) {
		*buf_p = NULL;
		return err;
	}

	*buf_p = container_of(obj, buf_t, obj);
	return PTL_OK;
}

static inline void buf_ref(buf_t *buf)
{
	obj_ref(&buf->obj);
}

static inline int buf_put(buf_t *buf)
{
	return obj_put(&buf->obj);
}

static inline ptl_handle_buf_t buf_to_handle(buf_t *buf)
{
        return (ptl_handle_buf_t)buf->obj.obj_handle;
}

int ptl_post_recv(ni_t *ni);

void buf_dump(buf_t *buf);

#endif /* PTL_BUF_H */
