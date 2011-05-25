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
	XI_SEND_EVENT		= (1 << 0),
	XI_ACK_EVENT		= (1 << 1),
	XI_REPLY_EVENT		= (1 << 2),
	XI_CT_SEND_EVENT	= (1 << 3),
	XI_CT_ACK_EVENT		= (1 << 4),
	XI_CT_REPLY_EVENT	= (1 << 5),

	XT_COMM_EVENT		= (1 << 0),
	XT_CT_COMM_EVENT	= (1 << 1),
	XT_ACK_EVENT		= (1 << 2),
	XT_REPLY_EVENT		= (1 << 3),
};

struct xremote {
	struct ibv_qp *qp;					   /* from RDMA CM */
#ifdef USE_XRC
	uint32_t xrc_remote_srq_num;
#endif
};

#define PTL_BASE_XX					\
	struct list_head	list;			\
	pthread_spinlock_t	recv_lock;		\
	pthread_spinlock_t	send_lock;		\
	struct buf		*send_buf;		\
	struct buf		*recv_buf;		\
	struct buf		*rdma_buf;		\
	ptl_size_t		rlength;		\
	ptl_size_t		mlength;		\
	ptl_size_t		roffset;		\
	ptl_size_t		moffset;		\
	ptl_size_t		put_resid;		\
	ptl_size_t		get_resid;		\
	unsigned int		event_mask;		\
	struct ibv_sge		*cur_rem_sge;		\
	ptl_size_t		num_rem_sge;		\
	ptl_size_t		cur_rem_off;		\
	ptl_size_t		cur_loc_iov_index;	\
	ptl_size_t		cur_loc_iov_off;	\
	uint32_t  		rdma_dir;		\
	int			rdma_comp;		\
	int			interim_rdma;		\
	pthread_spinlock_t	state_lock;		\
	int			state;			\
	int			next_state;		\
	int			state_again;		\
	int			state_waiting;		\
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
	ptl_jid_t		jid;			\
	ptl_ni_fail_t		ni_fail;		\
	ptl_size_t		threshold;		\
	struct data		*data_in;		\
	struct data		*data_out;		\
	conn_t			*conn;

/* initiator side transaction descriptor */
typedef struct xi {
	obj_t			obj;
	PTL_BASE_XX

	ptl_handle_xt_t		xt_handle;
	ptl_size_t		put_offset;
	ptl_size_t		get_offset;
	md_t			*put_md;
	md_t			*get_md;
	void			*user_ptr;
	ptl_process_t		target;

	/* This xi is waiting for connection to be established. */

	struct xremote		dest;


} xi_t;

int xi_init(void *arg, void *parm);
void xi_fini(void *arg);
int xi_new(void *arg);

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

static inline int xi_get(ptl_handle_xi_t xi_handle, xi_t **xi_p)
{
	int err;
	obj_t *obj;

	err = obj_get(POOL_XI, (ptl_handle_any_t)xi_handle, &obj);
	if (err) {
		*xi_p = NULL;
		return err;
	}

	*xi_p = container_of(obj, xi_t, obj);
	return PTL_OK;
}

static inline void xi_ref(xi_t *xi)
{
	obj_ref(&xi->obj);
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
	union {
		le_t			*le;
		me_t			*me;
	};

	/* This xt is waiting for connection to be established. */
	struct list_head	connect_pending_list;

	/* used to put xt on unexpected list */
	struct list_head	unexpected_list;

	struct xremote dest;

	struct ibv_sge		*indir_sge;
	mr_t			*indir_mr;
} xt_t;

int xt_init(void *arg, void *parm);
void xt_fini(void *arg);
int xt_new(void *arg);

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

static inline int xt_get(ptl_handle_xt_t xt_handle, xt_t **xt_p)
{
	int err;
	obj_t *obj;

	err = obj_get(POOL_XT, (ptl_handle_any_t)xt_handle, &obj);
	if (err) {
		*xt_p = NULL;
		return err;
	}

	*xt_p = container_of(obj, xt_t, obj);
	return PTL_OK;
}

static inline void xt_ref(xt_t *xt)
{
	obj_ref(&xt->obj);
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
	ni_t *ni = to_ni(xi);

	xi->dest.qp = connect->cm_id->qp;
#ifdef USE_XRC
	if (ni->options & PTL_NI_LOGICAL)
		xi->dest.xrc_remote_srq_num = ni->logical.rank_table[xi->target.rank].remote_xrc_srq_num;
#endif
}

static inline void set_xt_dest(xt_t *xt, conn_t *connect)
{
	ni_t *ni = to_ni(xt);

	xt->dest.qp = connect->cm_id->qp;
#ifdef USE_XRC
	if (ni->options & PTL_NI_LOGICAL)
		xt->dest.xrc_remote_srq_num = ni->logical.rank_table[xt->initiator.rank].remote_xrc_srq_num;
#endif
}

#endif /* PTL_XX_H */
