/*
 * ptl_xx - transaction descriptors
 */

#ifndef PTL_XX_H
#define PTL_XX_H

#define MAX_RDMA_WR_OUT		(4)

struct buf;
typedef ptl_handle_any_t ptl_handle_xi_t;
typedef ptl_handle_any_t ptl_handle_xt_t;

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
} event_mask_t;

struct xremote {
	struct ibv_qp *qp;					   /* from RDMA CM */
	uint32_t xrc_remote_srq_num;
};

#define PTL_BASE_XX					\
	struct list_head	list;			\
	struct list_head	recv_list;		\
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
	struct data		*data_out;

/* initiator side transaction descriptor */
typedef struct xi {
	PTL_BASE_OBJ
	PTL_BASE_XX

	ptl_handle_xt_t		xt_handle;
	ptl_size_t		put_offset;
	ptl_size_t		get_offset;
	md_t			*put_md;
	md_t			*get_md;
	void			*user_ptr;
	ptl_process_t		target;

	/* This xi is waiting for connection to be established. */
	struct list_head connect_pending_list;

	struct xremote dest;

} xi_t;

extern obj_type_t *type_xi;
void xi_init(void *arg);
void xi_release(void *arg);

static inline int xi_alloc(ni_t *ni, xi_t **xi_p)
{
	return obj_alloc(type_xi, (obj_t *)ni, (obj_t **)xi_p);
}

static inline int xi_get(ptl_handle_xi_t xi_handle, xi_t **xi_p)
{
	return obj_get(type_xi, (ptl_handle_any_t)xi_handle, (obj_t **)xi_p);
}

static inline void xi_ref(xi_t *xi)
{
	obj_ref((obj_t *)xi);
}

static inline int xi_put(xi_t *xi)
{
	return obj_put((obj_t *)xi);
}

static inline ptl_handle_xi_t xi_to_handle(xi_t *xi)
{
        return (ptl_handle_xi_t)xi->obj_handle;
}

/* target side transaction descriptor */
typedef struct xt {
	PTL_BASE_OBJ
	PTL_BASE_XX

	ptl_handle_xi_t		xi_handle;
	ptl_process_t		initiator;
	pt_t			*pt;
	union {
		le_t			*le;
		me_t			*me;
	};

	/* This xt is waiting for connection to be established. */
	struct list_head connect_pending_list;

	struct xremote dest;

	struct ibv_sge		*indir_sge;
	mr_t			*indir_mr;
} xt_t;

extern obj_type_t *type_xt;
void xt_init(void *arg);
void xt_release(void *arg);

static inline int xt_alloc(ni_t *ni, xt_t **xt_p)
{
	return obj_alloc(type_xt, (obj_t *)ni, (obj_t **)xt_p);
}

static inline int xt_get(ptl_handle_xt_t xt_handle, xt_t **xt_p)
{
	return obj_get(type_xt, (ptl_handle_any_t)xt_handle, (obj_t **)xt_p);
}

static inline void xt_ref(xt_t *xt)
{
	obj_ref((obj_t *)xt);
}

static inline int xt_put(xt_t *xt)
{
	return obj_put((obj_t *)xt);
}

static inline ptl_handle_xt_t xt_to_handle(xt_t *xt)
{
        return (ptl_handle_xt_t)xt->obj_handle;
}

static inline void set_xi_dest(xi_t *xi, struct nid_connect *connect)
{
	xi->dest.qp = connect->cm_id->qp;
	xi->dest.xrc_remote_srq_num = xi->obj_ni->shmem.rank_table->elem[xi->target.rank].xrc_srq_num;
}

static inline void set_xt_dest(xt_t *xt, struct nid_connect *connect)
{
	xt->dest.qp = connect->cm_id->qp;
	xt->dest.xrc_remote_srq_num = xt->obj_ni->shmem.rank_table->elem[xt->initiator.rank].xrc_srq_num;
}

#endif /* PTL_XX_H */
