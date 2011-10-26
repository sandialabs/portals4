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
	pthread_mutex_t	mutex;

int xi_setup(void *arg);
void xi_cleanup(void *arg);

/* target side transaction descriptor */
typedef struct xt {
	obj_t			obj;
	PTL_BASE_XX

	// todo: remove all
	struct list_head	list;
	struct buf *ack_buf;		/* remote ACK is requested */ \
	struct buf *send_buf;			\



	struct list_head	rdma_list; \
	pthread_spinlock_t	rdma_list_lock; \

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

	/* Indicate whether the XT owns the NI atomic mutex. */
	int in_atomic;

} xt_t;

int xt_setup(void *arg);
void xt_cleanup(void *arg);

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

#endif /* PTL_XX_H */
