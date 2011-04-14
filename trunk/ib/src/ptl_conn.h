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
	CONN_STATE_XRC_CONNECTED,
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
	struct rdma_cm_id	*cm_id;
	struct sockaddr_in	sin;
	int			retry_resolve_addr;
	int			retry_resolve_route;
	int			retry_connect;
	struct list_head	xi_list;
	struct list_head	xt_list;
	pthread_spinlock_t	wait_list_lock;

	/* logical NI only */
	struct list_head	list;
	struct conn		*main_connect;
} conn_t;

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

void process_cm_event(EV_P_ ev_io *w, int revents);

#endif /* PTL_CONN_H */
