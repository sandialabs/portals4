/*
 * ptl_ni.h
 */

#ifndef PTL_NI_H
#define PTL_NI_H

/*
 * rank_entry_t
 *	per private rank table entry info
 *	only used for logical NIs
 */
typedef struct rank_entry {
	ptl_rank_t		rank;		/* world rank */
	ptl_rank_t		main_rank;	/* main rank on NID */
	ptl_nid_t		nid;
	ptl_pid_t		pid;
#ifdef USE_XRC
	uint32_t		remote_xrc_srq_num;
#endif
	conn_t			connect;
} entry_t;

/* Used by SHMEM to communicate the PIDs between the local ranks for a
 * physical NI. */
struct shmem_pid_table {
	ptl_process_t id;

	/* Set to 1 when id is valid. */
	int valid;
};

/*
 * ni_t
 *	per NI info
 */
typedef struct ni {
	obj_t			obj;

	gbl_t			*gbl;

	ptl_ni_limits_t		limits;		/* max number of xxx */
	ptl_ni_limits_t		current;	/* current num of xxx */

	int			ref_cnt;

	struct iface		*iface;
	unsigned int		iface_id;
	unsigned int		options;
	unsigned int		ni_type;

	ptl_sr_value_t		status[PTL_SR_LAST];

	ptl_size_t		num_recv_pkts;
	ptl_size_t		num_recv_bytes;
	ptl_size_t		num_recv_errs;
	ptl_size_t		num_recv_drops;

	/* Serialize atomic operations on this NI. */
	pthread_mutex_t		atomic_mutex;

	pt_t			*pt;
	pthread_mutex_t		pt_mutex;
	ptl_pt_index_t		last_pt;

	struct list_head	md_list;
	pthread_spinlock_t	md_list_lock;

	struct list_head	ct_list;
	pthread_spinlock_t	ct_list_lock;

	RB_HEAD(the_root, mr) mr_tree;
	pthread_spinlock_t	mr_tree_lock;

	pthread_mutex_t		eq_wait_mutex;
	pthread_cond_t		eq_wait_cond;
	int			eq_waiters;

	pthread_mutex_t		ct_wait_mutex;
	pthread_cond_t		ct_wait_cond;
	int			ct_waiters;

	/* NI identifications */
	ptl_process_t		id;
	ptl_uid_t		uid;

	/* RDMA transport specific */
	struct {
		struct ibv_cq		*cq;
		struct ibv_comp_channel	*ch;
		ev_io			async_watcher;
		pthread_t catcher;
		int has_catcher;
		int catcher_stop;
		
		struct ibv_srq		*srq;	/* either regular or XRC */

		/* Pending send and receive operations. */
		struct list_head	recv_list;
		pthread_spinlock_t	recv_list_lock;

		int			num_posted_recv;

		struct rdma_cm_id *self_cm_id;
	} rdma;

	/* SHMEM transport specific */
	struct {
		uint8_t *comm_pad;
		size_t comm_pad_size;
		size_t per_proc_comm_buf_size;
		int per_proc_comm_buf_numbers;
		size_t num_siblings;		/* number of ranks on the node */
		ptl_rank_t local_rank;		/* local rank on this node [0..num_siblings[ */
		int knem_fd;
		struct NEMESIS_blocking_queue *receiveQ;
		pthread_t catcher;
		int has_catcher;
		char *comm_pad_shm_name;
	} shmem;

	/* object allocation pools */
	pool_t			mr_pool;
	pool_t			md_pool;
	pool_t			me_pool;
	pool_t			le_pool;
	pool_t			eq_pool;
	pool_t			ct_pool;
	pool_t			xi_pool;
	pool_t			xt_pool;
	pool_t			buf_pool;
	pool_t			sbuf_pool;

	/* Connection mappings. */
	union {
		struct {
			/* Logical NI. */

			/* On a NID, the process creating the domain is going to
			 * be the one with the lowest PID. Connection attempts to
			 * the other PIDs will be rejected. Also, locally, the
			 * XI/XT will not be queued on the non-main ranks, but on
			 * the main rank. */
#ifdef USE_XRC
			int			is_main;
#endif
			int			main_rank;

			/* Rank table. This is used to connection TO remote ranks */
			int			map_size;
			struct rank_entry	*rank_table;
			ptl_process_t	*mapping;

			/* Connection list. This is a set of passive connections,
			 * used for connections FROM remote ranks. */
			pthread_mutex_t		lock;
			struct list_head	connect_list;

#ifdef USE_XRC
			/* IB XRC support. */
			int			xrc_domain_fd;
			struct ibv_xrc_domain	*xrc_domain;
			uint32_t		xrc_rcv_qpn;
#endif

		} logical;

		struct {
			/* Physical NI. */
			void			*tree;
			pthread_spinlock_t	lock;
		} physical;
	};
} ni_t;

static inline int ni_alloc(pool_t *pool, ni_t **ni_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(pool, &obj);
	if (err) {
		*ni_p = NULL;
		return err;
	}

	*ni_p = container_of(obj, ni_t, obj);
	return PTL_OK;
}

static inline void ni_get(ni_t *ni)
{
	obj_get(&ni->obj);
}

static inline int ni_put(ni_t *ni)
{
	return obj_put(&ni->obj);
}

static inline int to_ni(ptl_handle_ni_t handle, ni_t **ni_p)
{
	int err;
	obj_t *obj;
	ni_t *ni;

	err = to_obj(POOL_NI, (ptl_handle_any_t)handle, &obj);
	if (err)
		goto err;

	ni = container_of(obj, ni_t, obj);

	/* this is because we can call PtlNIFini
	   and still get the object if someone
	   is holding a reference */
	if (ni && ni->ref_cnt <= 0) {
		ni_put(ni);
		err = PTL_ARG_INVALID;
		goto err;
	}

	*ni_p = ni;
	return PTL_OK;

err:
	*ni_p = NULL;
	return err;
}

static inline ptl_handle_ni_t ni_to_handle(const ni_t *ni)
{
	return (ptl_handle_ni_t)ni->obj.obj_handle;
}

static inline ni_t *obj_to_ni(const void *obj)
{
	return ((obj_t *)obj)->obj_ni;
}

static inline void ni_inc_status(ni_t *ni, ptl_sr_index_t index)
{
	if (index < PTL_STATUS_LAST) {
		pthread_spin_lock(&ni->obj.obj_lock);
		ni->status[index]++;
		pthread_spin_unlock(&ni->obj.obj_lock);
	}
}

int init_connect(ni_t *ni, conn_t *connect);

/* convert ni option flags to a 2 bit type */
static inline int ni_options_to_type(unsigned int options)
{
	return (((options & PTL_NI_MATCHING) ? 1 : 0) << 1) |
		((options & PTL_NI_LOGICAL) ? 1 : 0);
}

#endif /* PTL_NI_H */
