/**
 * @file ptl_conn.h
 *
 * This file contains declarations for ptl_conn.c
 * (See ptl_conn.c for detailed comments.)
 */

#ifndef PTL_CONN_H
#define PTL_CONN_H

/* forward declarations */
struct ni;
struct buf;

enum conn_state {
	CONN_STATE_DISCONNECTED,
	CONN_STATE_RESOLVING_ADDR,
	CONN_STATE_RESOLVING_ROUTE,
	CONN_STATE_CONNECTING,
	CONN_STATE_CONNECTED,
#ifdef USE_XRC
	CONN_STATE_XRC_CONNECTED,
#endif
};

enum transport_type {
	CONN_TYPE_NONE,
	CONN_TYPE_RDMA,
	CONN_TYPE_SHMEM,
};

/**
 * Per transport methods.
 */
struct transport {
	enum transport_type	type;

	int			(*post_tgt_dma)(struct buf *buf);
	int			(*send_message)(struct buf *buf, int signaled);
};

extern struct transport transport_rdma;
extern struct transport transport_shmem;

/**
 * Per connection information.
 */
struct conn {
	ptl_process_t		id;		/* dest nid/pid keep first */
	pthread_mutex_t		mutex;
	struct ni		*ni;
	int			state;
	struct sockaddr_in	sin;
	struct list_head	buf_list;
	pthread_spinlock_t	wait_list_lock;

	struct transport	transport;

	union {
		struct {
			struct rdma_cm_id	*cm_id;
			int			retry_resolve_addr;
			int			retry_resolve_route;
			int			retry_connect;
		} rdma;

		struct {
			ptl_rank_t	local_rank;	/* local rank on that node. */
		} shmem;
	};

	/* logical NI only */
	struct list_head	list;
#ifdef USE_XRC
	struct conn		*main_connect;
#endif
};

typedef struct conn conn_t;

/* RDMA CM private data */
struct cm_priv_request {
	uint32_t		options;	  /* NI options (physical/logical, ...) */
	// TODO: make network safe
	ptl_process_t		src_id;		/* rank or NID/PID requesting that connection */
};

enum {
	REJECT_REASON_NO_NI	= 1, /* NI options don't match */
	REJECT_REASON_GOOD_SRQ,		/* no main process, SRQ # is good */
	REJECT_REASON_BAD_PARAM,	/* request parm is invalid */
	REJECT_REASON_CONNECTED,	/* already connected */
	REJECT_REASON_CONNECTING,	/* already connected */
	REJECT_REASON_ERROR, /* something unexpected happened; catch all */
};

struct cm_priv_reject {
	uint32_t		reason;
#ifdef USE_XRC
	uint32_t		xrc_srq_num;
#endif
};

struct cm_priv_accept {
#ifdef USE_XRC
	uint32_t		xrc_srq_num;
#endif
};

conn_t *get_conn(struct ni *ni, ptl_process_t id);

void destroy_conns(struct ni *ni);

void conn_init(conn_t *conn, struct ni *ni);

void conn_fini(conn_t *conn);

int init_connect(struct ni *ni, conn_t *conn);

void process_cm_event(EV_P_ ev_io *w, int revents);

#endif /* PTL_CONN_H */
