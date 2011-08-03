/*
 * ptl_conn.h - connection management
 */

#ifndef PTL_CONN_H
#define PTL_CONN_H

struct ni;

/* connection state */
enum {
	CONN_STATE_DISCONNECTED,
	CONN_STATE_RESOLVING_ADDR,
	CONN_STATE_RESOLVING_ROUTE,
	CONN_STATE_CONNECTING,
	CONN_STATE_CONNECTED,
#ifdef USE_XRC
	CONN_STATE_XRC_CONNECTED,
#endif
};

/* Transport type. */
enum transport_type {
	CONN_TYPE_NONE,
	CONN_TYPE_RDMA,
};

/*
 * conn_t
 *	 per connection info
 */
typedef struct conn {
	ptl_process_t		id;		/* dest nid/pid keep first */
	pthread_mutex_t		mutex;
	struct ni		*ni;
	int			state;
	struct sockaddr_in	sin;
	struct list_head	xi_list;
	struct list_head	xt_list;
	pthread_spinlock_t	wait_list_lock;

	enum transport_type transport_type;

	union {
		struct {
			struct rdma_cm_id	*cm_id;
			int			retry_resolve_addr;
			int			retry_resolve_route;
			int			retry_connect;
		} rdma;
	};

	/* logical NI only */
	struct list_head	list;
#ifdef USE_XRC
	struct conn		*main_connect;
#endif
} conn_t;

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

/*
 * get_conn
 *	lookup or create new conn_t
 *	from ni to id
 */
conn_t *get_conn(struct ni *ni, const ptl_process_t *id);

/*
 * conn_init
 *	initialize conn_t
 */
void conn_init(struct ni *ni, conn_t *conn);

/*
 * init_connect
 *	request a connection from rdmacm
 */
int init_connect(struct ni *ni, conn_t *conn);

void cleanup_iface(iface_t *iface);

int init_iface(iface_t *iface);

int iface_init(gbl_t *gbl);

void iface_fini(gbl_t *gbl);

iface_t *get_iface(gbl_t *gbl, ptl_interface_t iface_id);

struct ni *iface_get_ni(iface_t *iface, int ni_type);

int iface_add_ni(iface_t *iface, struct ni *ni);

int iface_remove_ni(struct ni *ni);

#endif /* PTL_CONN_H */
