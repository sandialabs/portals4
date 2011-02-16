/*
 * ptl_ni.h
 */

#ifndef PTL_NI_H
#define PTL_NI_H

/* These values will need to come from runtime environment */
#define MAX_QP_SEND_WR		(10)
#define MAX_QP_RECV_WR		(10)
#define MAX_SRQ_RECV_WR		(100)

extern obj_type_t *type_ni;

typedef struct ni {
	PTL_BASE_OBJ

	gbl_t			*gbl;

	ptl_ni_limits_t		limits;
	ptl_ni_limits_t		current;

	struct {
		uint32_t		nid;
		uint32_t		pid;
	}			*map;

	int			ref_cnt;

	unsigned int		iface;
	unsigned int		options;
	unsigned int		ni_type;
	int			slot;

	ptl_sr_value_t		status[_PTL_SR_LAST];

	ptl_size_t		num_recv_pkts;
	ptl_size_t		num_recv_bytes;
	ptl_size_t		num_recv_errs;
	ptl_size_t		num_recv_drops;

	pt_t			*pt;
	pthread_mutex_t		pt_mutex;
	ptl_pt_index_t		last_pt;

	struct list_head	md_list;
	pthread_spinlock_t	md_list_lock;

	struct list_head	ct_list;
	pthread_spinlock_t	ct_list_lock;

	struct list_head	xi_wait_list;
	pthread_spinlock_t	xi_wait_list_lock;

	struct list_head	xt_wait_list;
	pthread_spinlock_t	xt_wait_list_lock;

	struct list_head	mr_list;
	pthread_spinlock_t	mr_list_lock;

	/*Can be held outside of EQ object lock */
	pthread_mutex_t		eq_wait_mutex;
	pthread_cond_t		eq_wait_cond;
	int			eq_waiting;

	/*Can be held outside of CT object lock */
	pthread_mutex_t		ct_wait_mutex;
	pthread_cond_t		ct_wait_cond;
	int			ct_waiting;

	/* simulation code */
	struct list_head	send_list;
	pthread_spinlock_t	send_list_lock;

	union {
	uint32_t		nid;
	uint32_t		rank;
	};
	uint32_t		pid;
	ptl_uid_t		uid;

	/* IB */
	struct ibv_context	*ibv_context;
	struct ibv_pd		*pd;
	struct ibv_cq		*cq;
	struct ibv_comp_channel	*ch;

	/* IB XRC support. */
	int			xrc_domain_fd;
	struct ibv_xrc_domain	*xrc_domain;
	struct ibv_srq		*xrc_srq;
	uint32_t		xrc_rcv_qpn;

	/* rc simulation code */
	struct ibv_qp		*qp;

	pthread_t		recv_thread;
	int			has_recv_thread;
	int			recv_run;

} ni_t;

static inline int ni_alloc(ni_t **ni_p)
{
	return obj_alloc(type_ni, NULL, (obj_t **)ni_p);
}

static inline void ni_ref(ni_t *ni)
{
	obj_ref((obj_t *)ni);
}

static inline int ni_put(ni_t *ni)
{
	return obj_put((obj_t *)ni);
}

static inline int ni_get(ptl_handle_ni_t handle, ni_t **ni_p)
{
	int err;
	ni_t *ni;

	err = obj_get(type_ni, (ptl_handle_any_t)handle, (obj_t **)&ni);
	if (err)
		return err;

	/* this is because we can call PtlNIFini
	   and still get the object if someone
	   is holding a reference */
	if (ni && ni->ref_cnt <= 0) {
		ni_put(ni);
		return PTL_ARG_INVALID;
	}

	*ni_p = ni;
	return PTL_OK;
}

static inline ptl_handle_ni_t ni_to_handle(ni_t *ni)
{
	return (ptl_handle_ni_t)ni->obj_handle;
}

static inline ni_t *to_ni(void *obj)
{
	return ((obj_t *)obj)->obj_ni;
}

static inline void ni_inc_status(ni_t *ni, ptl_sr_index_t index)
{
	if (index < _PTL_STATUS_LAST) {
		pthread_spin_lock(&ni->obj_lock);
		ni->status[index]++;
		pthread_spin_unlock(&ni->obj_lock);
	}
}

#endif /* PTL_NI_H */
