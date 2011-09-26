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
	BUF_RDMA,

	BUF_SHMEM,
	BUF_SHMEM_STOP,
	BUF_SHMEM_RETURN,
} buf_type_t;

#define BUF_DATA_SIZE 1024

typedef struct buf {
	obj_t			obj;

	/* To hang on NI's send_list or recv_list. */
	struct list_head	list;

	union {
		struct xi		*xi;
		struct xt		*xt;
	};

	struct xremote *dest;

	/* Valid length in data. */
	unsigned int		length;

	/* Internal buffer for short messages (send or receive). */
	uint8_t	internal_data[BUF_DATA_SIZE];

	/* Usually points to internal_data, except with SHMEM where it
	 * might be the data in another buffer */
	void *data;

	union {
		struct {
			union {
				struct ibv_send_wr	send_wr;
				struct ibv_recv_wr	recv_wr;
			};

			/* SG list to register the data field. */
			struct ibv_sge		sg_list[1];

		} rdma;

		struct {
			ptl_rank_t source;	/* local rank owning that buffer */
		} shmem;
	};

	int comp;

	buf_type_t type;

	/* List of MRs used with that buffer for a long message. */
	int num_mr;
	mr_t *mr_list[0];

} buf_t;

/* Returns the size a buf_t is really taking. That's buf_t itself plus
 * the list of MRs. */
static inline size_t real_buf_t_size(void)
{
	int max_sge = get_param(PTL_MAX_QP_SEND_SGE);

	return sizeof(buf_t) + max_sge * sizeof(mr_t *);
}

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
	return obj_put(&buf->obj);
}

static inline ptl_handle_buf_t buf_to_handle(buf_t *buf)
{
	return (ptl_handle_buf_t)buf->obj.obj_handle;
}

/* buffers in shared memory. */
static inline int sbuf_alloc(ni_t *ni, buf_t **buf_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->sbuf_pool, &obj);
	if (err) {
		*buf_p = NULL;
		return err;
	}

	*buf_p = container_of(obj, buf_t, obj);
	return PTL_OK;
}

static inline int to_sbuf(ptl_handle_buf_t buf_handle, buf_t **buf_p)
{
	int err;
	obj_t *obj;

	err = to_obj(POOL_SBUF, (ptl_handle_any_t)buf_handle, &obj);
	if (err) {
		*buf_p = NULL;
		return err;
	}

	*buf_p = container_of(obj, buf_t, obj);
	return PTL_OK;
}

int ptl_post_recv(ni_t *ni);

void buf_dump(buf_t *buf);

#endif /* PTL_BUF_H */
