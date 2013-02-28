/*
 * ptl_p4ppe.h
 */

#ifndef PTL_P4PPE_H
#define PTL_P4PPE_H

#if IS_PPE

/* Represents a client connected to the PPE. */
struct client
{
	RB_ENTRY(client) entry;

#ifndef HAVE_KITTEN
	/* Unix socket to the PPE. */
	int s;

	/* Watcher for connection to client. */
	ev_io watcher;
#endif

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
	struct ni **ni;
	uint32_t hash;
	int members;				/* number of rank in the set */
};

struct prog_thread {
	pthread_t thread;

	/* When to stop the progress thread. */
	int stop;

	/* Linked list of active NIs. */
	struct list_head ni_list;

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

#ifndef HAVE_KITTEN
	/* Watcher for incoming connection from clients. */
	ev_io client_watcher;
	int client_fd;
#endif

	/* List of existing clients. May replace with a tree someday. */
	RB_HEAD(clients_root, client) clients_tree;

	/* For the PPE use, mainly to allocate object. */
	gbl_t gbl;	
};

#endif	/* IS_PPE */

int ppe_run(int num_bufs, int num_threads);

#ifdef HAVE_KITTEN
int ppe_add_kitten_client(int pid, void *addr, void *ppe_addr, size_t str_size, char *str);
#endif

#endif /* PTL_P4PPE_H */
