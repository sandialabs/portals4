/*
 * ptl_gbl.h
 */

#ifndef PTL_GBL_H
#define PTL_GBL_H

typedef struct gbl {
	int			num_iface;	/* size of interface table */
	iface_t			*iface;		/* interface table */
	int			ref_cnt;	/* count PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	int finalized;

	pthread_mutex_t		gbl_mutex;
	pool_t			ni_pool;

	/* PPE specific. */

	/* Mapping of the whole process. */
	xpmem_apid_t apid;

	/* Number of the progress thread assigned to this client. */
	unsigned int prog_thread;
} gbl_t;

static inline int gbl_get(void)
{
	return PTL_OK;
}

static inline void gbl_put(void)
{
}

extern void gbl_release(ref_t *ref);

/* Represents a client connected to the PPE. */
struct client
{
	RB_ENTRY(client) entry;

	/* Unix socket to the PPE. */
	int s;

	/* Watcher for connection to client. */
	ev_io watcher;

	/* Keep the PID of the client. It's the key to the list. */
	pid_t pid;

	gbl_t gbl;
};

extern struct transport transport_mem;

/* A set of logical NIs. These NIs come from the same application;
   ie. all applications using that PPE will have a different set of
   logical NIs (if any). */
struct logical_ni_set {
	RB_ENTRY(logical_ni_set) entry;
	ni_t **ni;
	uint32_t hash;
	int members;				/* number of rank in the set */
};

struct prog_thread {
	pthread_t thread;

	/* When to stop the progress thread. */
	int stop;

	/* Points to own queue in comm pad. */
	queue_t *queue;
	
	/* Internal queue. Used for communication regarding transfer
	 * between clients. */
	queue_t internal_queue;
};

struct ppe {
	/* Communication pad. */
	struct ppe_comm_pad *comm_pad;
	struct xpmem_map comm_pad_mapping;

	/* The progress threads. */
	int num_prog_threads;		/* in prog_thread[] */
	int current_prog_thread;
	struct prog_thread prog_thread[MAX_PROGRESS_THREADS];

	/* The event loop thread. */
	pthread_t		event_thread;
	int			event_thread_run;

	/* Pool/list of ppebufs, used by client to talk to PPE. */
	struct {
		void *slab;
		int num;				/* total number of ppebufs */
	} ppebuf;

	/* Tree for physical NIs, indexed on PID. */
	RB_HEAD(phys_ni_root, ni) physni_tree;

	/* Tree for sets of logical NIs, indexed on hash. */
	RB_HEAD(logical_ni_set_root, logical_ni_set) set_tree;

	/* Watcher for incoming connection from clients. */
	ev_io client_watcher;
	int client_fd;

	/* Linked list of active NIs. */
	struct list_head ni_list;

	/* List of existing clients. May replace with a tree someday. */
	RB_HEAD(clients_root, client) clients_tree;
};

#endif /* PTL_GBL_H */
