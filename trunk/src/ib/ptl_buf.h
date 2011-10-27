/**
 * @file ptl_buf.h
 *
 * Message buffer object declarations.
 */

#ifndef PTL_BUF_H
#define PTL_BUF_H

struct xt;

/**
 * Type of buf object.
 */
enum buf_type {
	BUF_FREE,
	BUF_SEND,
	BUF_RECV,
	BUF_RDMA,

	BUF_SHMEM,
	BUF_SHMEM_STOP,
	BUF_SHMEM_RETURN,
};

typedef enum buf_type buf_type_t;

#define BUF_DATA_SIZE 1024

/**
 * A buf struct holds information about a common
 * buffer object that is used for sending and receiving
 * messages, shared memory messages and RDMA read and
 * write operations.
 *
 * A buf struct includes room to hold either an OFA
 * verbs send or recv work request or info if it is
 * a shared memory message. It also includes a data buffer that
 * can hold a short message. Additionally there
 * is an array to hold a list of pointers to any memory
 * regions used by the message so that they can be freed after
 * the operation is completed.
 *
 * Bufs used by OFA verbs are allocated in a slab that has been
 * registered.
 */
struct buf {
	/** base object */
	obj_t			obj;

	/** enables holding buf on lists */
	struct list_head	list;

	/** the transaction to which this buffer is related */
	struct {
		/* initiator side transaction descriptor */
		struct {
			PTL_BASE_XX

			ptl_handle_xt_t		xt_handle;
			ptl_size_t		put_offset;
			ptl_size_t		get_offset;
			md_t			*put_md;
			struct eq		*put_eq;
			md_t			*get_md;
			struct eq		*get_eq;
			struct ct       *put_ct;
			struct ct       *get_ct;
			void		*user_ptr;
			ptl_process_t	target;
			int				completed;
		} xi;
		struct xt	*xt;
	};

	/** remote destination for message */
	struct xremote		*dest;

	/** message length */
	unsigned int		length;

	/** data to hold message */
	uint8_t			internal_data[BUF_DATA_SIZE];

	/** message (usually internal_data) */
	void			*data;

	union {
		struct {
			union {
				struct ibv_send_wr send_wr;
				struct ibv_recv_wr recv_wr;
			};

			/* SG list to register the data field. */
			struct ibv_sge sg_list[1];
		} rdma;

		struct {
			ptl_rank_t source;	/* local rank owning that buffer */
		} shmem;
	};

	/** verbs completion requested */
	int			comp;

	/** type of buf */
	buf_type_t		type;

	/** recv state */
	int			state;

	/** Send completion must be signaled. **/
	int			signaled;

	/** number of mr's used in message */
	int			num_mr;

	/** mr's used in message */
	mr_t			*mr_list[0];
};

typedef struct buf buf_t;

int buf_setup(void *arg);

int buf_init(void *arg, void *parm);

void buf_cleanup(void *arg);

int ptl_post_recv(ni_t *ni, int count);

void buf_dump(buf_t *buf);

/**
 * Compute the actual buf size.
 *
 * Account for room needed to hold MR addresses in the buf_t struct.
 *
 * @return size of buf
 */
static inline size_t real_buf_t_size(void)
{
	int max_sge = get_param(PTL_MAX_QP_SEND_SGE);

	return sizeof(buf_t) + max_sge * sizeof(mr_t *);
}

/**
 * Allocate a buf from the normal buf pool.
 *
 * @param ni from which to allocate the buf
 * @param buf_p pointer to return value
 *
 * @return status
 */
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

/**
 * Allocate a buf from the shared memory pool.
 *
 * @param ni from which to allocate the buf
 * @param buf_p pointer to return value
 *
 * @return status
 */
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

/**
 * Take a reference to a buf.
 *
 * @param buf on which to take a reference
 */
static inline void buf_get(buf_t *buf)
{
	obj_get(&buf->obj);
}

/**
 * Drop a reference to a buf
 *
 * If the last reference has been dropped the buf
 * will be freed.
 *
 * @param buf on which to drop a reference
 *
 * @return status
 */
static inline int buf_put(buf_t *buf)
{
	return obj_put(&buf->obj);
}

typedef ptl_handle_any_t ptl_handle_buf_t;

static inline ptl_handle_buf_t buf_to_handle(buf_t *buf)
{
	return (ptl_handle_buf_t)buf->obj.obj_handle;
}

static inline void set_buf_dest(buf_t *buf, const conn_t *connect)
{
#ifdef USE_XRC
	ni_t *ni = to_ni(xi);
#endif

	if (connect->transport.type == CONN_TYPE_RDMA) {
		buf->xi.dest.rdma.qp = connect->rdma.cm_id->qp;
#ifdef USE_XRC
		if (ni->options & PTL_NI_LOGICAL)
			buf->xi.dest.xrc_remote_srq_num = ni->logical.rank_table[buf->xi.target.rank].remote_xrc_srq_num;
#endif
	} else {
		assert(connect->transport.type == CONN_TYPE_SHMEM);
		assert(connect->shmem.local_rank != -1);
		buf->xi.dest.shmem.local_rank = connect->shmem.local_rank;
	}
}

static inline void set_xt_dest(xt_t *xt, const conn_t *connect)
{
#ifdef USE_XRC
	ni_t *ni = to_ni(xt);
#endif

	if (connect->transport.type == CONN_TYPE_RDMA) {
		xt->dest.rdma.qp = connect->rdma.cm_id->qp;
#ifdef USE_XRC
		if (ni->options & PTL_NI_LOGICAL)
			xt->dest.xrc_remote_srq_num = ni->logical.rank_table[xt->initiator.rank].remote_xrc_srq_num;
#endif
	} else {
		assert(connect->transport.type == CONN_TYPE_SHMEM);
		assert(connect->shmem.local_rank != -1);
		xt->dest.shmem.local_rank = connect->shmem.local_rank;
	}
}

static inline int to_buf(ptl_handle_buf_t handle, buf_t **buf_p)
{
	int err;
	obj_t *obj;

	/* The buffer can either be in POOL_BUF or POOL_SBUF. */
	err = to_obj(POOL_ANY, (ptl_handle_any_t)handle, &obj);
	if (err) {
		*buf_p = NULL;
		return err;
	}

	*buf_p = container_of(obj, buf_t, obj);
	return PTL_OK;
}

#endif /* PTL_BUF_H */
