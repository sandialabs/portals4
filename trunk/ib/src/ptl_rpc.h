/*
 * ptl_rpc.h
 */

#ifndef PTL_RPC_H
#define PTL_RPC_H

/* TCP port for control connections. */
#define PTL_CTL_PORT		(3456)

/* Port on which RDMA listen wait for incoming XRC connection. */
#define PTL_XRC_PORT (7694)

/*
 * messages 
 */
struct rpc_msg {
	enum {
		INVALID,
		QUERY_XRC_DOMAIN = 1,
		QUERY_RANK_TABLE = 2,

		/* 8th bit indicate a reply. */
		REPLY_RANK_TABLE = 128,
		REPLY_XRC_DOMAIN = 129,
	} type;

	uint32_t sequence;

	union {
		/* QUERY_RANK_TABLE */
		struct {
			ptl_rank_t local_rank; /* message from local rank */
			ptl_rank_t rank;	   /* (world) rank */
			uint32_t xrc_srq_num;
			in_addr_t addr;		/* IPV4 address, in network order */
		} query_rank_table;

		/* REPLY_RANK_TABLE */
		struct {
			/* Shared memory */
			char shmem_filename[1024];
			size_t shmem_filesize;
		} reply_rank_table;

		/* QUERY_XRC_DOMAIN */
		struct {
			char net_name[50];		/* network interface name, eg. ib0 */
		} query_xrc_domain;

		/* REPLY_XRC_DOMAIN */
		struct {
			/* file name for the XRC domain. */
			char xrc_domain_fname[1024];
		} reply_xrc_domain;

		/* LOCAL RANK TABLE */
		struct {
			ptl_nid_t nid;
			struct {
				ptl_rank_t rank;
				uint32_t xrc_srq_num;
			} ranks[64];
		} local_rank_table;
	};
};

/*
 * type of end point
 */
enum rpc_type {
	rpc_type_server,
	rpc_type_client,
};

/*
 * per rpc session info
 */
struct rpc;
struct session {
	struct list_head		session_list;
	int				fd;			/* connected socket */
	uint32_t			sequence;
	pthread_mutex_t			mutex;
	pthread_cond_t			cond;
	ev_io					watcher;
	struct rpc				*rpc; /* backpointer to rpc owner */
	struct rpc_msg			rpc_msg;
};

/*
 * per rpc end point info
 */
struct rpc {
	enum rpc_type			type;
	int				fd;			/* listening socket */

	ev_io watcher;

	/* List of all active sessions. */
	struct list_head		session_list;
	struct session			*to_server; /* for client only */
	pthread_spinlock_t		session_list_lock;

	void (*callback)(struct session *session, void *data);
	void *callback_data;
};

int rpc_init(enum rpc_type type, ptl_nid_t nid, unsigned int ctl_port,
			 struct rpc **rpc_p,
			 void (*callback)(struct session *session, void *callback_data),
			 void *callback_data);
int rpc_fini(struct rpc *rpc);
int rpc_get(struct session *session,
			struct rpc_msg *msg_in, struct rpc_msg *msg_out);
int rpc_send(struct session *session, struct rpc_msg *msg_out);

#endif /* RPC_PTL_H */
