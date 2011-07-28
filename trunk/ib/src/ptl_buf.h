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

	union {
		struct xi		*xi;
		struct xt		*xt;
	};

	struct xremote *dest;

	/* Size of data to send. */
	unsigned int		length;

	/* Internal buffer for short messages (send or receive). */
	uint8_t			data[1024];

	union {
		struct {
			union {
				struct ibv_send_wr	send_wr;
				struct ibv_recv_wr	recv_wr;
			};

			/* HACK - Mellanox driver bug workaround. We get completions even
			 * if they were not requested. Happens on mthca and mlx4. Querying
			 * a QP after it's created indicates that sq_sig_all is set
			 * although it was requested at creation. This variable should go
			 * away when the bug is fixed. */
			int comp;

			/* SG list to register the data field. */
			struct ibv_sge		sg_list[1];

		} ib;
	};

	buf_type_t		type;

	/* List of MRs used for that buffer in case of a long transfer. */
	int num_mr;
	mr_t **mr_list;

	char internal_data[0];

} buf_t;

int buf_setup(void *arg);
int buf_init(void *arg, void *parm);
void buf_cleanup(void *arg);

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

static inline int to_buf(ptl_handle_buf_t buf_handle, buf_t **buf_p)
{
	int err;
	obj_t *obj;

	err = to_obj(POOL_BUF, (ptl_handle_any_t)buf_handle, &obj);
	if (err) {
		*buf_p = NULL;
		return err;
	}

	*buf_p = container_of(obj, buf_t, obj);
	return PTL_OK;
}

static inline void buf_get(buf_t *buf)
{
	obj_get(&buf->obj);
}

static inline int buf_put(buf_t *buf)
{
	if (buf->xi)
		xi_put(buf->xi);
	return obj_put(&buf->obj);
}

static inline ptl_handle_buf_t buf_to_handle(buf_t *buf)
{
        return (ptl_handle_buf_t)buf->obj.obj_handle;
}

int ptl_post_recv(ni_t *ni);

void buf_dump(buf_t *buf);

#endif /* PTL_BUF_H */
