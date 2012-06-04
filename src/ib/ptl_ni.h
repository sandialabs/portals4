/*
 * ptl_ni.h
 */

#ifndef PTL_NI_H
#define PTL_NI_H

#include "ptl_locks.h"

struct queue;
struct conn;

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
	struct conn			*connect;
} entry_t;

/* Used by SHMEM to communicate the PIDs between the local ranks for a
 * physical NI. */
struct shmem_pid_table {
	ptl_process_t id;

	/* Set to 1 when id is valid. */
	int valid;
};

struct shmem_bounce_head {
	union counted_ptr free_list;	 /* head of free list of bounce buffers */
	void *head_index0;	 /* logical address of the head of local index
						  * 0. Invariant. */
};

/* Memory regions tree attached to an NI. The PPE must have 2, the
 * other transports need one. */
struct ni_mr_tree {
	RB_HEAD(the_root, mr) tree;
	PTL_FASTLOCK_TYPE	tree_lock;
};

/*
 * ni_t
 *	per NI info
 */
typedef struct ni {
	obj_t			obj;

	ptl_ni_limits_t		limits;		/* max number of xxx */
	ptl_ni_limits_t		current;	/* used for accounting of objects, not real limits. */

	atomic_t			ref_cnt;

	struct iface		*iface;
	unsigned int		iface_id;
	unsigned int		options;
	unsigned int		ni_type;

	ptl_sr_value_t		status[PTL_SR_LAST];

	ptl_size_t		num_recv_pkts;
	ptl_size_t		num_recv_bytes;
	ptl_size_t		num_recv_errs;
	ptl_size_t		num_recv_drops;

	int shutting_down;

	/* Serialize atomic operations on this NI. */
	pthread_mutex_t		atomic_mutex;

	pt_t			*pt;
	pthread_mutex_t		pt_mutex;
	ptl_pt_index_t		last_pt;

	struct list_head	md_list;
	PTL_FASTLOCK_TYPE	md_list_lock;

	struct list_head	ct_list;
	PTL_FASTLOCK_TYPE	ct_list_lock;

	/* The PPE must have a tree indexed on the application addresses,
	 * and one tree for its own addresses. The other implementations
	 * don't need that distinction. */
	struct ni_mr_tree mr_self;		/* the PPE */
	struct ni_mr_tree mr_app;		/* the client */

	int umn_fd;
	ev_io umn_watcher;
	uint64_t *umn_counter;

	/* NI identifications */
	ptl_process_t		id;
	ptl_uid_t		uid;

	/* Progress thread. */
	pthread_t catcher;
	int has_catcher;
	int catcher_stop;

	/* RDMA transport specific */
	struct {
		struct ibv_cq		*cq;
		struct ibv_comp_channel	*ch;
		ev_io			async_watcher;
		
		struct ibv_srq		*srq;

		/* Pending send and receive operations. */
		struct list_head	recv_list;
		PTL_FASTLOCK_TYPE	recv_list_lock;

		atomic_t			num_posted_recv;

		struct rdma_cm_id *self_cm_id;

		/* Number of established connections. */
		atomic_t num_conn;
	} rdma;

#if WITH_TRANSPORT_SHMEM
	/* SHMEM transport specific */
	struct {
		void *comm_pad;		/* in shared memory */
		size_t comm_pad_size;
		size_t per_proc_comm_buf_size;
		int per_proc_comm_buf_numbers;
		int knem_fd;
		struct queue *queue;	/* own queue, in the comm pad */
		void *first_queue;		/* addr of rank 0 queue, in the comm pad */
		char *comm_pad_shm_name;

#if !USE_KNEM
		/* Bounce buffers used when KNEM is not available. They are
		 * created and linked by rank 0. */
		struct {
			struct shmem_bounce_head *head;
			void *bbs;			/* local address of the bounce buffers */

			size_t buf_size;
			unsigned int num_bufs;
		} bounce_buf;
#endif
	} shmem;
#endif

#if WITH_TRANSPORT_SHMEM || IS_PPE
	struct {
		int node_size;		/* number of ranks on the node */
		int index;	   /* local index on this node [0..node_size[ */
		uint32_t hash;

#ifdef IS_PPE
		int in_group;
#endif
	} mem;
#endif

#if WITH_TRANSPORT_SHMEM && !USE_KNEM
	PTL_FASTLOCK_TYPE noknem_lock;
	struct list_head noknem_list;
#endif

	/* object allocation pools */
	pool_t mr_pool;
	pool_t md_pool;
	pool_t me_pool;
	pool_t le_pool;
	pool_t eq_pool;
	pool_t ct_pool;
	pool_t xt_pool;
	pool_t buf_pool;
	pool_t sbuf_pool;
	pool_t conn_pool;

	/* Connection mappings. */
	union {
		struct {
			/* Logical NI. */

			/* On a NID, the process creating the domain is going to
			 * be the one with the lowest PID. Connection attempts to
			 * the other PIDs will be rejected. Also, locally, the
			 * XI/XT will not be queued on the non-main ranks, but on
			 * the main rank. */

			/* Rank table. This is used to connection TO remote ranks */
			int			map_size;
			struct rank_entry	*rank_table;
			ptl_process_t	*mapping;
		} logical;

		struct {
			/* Physical NI. */
			void			*tree;
			PTL_FASTLOCK_TYPE	lock;
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
	obj_t *obj;
	ni_t *ni;

	obj = to_obj(POOL_NI, (ptl_handle_any_t)handle);
	if (!obj)
		goto err;

	ni = container_of(obj, ni_t, obj);

#ifndef NO_ARG_VALIDATION
	/* this is because we can call PtlNIFini
	   and still get the object if someone
	   is holding a reference */
	if (atomic_read(&ni->ref_cnt) <= 0) {
		ni_put(ni);
		goto err;
	}
#endif

	*ni_p = ni;
	return PTL_OK;

err:
	*ni_p = NULL;
	return PTL_ARG_INVALID;
}

static inline ptl_handle_ni_t ni_to_handle(const ni_t *ni)
{
	return (ptl_handle_ni_t)ni->obj.obj_handle;
}

static inline ni_t *obj_to_ni(const void *obj)
{
	return ((obj_t *)obj)->obj_ni;
}

int init_connect(ni_t *ni, struct conn *connect);

/* convert ni option flags to a 2 bit type */
static inline int ni_options_to_type(unsigned int options)
{
	return (((options & PTL_NI_MATCHING) ? 1 : 0) << 1) |
		((options & PTL_NI_LOGICAL) ? 1 : 0);
}

#endif /* PTL_NI_H */
