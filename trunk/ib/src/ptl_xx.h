/*
 * ptl_xx - transaction descriptors
 */

#ifndef PTL_XX_H
#define PTL_XX_H

struct buf;
typedef ptl_handle_any_t ptl_handle_xi_t;
typedef ptl_handle_any_t ptl_handle_xt_t;

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

#define PTL_BASE_XX					\
	struct list_head	list;			\
	struct buf		*recv_buf;		\
	ptl_size_t		rlength;		\
	ptl_size_t		mlength;		\
	ptl_size_t		roffset;		\
	ptl_size_t		moffset;		\
	ptl_size_t		put_resid;		\
	ptl_size_t		get_resid;		\
	unsigned int		event_mask;		\
	struct {								\
		struct ibv_sge		*cur_rem_sge;	\
		ptl_size_t		num_rem_sge;		\
		ptl_size_t		cur_rem_off;		\
		atomic_t 		rdma_comp;			\
		int			interim_rdma;			\
	} rdma;									\
	struct {								\
		struct shmem_iovec *cur_rem_iovec;	\
		ptl_size_t		num_rem_iovecs;		\
		ptl_size_t		cur_rem_off;		\
	} shmem;								\
	ptl_size_t		cur_loc_iov_index;		\
	ptl_size_t		cur_loc_iov_off;	\
	uint32_t  		rdma_dir;		\
	int			state;			\
	int			next_state;		\
	struct xremote		dest;		\
	unsigned		operation;		\
	int			pkt_len;		\
	ptl_pt_index_t		pt_index;		\
	ptl_match_bits_t	match_bits;		\
	ptl_ack_req_t		ack_req;		\
	uint64_t		hdr_data;		\
	uint64_t		operand;		\
	ptl_op_t		atom_op;		\
	ptl_datatype_t		atom_type;		\
	ptl_uid_t		uid;			\
	ptl_ni_fail_t		ni_fail;		\
	ptl_size_t		threshold;		\
	struct data		*data_in;		\
	struct data		*data_out;		\
	conn_t			*conn;			\
	struct buf *ack_buf;		/* remote ACK is requested */ \
	struct list_head	rdma_list; \
	pthread_spinlock_t	rdma_list_lock; \
	pthread_mutex_t	mutex;


/* initiator side transaction descriptor */
typedef struct xi {
	obj_t			obj;
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
} xi_t;

int xi_setup(void *arg);

static inline int xi_alloc(ni_t *ni, xi_t **xi_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->xi_pool, &obj);
	if (err) {
		*xi_p = NULL;
		return err;
	}

	*xi_p = container_of(obj, xi_t, obj);
	return PTL_OK;
}

static inline int to_xi(ptl_handle_xi_t xi_handle, xi_t **xi_p)
{
	int err;
	obj_t *obj;

	err = to_obj(POOL_XI, (ptl_handle_any_t)xi_handle, &obj);
	if (err) {
		*xi_p = NULL;
		return err;
	}

	*xi_p = container_of(obj, xi_t, obj);
	return PTL_OK;
}

static inline void xi_get(xi_t *xi)
{
	obj_get(&xi->obj);
}

static inline int xi_put(xi_t *xi)
{
	return obj_put(&xi->obj);
}

static inline ptl_handle_xi_t xi_to_handle(xi_t *xi)
{
	return (ptl_handle_xi_t)xi->obj.obj_handle;
}

/* target side transaction descriptor */
typedef struct xt {
	obj_t			obj;
	PTL_BASE_XX

	ptl_handle_xi_t		xi_handle;
	ptl_process_t		initiator;
	pt_t			*pt;

	/* ME/LE used to process this XT. */
	union {
		le_t			*le;
		me_t			*me;
	};

	/* If the XT is on the overflow list, and a matching PtlMEAppend
	 * occurs while being processed, remember which one matches. */
	union {
		le_t			*le;
		me_t			*me;
	} matching;

	/* used to put xt on unexpected list */
	struct list_head	unexpected_list;

	/* Holds the indirect SGE or KNEM iovecs. indir_mr describes the
	 * memory allocated.  */
	void			*indir_sge;
	mr_t			*indir_mr;

	void *start;

	/* ack or reply buffer */
	struct buf *send_buf;

} xt_t;

int xt_setup(void *arg);

static inline int xt_alloc(ni_t *ni, xt_t **xt_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->xt_pool, &obj);
	if (err) {
		*xt_p = NULL;
		return err;
	}

	*xt_p = container_of(obj, xt_t, obj);
	return PTL_OK;
}

static inline int to_xt(ptl_handle_xt_t xt_handle, xt_t **xt_p)
{
	int err;
	obj_t *obj;

	err = to_obj(POOL_XT, (ptl_handle_any_t)xt_handle, &obj);
	if (err) {
		*xt_p = NULL;
		return err;
	}

	*xt_p = container_of(obj, xt_t, obj);
	return PTL_OK;
}

static inline void xt_get(xt_t *xt)
{
	obj_get(&xt->obj);
}

static inline int xt_put(xt_t *xt)
{
	return obj_put(&xt->obj);
}

static inline ptl_handle_xt_t xt_to_handle(xt_t *xt)
{
        return (ptl_handle_xt_t)xt->obj.obj_handle;
}

static inline void set_xi_dest(xi_t *xi, conn_t *connect)
{
#ifdef USE_XRC
	ni_t *ni = to_ni(xi);
#endif

	if (connect->transport.type == CONN_TYPE_RDMA) {
		xi->dest.rdma.qp = connect->rdma.cm_id->qp;
#ifdef USE_XRC
		if (ni->options & PTL_NI_LOGICAL)
			xi->dest.xrc_remote_srq_num = ni->logical.rank_table[xi->target.rank].remote_xrc_srq_num;
#endif
	} else {
		assert(connect->transport.type == CONN_TYPE_SHMEM);
		assert(connect->shmem.local_rank != -1);
		xi->dest.shmem.local_rank = connect->shmem.local_rank;
	}
}

static inline void set_xt_dest(xt_t *xt, conn_t *connect)
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

#endif /* PTL_XX_H */
