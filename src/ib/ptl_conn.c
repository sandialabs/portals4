/**
 * @file ptl_conn.c
 *
 * This file contains routines for handling connection setup.
 *
 * Each NI has a conn_t struct for each peer NI that it
 * communicates with, whether on the local or a remote node.
 * There must be a connected set of rdma QPs one on the local NI
 * and one on each remote NI in order to use rdma transport.
 * For local peer NIs on the same node a shared memory transport
 * is used that does not require a connection but the library
 * still has a conn_t for each local peer NI.
 *
 * The code supports two options for connection setup: RC and XRC.
 * XRC has better scaling behavior in very large systems and uses
 * a single receive QP per node (NID) and a shared receive queue (SRQ)
 * per process (PID). There is a separate send QP per process for
 * each remote node. For RC there is a send and receive QP for each
 * remote process (NID/PID).
 */

#include "ptl_loc.h"

#define max(a,b)	(((a) > (b)) ? (a) : (b))

/**
 * Initialize a new conn_t struct.
 *
 * @param[in] conn the conn_t to init
 * @param[in] ni the ni that owns it
 */
int conn_init(void *arg, void *parm)
{
	conn_t *conn = arg;

	OBJ_NEW(conn);

	pthread_mutex_init(&conn->mutex, NULL);
	pthread_spin_init(&conn->wait_list_lock, PTHREAD_PROCESS_PRIVATE);

	conn->state = CONN_STATE_DISCONNECTED;
	conn->transport = transport_rdma;
	conn->rdma.cm_id = NULL;

	INIT_LIST_HEAD(&conn->buf_list);
#ifdef USE_XRC
	INIT_LIST_HEAD(&conn->list);
#endif

	atomic_set(&conn->rdma.send_comp_threshold, 0);
	atomic_set(&conn->rdma.rdma_comp_threshold, 0);

	atomic_set(&conn->rdma.num_req_posted, 0);
	atomic_set(&conn->rdma.num_req_not_comp, 0);

	conn->rdma.max_req_avail = 0;

	return PTL_OK;
}

/**
 * Cleanup a conn_t struct.
 *
 * @param[in] conn
 */
void conn_fini(void *arg)
{
	conn_t *conn = arg;

	if (conn->transport.type == CONN_TYPE_RDMA) {
		if (conn->rdma.cm_id) {
			if (conn->rdma.cm_id->qp)
				rdma_destroy_qp(conn->rdma.cm_id);
			rdma_destroy_id(conn->rdma.cm_id);
			conn->rdma.cm_id = NULL;
		}
	}

	pthread_mutex_destroy(&conn->mutex);
	pthread_spin_destroy(&conn->wait_list_lock);
}

/**
 * Numerically compare two physical IDs.
 *
 * Compare NIDs and then compare PIDs if NIDs are the same.
 * Used to sort IDs in a binary tree. Can be used for a
 * portals physical ID or for a conn_t which contains an ID
 * as its first member.
 *
 * @param[in] a first ID
 * @param[in] b second ID
 *
 * @return > 0 if a > b
 * @return 0 if a = b
 * @return < 0 if a < b
 */
static int compare_id(const ptl_process_t *id1, const ptl_process_t *id2)
{
	return (id1->phys.nid != id2->phys.nid) ?
			(id1->phys.nid - id2->phys.nid) :
			(id1->phys.pid - id2->phys.pid);
}

static int compare_conn_id(const void *a, const void *b)
{
	const conn_t *c1 = a;
	const conn_t *c2 = b;

	return compare_id(&c1->id, &c2->id);
}

/**
 * Get connection info for a given process id.
 *
 * For logical NIs the connection is contained in the rank table.
 * For physical NIs the connection is held in a binary tree using
 * the ID as a sorting value.
 *
 * For physical NIs if this is the first time we are sending a message
 * to this process create a new conn_t. For logical NIs the conn_t
 * structs are all allocated when the rank table is loaded.
 *
 * @param[in] ni the NI from which to get the connection
 * @param[in] id the process ID to lookup
 *
 * @return the conn_t and takes a reference on it
 */
conn_t *get_conn(ni_t *ni, ptl_process_t id)
{
	conn_t *conn;
	void **ret;

	if (ni->shutting_down) {
		WARN();
		return NULL;
	}

	if (ni->options & PTL_NI_LOGICAL) {
		if (unlikely(id.rank >= ni->logical.map_size)) {
			ptl_warn("Invalid rank (%d >= %d)\n",
				 id.rank, ni->logical.map_size);
			return NULL;
		}

		conn = ni->logical.rank_table[id.rank].connect;
		conn_get(conn);
	} else {
		conn_t conn_search;

		pthread_spin_lock(&ni->physical.lock);

		/* lookup in binary tree */
		conn_search.id = id;
		ret = tfind(&conn_search, &ni->physical.tree, compare_conn_id);
		if (ret) {
			conn = *ret;
			conn_get(conn);
		} else {
			/* Not found. Allocate and insert. */
			if (conn_alloc(ni, &conn)) {
				pthread_spin_unlock(&ni->physical.lock);
				WARN();
				return NULL;
			}

			conn->id = id;

			/* Get the IP address from the NID. */
			conn->sin.sin_family = AF_INET;
			conn->sin.sin_addr.s_addr = nid_to_addr(id.phys.nid);
			conn->sin.sin_port = pid_to_port(id.phys.pid);

			/* insert new conn into binary tree */
			ret = tsearch(conn, &ni->physical.tree, compare_conn_id);
			if (!ret) {
				WARN();
				conn_put(conn);
				conn = NULL;
			} else {
				conn_get(conn);
			}
		}

		pthread_spin_unlock(&ni->physical.lock);
	}

	return conn;
}

static void destroy_conn(void *data)
{
	conn_t *conn = data;

	pthread_mutex_lock(&conn->mutex);

	if (conn->transport.type == CONN_TYPE_RDMA) {
		switch(conn->state) {
		case CONN_STATE_CONNECTING:
		case CONN_STATE_CONNECTED:
		case CONN_STATE_RESOLVING_ROUTE:
			conn->state = CONN_STATE_DISCONNECTING;
			if (conn->rdma.cm_id)
				rdma_disconnect(conn->rdma.cm_id);
			break;

		case CONN_STATE_RESOLVING_ADDR:
			conn->state = CONN_STATE_DISCONNECTING;
			break;

		case CONN_STATE_DISCONNECTING:
			abort();			/* not possible */
			break;

 		case CONN_STATE_DISCONNECTED:
			break;
		}
	}

	pthread_mutex_unlock(&conn->mutex);
}

/**
 * Destroys all connections belonging to an NI
 */
void destroy_conns(ni_t *ni)
{
	if (ni->options & PTL_NI_LOGICAL) {
		int i;
		const int map_size = ni->logical.map_size;

		/* Destroy active connections. */
		for (i = 0; i < map_size; i++) {
			conn_t *conn = ni->logical.rank_table[i].connect;
			destroy_conn(conn);
		}

#if 0
		/* We could wait for all connections to coalesce, however this
		 * leads to some minute-long timeout in some tests. */
		for (i = 0; i < map_size; i++) {
			conn_t *conn = ni->logical.rank_table[i].connect;
			while (conn->state != CONN_STATE_DISCONNECTED) {
				usleep(1000);
			}
		}
#endif
#ifdef USE_XRC
		/* Destroy passive connections. */
		while(!list_empty(&ni->logical.connect_list)) {
			conn_t *conn = list_first_entry(&ni->logical.connect_list, conn_t, list);

			list_del(&conn->list);
			destroy_conn(conn);
		}
#endif
	} else {
		tdestroy(ni->physical.tree, destroy_conn);
	}
}

/**
 * @param[in] ni
 * @param[in] conn
 *
 * @return status
 *
 * conn must be locked
 */
int init_connect(ni_t *ni, conn_t *conn)
{
	struct rdma_cm_id *cm_id;

	assert(conn->rdma.cm_id == NULL);

	ptl_info("Initiate connect with %x:%d\n",
			 conn->sin.sin_addr.s_addr, conn->sin.sin_port);

	if (ni->shutting_down)
		return PTL_FAIL;

	conn_get(conn);

	assert(conn->state == CONN_STATE_DISCONNECTED);

	conn->rdma.retry_resolve_addr = 3;
	conn->rdma.retry_resolve_route = 3;
	conn->rdma.retry_connect = 3;

	if (rdma_create_id(ni->iface->cm_channel, &cm_id,
			   conn, RDMA_PS_TCP)) {
		WARN();
		conn_put(conn);
		return PTL_FAIL;
	}

	conn->state = CONN_STATE_RESOLVING_ADDR;
	conn->rdma.cm_id = cm_id;

	if (rdma_resolve_addr(cm_id, NULL,
			      (struct sockaddr *)&conn->sin, get_param(PTL_RDMA_TIMEOUT))) {
		ptl_warn("rdma_resolve_addr failed %x:%d\n",
				 conn->sin.sin_addr.s_addr, conn->sin.sin_port);
		conn->state = CONN_STATE_DISCONNECTED;
		conn->rdma.cm_id = NULL;
		rdma_destroy_id(cm_id);
		conn_put(conn);
		return PTL_FAIL;
	}

	ptl_info("Connection initiated successfully to %x:%d\n",
			 conn->sin.sin_addr.s_addr, conn->sin.sin_port);

	return PTL_OK;
}

/**
 * Retrieve some current parameters from the QP. Right now we only
 * need max_inline_data.
 *
 * @param[in] conn
 */
static void get_qp_param(conn_t *conn)
{
	int rc;
	struct ibv_qp_attr attr;
	struct ibv_qp_init_attr init_attr;

	rc = ibv_query_qp(conn->rdma.cm_id->qp, &attr, IBV_QP_CAP, &init_attr);
	assert(rc == 0);

	if (rc == 0) {
		conn->rdma.max_inline_data = init_attr.cap.max_inline_data;

		/* Limit the send buffer operations from the initiator to 1/4th
		 * of the work requests. */
		conn->rdma.max_req_avail = init_attr.cap.max_send_wr / 4;
	}
}

/**
 * @param[in] ni
 * @param[in] conn
 * @param[in] event
 *
 * @return status
 *
 * conn is locked
 */
static int accept_connection_request(ni_t *ni, conn_t *conn,
				     struct rdma_cm_event *event)
{
	struct rdma_conn_param conn_param;
	struct ibv_qp_init_attr init_attr;
	struct cm_priv_accept priv;

	conn->state = CONN_STATE_CONNECTING;

	memset(&init_attr, 0, sizeof(init_attr));

#ifdef USE_XRC
	if (ni->options & PTL_NI_LOGICAL) {
		init_attr.qp_type = IBV_QPT_XRC;
		init_attr.xrc_domain = ni->logical.xrc_domain;
		init_attr.cap.max_send_wr = 0;
	} else
#endif
	{
		init_attr.qp_type = IBV_QPT_RC;
		init_attr.cap.max_send_wr = ni->iface->cap.max_send_wr;
	}
	init_attr.send_cq = ni->rdma.cq;
	init_attr.recv_cq = ni->rdma.cq;
	init_attr.srq = ni->rdma.srq;
	init_attr.cap.max_send_sge = ni->iface->cap.max_send_sge;

	if (rdma_create_qp(event->id, ni->iface->pd, &init_attr)) {
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	/* If we were already trying to connect ourselves, cancel it. */
	if (conn->rdma.cm_id != NULL) {
		assert(conn->rdma.cm_id->context == conn);
		conn->rdma.cm_id->context = NULL;
	}

	event->id->context = conn;
	conn->rdma.cm_id = event->id;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;
	conn_param.retry_count		= 7;
	conn_param.rnr_retry_count		= 7;

	if (ni->options & PTL_NI_LOGICAL) {
		conn_param.private_data = &priv;
		conn_param.private_data_len = sizeof(priv);

#ifdef USE_XRC
		priv.xrc_srq_num = ni->rdma.srq->xrc_srq_num;
#endif
	}

	if (rdma_accept(event->id, &conn_param)) {
		rdma_destroy_qp(event->id);
		conn->rdma.cm_id = NULL;
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	return PTL_OK;
}

#ifdef USE_XRC
/**
 * Accept a connection request from/to a logical NI.
 *
 * @param[in] ni
 * @param[in] event
 *
 * @return status
 */
static int accept_connection_request_logical(ni_t *ni,
					     struct rdma_cm_event *event)
{
	int ret;
	conn_t *conn;

	assert(ni->options & PTL_NI_LOGICAL);

	/* Accept the connection and give back our SRQ
	 * number. This will be a passive connection (ie, nothing
	 * will be sent from that side. */
	if (conn_alloc(ni, &conn)) {
		WARN();
		return PTL_NO_SPACE;
	}

	pthread_mutex_lock(&ni->logical.lock);
	list_add_tail(&conn->list, &ni->logical.connect_list);
	pthread_mutex_unlock(&ni->logical.lock);

	pthread_mutex_lock(&conn->mutex);
	ret = accept_connection_request(ni, conn, event);
	if (ret) {
		WARN();
		pthread_mutex_lock(&ni->logical.lock);
		list_del_init(&conn->list);
		pthread_mutex_unlock(&ni->logical.lock);
		pthread_mutex_unlock(&conn->mutex);

		conn_put(conn);
	} else {
		pthread_mutex_unlock(&conn->mutex);
	}

	return ret;
}
#endif

/**
 * Accept an RC connection request to self.
 *
 * called while holding connect->mutex
 * only used for physical NIs
 *
 * @param[in] ni
 * @param[in] conn
 * @param[in] event
 *
 * @return status
 */
static int accept_connection_self(ni_t *ni, conn_t *conn,
				  struct rdma_cm_event *event)
{
	struct rdma_conn_param conn_param;
	struct ibv_qp_init_attr init_attr;

	conn->state = CONN_STATE_CONNECTING;

	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.qp_type = IBV_QPT_RC;
	init_attr.send_cq = ni->rdma.cq;
	init_attr.recv_cq = ni->rdma.cq;
	init_attr.srq = ni->rdma.srq;
	init_attr.cap.max_send_wr = ni->iface->cap.max_send_wr;
	init_attr.cap.max_send_sge = ni->iface->cap.max_send_sge;

	if (rdma_create_qp(event->id, ni->iface->pd, &init_attr)) {
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	ni->rdma.self_cm_id = event->id;

	event->id->context = conn;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;
	conn_param.rnr_retry_count = 7;

	if (rdma_accept(event->id, &conn_param)) {
		rdma_destroy_qp(event->id);
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	return PTL_OK;
}

/**
 * @param[in] conn
 */
static void flush_pending_xi_xt(conn_t *conn)
{
	buf_t *buf;

	pthread_spin_lock(&conn->wait_list_lock);
	while(!list_empty(&conn->buf_list)) {
		buf = list_first_entry(&conn->buf_list, buf_t, list);
		list_del_init(&buf->list);
		pthread_spin_unlock(&conn->wait_list_lock);

		if (buf->type == BUF_TGT)
			process_tgt(buf);
		else {
			assert(buf->type == BUF_INIT);
			process_init(buf);
		}
			
		pthread_spin_lock(&conn->wait_list_lock);
	}


	pthread_spin_unlock(&conn->wait_list_lock);
}

/**
 * Process RC connection request event.
 *
 * @param[in] iface
 * @param[in] event
 *
 * @return status
 */
static int process_connect_request(struct iface *iface, struct rdma_cm_event *event)
{
	const struct cm_priv_request *priv;
	struct cm_priv_reject rej;
	conn_t *conn;
	int ret = 0;
	int c;
	ni_t *ni;

	if (!event->param.conn.private_data ||
		(event->param.conn.private_data_len < sizeof(struct cm_priv_request))) {
		rej.reason = REJECT_REASON_BAD_PARAM;

		goto reject;
	}

	priv = event->param.conn.private_data;
	ni = iface->ni[ni_options_to_type(priv->options)];

	if (!ni) {
		rej.reason = REJECT_REASON_NO_NI;
		goto reject;
	}

#ifdef USE_XRC
	if (ni->options & PTL_NI_LOGICAL) {
		if (ni->logical.is_main) {
			ret = accept_connection_request_logical(ni, event);
			if (!ret) {
				/* Good. */
				return ret;
			}
			
			WARN();
			rej.reason = REJECT_REASON_ERROR;
			rej.xrc_srq_num = ni->rdma.srq->xrc_srq_num;
		}
		else {
			/* If this is not the main process on this node, reject
			 * the connection but give out SRQ number. */	
			rej.reason = REJECT_REASON_GOOD_SRQ;
			rej.xrc_srq_num = ni->rdma.srq->xrc_srq_num;
		}

		goto reject;
	}
#endif

	conn = get_conn(ni, priv->src_id);
	if (!conn) {
		WARN();
		rej.reason = REJECT_REASON_ERROR;
		goto reject;
	}

	pthread_mutex_lock(&conn->mutex);

	switch (conn->state) {
	case CONN_STATE_CONNECTED:
		/* We received a connection request but we are already connected. Reject it. */
		rej.reason = REJECT_REASON_CONNECTED;
		pthread_mutex_unlock(&conn->mutex);
		conn_put(conn);
		goto reject;
		break;

	case CONN_STATE_DISCONNECTED:
		/* we received a connection request and we are disconnected
		   - accept it
		*/
		ret = accept_connection_request(ni, conn, event);
		break;

	case CONN_STATE_DISCONNECTING:
		/* Not sure how to handle that case. Ignore and disconnect
		 * anyway? */
		abort();
		break;

	case CONN_STATE_RESOLVING_ADDR:
	case CONN_STATE_RESOLVING_ROUTE:
	case CONN_STATE_CONNECTING:
		/* we received a connection request but we are already connecting
		 * - accept connection from higher id
		 * - reject connection from lower id
		 * - accept connection from self, but cleanup
		 */
		c = compare_id(&priv->src_id, &ni->id);
		if (c > 0)
			ret = accept_connection_request(ni, conn, event);
		else if (c < 0) {
			rej.reason = REJECT_REASON_CONNECTING;
			pthread_mutex_unlock(&conn->mutex);
			conn_put(conn);
			goto reject;
		}
		else {
			ret = accept_connection_self(ni, conn, event);
		}
		break;
	}

	pthread_mutex_unlock(&conn->mutex);

	return ret;

 reject:
	rdma_reject(event->id, &rej, sizeof(rej));
	return PTL_FAIL;
}

/**
 * Process CM event.
 *
 * there is a listening rdmacm id per iface
 * this is called as a handler from libev
 *
 * @param[in] w
 * @param[in] revents
 */
void process_cm_event(EV_P_ ev_io *w, int revents)
{
	struct iface *iface = w->data;
	ni_t *ni;
	struct rdma_cm_event *event;
	conn_t *conn;
	struct rdma_conn_param conn_param;
	struct cm_priv_request priv;
	struct ibv_qp_init_attr init;
	const struct cm_priv_reject *rej;

	if (rdma_get_cm_event(iface->cm_channel, &event)) {
		WARN();
		return;
	}

	conn = event->id->context;
	if (conn) {
		ni = conn->obj.obj_ni;
		if (ni->shutting_down)
			goto done;
	} else {
		if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
			/* Ignore this event. */
			goto done;
		}
		ni = NULL;
	}

	ptl_info("Rank got CM event %d for id %p\n", event->event, event->id);

	switch(event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		pthread_mutex_lock(&conn->mutex);

		if (conn->state != CONN_STATE_RESOLVING_ADDR) {
			/* Our connect attempt got overriden by the remote
			 * side. */
			conn_put(conn);
			pthread_mutex_unlock(&conn->mutex);
			break;
		}

		assert(conn->rdma.cm_id == event->id);

		conn->state = CONN_STATE_RESOLVING_ROUTE;
		if (rdma_resolve_route(event->id, get_param(PTL_RDMA_TIMEOUT))) {
			conn->state = CONN_STATE_DISCONNECTED;
			conn->rdma.cm_id = NULL;
			conn_put(conn);
		}

		pthread_mutex_unlock(&conn->mutex);
		break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		memset(&conn_param, 0, sizeof conn_param);

		conn_param.responder_resources	= 1;
		conn_param.initiator_depth	= 1;
		conn_param.retry_count		= 7;
		conn_param.rnr_retry_count	= 7;
		conn_param.private_data		= &priv;
		conn_param.private_data_len	= sizeof(priv);

		pthread_mutex_lock(&conn->mutex);

		if (conn->state != CONN_STATE_RESOLVING_ROUTE) {
			/* Our connect attempt got overriden by the remote
			 * side. */
			conn_put(conn);
			pthread_mutex_unlock(&conn->mutex);
			break;
		}

		assert(conn->rdma.cm_id == event->id);

		/* Create the QP. */
		memset(&init, 0, sizeof(init));
		init.qp_context			= ni;
		init.send_cq			= ni->rdma.cq;
		init.recv_cq			= ni->rdma.cq;
		init.cap.max_send_wr		= ni->iface->cap.max_send_wr;
		init.cap.max_send_sge		= ni->iface->cap.max_send_sge;

#ifdef USE_XRC
		if (ni->options & PTL_NI_LOGICAL) {
			init.qp_type			= IBV_QPT_XRC;
			init.xrc_domain			= ni->logical.xrc_domain;
			priv.src_id.rank		= ni->id.rank;
		} else
#endif
		{
			init.qp_type			= IBV_QPT_RC;
			init.srq			= ni->rdma.srq;
			priv.src_id			= ni->id;
		}
		priv.options			= ni->options;

		assert(conn->rdma.cm_id == event->id);

		if (rdma_create_qp(event->id, ni->iface->pd, &init)) {
			WARN();
			conn->state = CONN_STATE_DISCONNECTED;
			conn->rdma.cm_id = NULL;
			conn_put(conn);
		}
		else if (rdma_connect(event->id, &conn_param)) {
			WARN();
			conn->state = CONN_STATE_DISCONNECTED;
			rdma_destroy_qp(conn->rdma.cm_id);
			conn->rdma.cm_id = NULL;
			conn_put(conn);
		} else {
			conn->state = CONN_STATE_CONNECTING;
		}

		pthread_mutex_unlock(&conn->mutex);

		break;

	case RDMA_CM_EVENT_ESTABLISHED: {
		pthread_mutex_lock(&conn->mutex);

		if (ni->rdma.self_cm_id == event->id) {
			/* Self connection. Let the initiator side finish the
			 * connection. */
			pthread_mutex_unlock(&conn->mutex);
			break;
		}

		if (conn->state != CONN_STATE_CONNECTING) {
			pthread_mutex_unlock(&conn->mutex);
			break;
		}

		assert(conn->rdma.cm_id == event->id);

		get_qp_param(conn);

		conn->state = CONN_STATE_CONNECTED;

#ifdef USE_XRC
		if ((ni->options & PTL_NI_LOGICAL) &&
			(event->param.conn.private_data_len)) {
			/* If we have private data, it's that side asked for the
			 * connection (as opposed to accepting an incoming
			 * request). */
			const struct cm_priv_accept *priv_accept = event->param.conn.private_data;
			struct rank_entry *entry = container_of(conn, struct rank_entry, connect);

			/* Should not be set yet. */
			assert(entry->remote_xrc_srq_num == 0);

			entry->remote_xrc_srq_num = priv_accept->xrc_srq_num;

			/* Flush the posted requests/replies. */
			while(!list_empty(&conn->list)) {
				conn_t *c = list_first_entry(&conn->list, conn_t, list);

				list_del_init(&c->list);

				pthread_mutex_unlock(&conn->mutex);

				pthread_mutex_lock(&c->mutex);
				flush_pending_xi_xt(c);
				pthread_mutex_unlock(&c->mutex);

				pthread_mutex_lock(&conn->mutex);
			}
		}
#endif
		flush_pending_xi_xt(conn);

		pthread_mutex_unlock(&conn->mutex);
	}
		break;

	case RDMA_CM_EVENT_CONNECT_REQUEST:
		process_connect_request(iface, event);
		break;

	case RDMA_CM_EVENT_REJECTED:
		pthread_mutex_lock(&conn->mutex);

		if (!event->param.conn.private_data ||
			(event->param.conn.private_data_len < sizeof(struct cm_priv_reject))) {
			ptl_warn("Invalid reject private data size (%d, %zd)\n",
					 event->param.conn.private_data_len, 
					 sizeof(struct cm_priv_reject));
			pthread_mutex_unlock(&conn->mutex);
			break;
		}

		rej = event->param.conn.private_data;

		if (rej->reason == REJECT_REASON_CONNECTED ||
			rej->reason == REJECT_REASON_CONNECTING) {
			/* Both sides tried to connect at the same time. This is
			 * good. */
			pthread_mutex_unlock(&conn->mutex);
			break;
		}
			
		/* TODO: handle other reject cases. */
		assert(rej->reason == REJECT_REASON_GOOD_SRQ);

		conn->state = CONN_STATE_DISCONNECTED;

#ifdef USE_XRC
		if ((conn->ni->options & PTL_NI_LOGICAL) &&
			rej->reason == REJECT_REASON_GOOD_SRQ) {

			struct rank_entry *entry;
			conn_t *main_connect;

			/* The connection list must be empty, since we're still
			 * trying to connect. */
			assert(list_empty(&conn->list));

			entry = container_of(conn, struct rank_entry, connect);
			main_connect = &ni->logical.rank_table[entry->main_rank].connect;

			assert(conn != main_connect);

			entry->remote_xrc_srq_num = rej->xrc_srq_num;

			/* We can now connect to the real endpoint. */
			conn->state = CONN_STATE_XRC_CONNECTED;

			pthread_spin_lock(&main_connect->wait_list_lock);

			conn->main_connect = main_connect;

			if (main_connect->state == CONN_STATE_DISCONNECTED) {
				list_add_tail(&conn->list, &main_connect->list);
				init_connect(ni, main_connect);
				pthread_spin_unlock(&main_connect->wait_list_lock);
			}
			else if (main_connect->state == CONN_STATE_CONNECTED) {
				pthread_spin_unlock(&main_connect->wait_list_lock);
				flush_pending_xi_xt(conn);
			}
			else {
				/* move xi/xt so they will be processed when the node is
				 * connected. */
				pthread_spin_lock(&conn->wait_list_lock);
				list_splice_init(&conn->buf_list, &main_connect->buf_list);
				list_splice_init(&conn->xt_list, &main_connect->xt_list);
				pthread_spin_unlock(&conn->wait_list_lock);
				pthread_spin_unlock(&main_connect->wait_list_lock);
			}
		}
#endif

		rdma_destroy_qp(conn->rdma.cm_id);
		rdma_destroy_id(conn->rdma.cm_id);
		conn->rdma.cm_id = NULL;

		pthread_mutex_unlock(&conn->mutex);

		conn_put(conn);
		break;

	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_DISCONNECTED:
	case RDMA_CM_EVENT_TIMEWAIT_EXIT:
		pthread_mutex_lock(&conn->mutex);
		if (conn->state != CONN_STATE_DISCONNECTED) {
			conn->state = CONN_STATE_DISCONNECTED;
			conn->rdma.cm_id->context = NULL;
			rdma_destroy_qp(conn->rdma.cm_id);
			conn->rdma.cm_id = NULL;

			pthread_mutex_unlock(&conn->mutex);

			conn_put(conn);
		} else {
			pthread_mutex_unlock(&conn->mutex);
		}
		break;

	default:
		ptl_warn("Got unexpected CM event: %d\n", event->event);
		break;
	}

 done:
	rdma_ack_cm_event(event);
}
