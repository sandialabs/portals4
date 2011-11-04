/**
 * @file ptl_buf.h
 *
 * Message buffer object declarations.
 */

#ifndef PTL_BUF_H
#define PTL_BUF_H

/**
 * Type of buf object. It will change over the lifetime of the
 * buffer. For instance a buffer reserved by the initiator will have
 * the BUF_INIT, and will change to BUF_SEND when it is posted. */
enum buf_type {
	BUF_FREE,
	BUF_SEND,
	BUF_RECV,
	BUF_RDMA,

	BUF_SHMEM,
	BUF_SHMEM_STOP,
	BUF_SHMEM_RETURN,

	BUF_INIT,
	BUF_TGT
};

/* Event mask */
enum {
	XI_PUT_SUCCESS_DISABLE_EVENT= (1 << 0),
	XI_GET_SUCCESS_DISABLE_EVENT= (1 << 1),
	XI_SEND_EVENT				= (1 << 2),
	XI_ACK_EVENT				= (1 << 3),
	XI_REPLY_EVENT				= (1 << 4),
	XI_CT_SEND_EVENT			= (1 << 5),
	XI_PUT_CT_BYTES				= (1 << 6),
	XI_GET_CT_BYTES				= (1 << 7),
	XI_CT_ACK_EVENT				= (1 << 8),
	XI_CT_REPLY_EVENT			= (1 << 9),
	XI_RECEIVE_EXPECTED			= (1 << 10),

	XT_COMM_EVENT		= (1 << 0),
	XT_CT_COMM_EVENT	= (1 << 1),
	XT_ACK_EVENT		= (1 << 2),
	XT_REPLY_EVENT		= (1 << 3),
};

typedef enum buf_type buf_type_t;

struct xremote {
	union {
		struct {
			struct ibv_qp *qp;					   /* from RDMA CM */
#ifdef USE_XRC
			uint32_t xrc_remote_srq_num;
#endif
		} rdma;

		struct {
			ptl_rank_t local_rank;
		} shmem;
	};
};

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

	pthread_mutex_t		mutex;

	/** enables holding buf on lists */
	struct list_head	list;

	/* remains of xi/xt */
	/* TODO names with xx in front had collisions
	 * with other names already in buf, need to
	 * see if we can merge them */


	struct buf		*xxbuf;
	unsigned int	event_mask;

	ptl_size_t		mlength;	/* todo: may remove */
	ptl_size_t		moffset;	/* todo: may remove */
	ptl_ni_fail_t	ni_fail;	/* todo: may remove */

	conn_t			*conn;

	struct data		*data_in;
	struct data		*data_out;

	union {
		/* Initiator side only. */
		struct {
			int init_state;
			int			completed;
			ptl_process_t		target;
			ptl_size_t	ct_threshold;
			struct buf		*recv_buf;
			void			*user_ptr;

			struct md		*put_md;
			struct eq		*put_eq;
			struct md		*get_md;
			struct eq		*get_eq;
			struct ct		*put_ct;
			struct ct		*get_ct;
			ptl_size_t		put_offset;
			ptl_size_t		get_offset;
		};

		/* Target side only. */
		struct {
			int tgt_state;
			int			in_atomic;

			pt_t			*pt;
			void			*start;
			void			*get_indir_sge;
			void			*put_indir_sge;
			int			rdma_desc_ok;
			union {
				le_t			*le;
				me_t			*me;
			};
			union {
				le_t			*le;
				me_t			*me;
			} matching;
			struct buf		*send_buf;
			ptl_size_t		cur_loc_iov_index;
			ptl_size_t		cur_loc_iov_off;
			ptl_size_t		put_resid;
			ptl_size_t		get_resid;
			uint32_t		rdma_dir;
		};
	};

	/** recv state */
	int	recv_state;

	/** remote destination for message */
	struct xremote		dest;

	/** message length */
	unsigned int		length;

	/** message (usually internal_data) */
	void			*data;

	/** data to hold message */
	uint8_t			internal_data[BUF_DATA_SIZE];

	/** verbs completion requested */
	int			comp;

	/** type of buf */
	buf_type_t		type;

	/** Send completion must be signaled. **/
	int			signaled;

	/* Target only. Must be out of the union. */
	struct list_head	unexpected_list;
	struct list_head	rdma_list;
	pthread_spinlock_t	rdma_list_lock;

	union {
		struct {
			union {
				struct ibv_send_wr send_wr;
				struct ibv_recv_wr recv_wr;
			};

			/* SG list to register the internal_data field. */
			struct ibv_sge sg_list[1];

			/* For RDMA operations. */
			struct ibv_sge		*cur_rem_sge;
			ptl_size_t		num_rem_sge;
			ptl_size_t		cur_rem_off;
			atomic_t 		rdma_comp;
			int			interim_rdma;
		} rdma;

		struct {
			ptl_rank_t source;	/* local rank owning that buffer */

			/* For large (ie. KNEM) operations. */
			struct shmem_iovec	*cur_rem_iovec;
			ptl_size_t		num_rem_iovecs;
			ptl_size_t		cur_rem_off;
		} shmem;
	};

	/** number of mr's used in message */
	int			num_mr;

	/** mr's used in message */
	mr_t			*mr_list[0];
};

typedef struct buf buf_t;

int buf_setup(void *arg);

int buf_init(void *arg, void *parm);

void buf_fini(void *arg);

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
		buf->dest.rdma.qp = connect->rdma.cm_id->qp;
#ifdef USE_XRC
		if (ni->options & PTL_NI_LOGICAL)
			buf->dest.xrc_remote_srq_num = ni->logical.rank_table[buf->xi.target.rank].remote_xrc_srq_num;
#endif
	} else {
		assert(connect->transport.type == CONN_TYPE_SHMEM);
		assert(connect->shmem.local_rank != -1);
		buf->dest.shmem.local_rank = connect->shmem.local_rank;
	}
}

// TODO merge with set_buf_dest
static inline void set_tgt_dest(buf_t *buf, const conn_t *connect)
{
#ifdef USE_XRC
	ni_t *ni = to_ni(buf);
#endif

	if (connect->transport.type == CONN_TYPE_RDMA) {
		buf->dest.rdma.qp = connect->rdma.cm_id->qp;
#ifdef USE_XRC
		if (ni->options & PTL_NI_LOGICAL)
			buf->dest.xrc_remote_srq_num =
				ni->logical.rank_table[xt->
					initiator.rank].remote_xrc_srq_num;
#endif
	} else {
		assert(connect->transport.type == CONN_TYPE_SHMEM);
		assert(connect->shmem.local_rank != -1);
		buf->dest.shmem.local_rank = connect->shmem.local_rank;
	}
}

static inline int to_buf(ptl_handle_buf_t handle, buf_t **buf_p)
{
	obj_t *obj;

	/* The buffer can either be in POOL_BUF or POOL_SBUF. */
	obj = to_obj(POOL_ANY, (ptl_handle_any_t)handle);
	if (!obj) {
		*buf_p = NULL;
		return PTL_ARG_INVALID;
	}

	*buf_p = container_of(obj, buf_t, obj);
	return PTL_OK;
}

#endif /* PTL_BUF_H */
