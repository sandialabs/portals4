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
void conn_init(conn_t *conn, ni_t *ni)
{
	memset(conn, 0, sizeof(*conn));

	pthread_mutex_init(&conn->mutex, NULL);
	pthread_spin_init(&conn->wait_list_lock, PTHREAD_PROCESS_PRIVATE);

	conn->ni = ni;
	conn->state = CONN_STATE_DISCONNECTED;
	conn->transport = transport_rdma;

	INIT_LIST_HEAD(&conn->buf_list);
	INIT_LIST_HEAD(&conn->list);
}

/**
 * Cleanup a conn_t struct.
 *
 * @param[in] conn
 */
void conn_fini(conn_t *conn)
{
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
static int compare_id(const void *a, const void *b)
{
	const conn_t *c1 = a;
	const conn_t *c2 = b;

	return (c1->id.phys.nid != c2->id.phys.nid) ?
			(c1->id.phys.nid - c2->id.phys.nid) :
			(c1->id.phys.pid - c2->id.phys.pid);
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
 * @return the conn_t
 */
conn_t *get_conn(ni_t *ni, ptl_process_t id)
{
	conn_t *conn;
	void **ret;

	if (ni->options & PTL_NI_LOGICAL) {
		if (unlikely(id.rank >= ni->logical.map_size)) {
			ptl_warn("Invalid rank (%d >= %d)\n",
				 id.rank, ni->logical.map_size);
			return NULL;
		}

		conn = &ni->logical.rank_table[id.rank].connect;
	} else {
		pthread_spin_lock(&ni->physical.lock);

		/* lookup in binary tree */
		ret = tfind(&id, &ni->physical.tree, compare_id);
		if (ret) {
			conn = *ret;
		} else {
			/* Not found. Allocate and insert. */
			conn = malloc(sizeof(*conn));
			if (!conn) {
				pthread_spin_unlock(&ni->physical.lock);
				WARN();
				return NULL;
			}

			conn_init(conn, ni);
			conn->id = id;

			/* Get the IP address from the NID. */
			conn->sin.sin_family = AF_INET;
			conn->sin.sin_addr.s_addr = nid_to_addr(id.phys.nid);
			conn->sin.sin_port = pid_to_port(id.phys.pid);

			/* insert new conn into binary tree */
			ret = tsearch(conn, &ni->physical.tree, compare_id);
			if (!ret) {
				WARN();
				free(conn);
				conn = NULL;
			}
		}
		
		pthread_spin_unlock(&ni->physical.lock);
	}

	return conn;
}

static void destroy_conn(conn_t *conn)
{
	if (conn->transport.type == CONN_TYPE_RDMA) {
		if (conn->rdma.cm_id) {
			rdma_disconnect(conn->rdma.cm_id);
			rdma_destroy_qp(conn->rdma.cm_id);
			rdma_destroy_id(conn->rdma.cm_id);
		}
	}
}

static void destroy_physical_conn(void *data)
{
	conn_t *conn = data;
		
	destroy_conn(conn);
	free(conn);
}

/**
 * Destroys all connections belonging to an NI
 */
void destroy_conns(ni_t *ni)
{
	if (ni->options & PTL_NI_LOGICAL) {
		unsigned int i;

		/* Destroy active connections. */
		for (i=0; i<ni->logical.map_size; i++) {
			conn_t *conn = &ni->logical.rank_table[i].connect;

			destroy_conn(conn);
		}

#ifdef USE_XRC
		/* Destroy passive connections. */
		while(!list_empty(&ni->logical.connect_list)) {
			conn_t *conn = list_first_entry(&ni->logical.connect_list, conn_t, list);

			list_del(&conn->list);
			destroy_conn(conn);
			free(conn);
		}
#endif
	} else {
		tdestroy(ni->physical.tree, destroy_physical_conn);
	}
}

/**
 * @param[in] ni
 * @param[in] conn
 *
 * @return status
 */
int init_connect(ni_t *ni, conn_t *conn)
{
	if (debug)
		printf("Initiate connect with %x:%d\n",
			   conn->sin.sin_addr.s_addr, conn->sin.sin_port);

	assert(conn->state == CONN_STATE_DISCONNECTED);

	conn->rdma.retry_resolve_addr = 3;
	conn->rdma.retry_resolve_route = 3;
	conn->rdma.retry_connect = 3;

	if (rdma_create_id(ni->iface->cm_channel, &conn->rdma.cm_id,
			   conn, RDMA_PS_TCP)) {
		WARN();
		return PTL_FAIL;
	}

	conn->state = CONN_STATE_RESOLVING_ADDR;

	if (rdma_resolve_addr(conn->rdma.cm_id, NULL,
			      (struct sockaddr *)&conn->sin, get_param(PTL_RDMA_TIMEOUT))) {
		ptl_warn("rdma_resolve_addr failed %x:%d\n",
				 conn->sin.sin_addr.s_addr, conn->sin.sin_port);
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	if (debug)
		printf("Connection initiated successfully to %x:%d\n",
			   conn->sin.sin_addr.s_addr, conn->sin.sin_port);

	return PTL_OK;
}

/**
 * @param[in] ni
 * @param[in] conn
 * @param[in] event
 *
 * @return status
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
		init_attr.cap.max_send_wr = get_param(PTL_MAX_QP_SEND_WR) +
					    get_param(PTL_MAX_RDMA_WR_OUT);
	}
	init_attr.send_cq = ni->rdma.cq;
	init_attr.recv_cq = ni->rdma.cq;
	init_attr.srq = ni->rdma.srq;
	init_attr.cap.max_send_sge = max(get_param(PTL_MAX_INLINE_SGE), get_param(PTL_MAX_QP_SEND_SGE));
	init_attr.cap.max_inline_data = 512;

	if (rdma_create_qp(event->id, ni->iface->pd, &init_attr)) {
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	conn->rdma.cm_id = event->id;
	event->id->context = conn;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;

	if (ni->options & PTL_NI_LOGICAL) {
		conn_param.private_data = &priv;
		conn_param.private_data_len = sizeof(priv);

#ifdef USE_XRC
		priv.xrc_srq_num = ni->rdma.srq->xrc_srq_num;
#endif
	}

	if (rdma_accept(event->id, &conn_param)) {
		rdma_destroy_qp(event->id);
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
	conn = malloc(sizeof(*conn));
	if (!conn) {
		WARN();
		return PTL_NO_SPACE;
	}

	conn_init(conn, ni);

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

		free(conn);
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
	init_attr.cap.max_send_wr = get_param(PTL_MAX_QP_SEND_WR) +
				    get_param(PTL_MAX_RDMA_WR_OUT);
	init_attr.cap.max_send_sge = max(get_param(PTL_MAX_INLINE_SGE), get_param(PTL_MAX_QP_SEND_SGE));
	init_attr.cap.max_inline_data = 512;

	if (rdma_create_qp(event->id, ni->iface->pd, &init_attr)) {
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	ni->rdma.self_cm_id = event->id;

	event->id->context = conn;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;

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
		rej.reason = REJECT_REASON_ERROR;
		goto reject;
	}

	pthread_mutex_lock(&conn->mutex);

	switch (conn->state) {
	case CONN_STATE_CONNECTED:
		/* We received a connection request but we are already connected. Reject it. */
		rej.reason = REJECT_REASON_CONNECTED;
		pthread_mutex_unlock(&conn->mutex);
		goto reject;
		break;

	case CONN_STATE_DISCONNECTED:
		/* we received a connection request and we are disconnected
		   - accept it
		*/
		ret = accept_connection_request(ni, conn, event);
		break;

	default:
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

	if (debug)
		printf("Rank got CM event %d for id %p\n", event->event, event->id);

	switch(event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		pthread_mutex_lock(&conn->mutex);

		if (conn->rdma.cm_id == event->id) {
			if (rdma_resolve_route(conn->rdma.cm_id, get_param(PTL_RDMA_TIMEOUT))) {
				//todo 
				abort();
			} else {
				conn->state = CONN_STATE_RESOLVING_ROUTE;
			}
		} else {
			/* That connection attempt got overriden by a higher
			 * priority connect request from the same node we were
			 * trying to connect to. See process_connect_request(). Do
			 * nothing. */	
		}

		pthread_mutex_unlock(&conn->mutex);
		break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		memset(&conn_param, 0, sizeof conn_param);

		conn_param.responder_resources	= 1;
		conn_param.initiator_depth	= 1;
		conn_param.retry_count		= 5;
		conn_param.private_data		= &priv;
		conn_param.private_data_len	= sizeof(priv);

		ni = conn->ni;

		/* Create the QP. */
		memset(&init, 0, sizeof(init));
		init.qp_context			= ni;
		init.send_cq			= ni->rdma.cq;
		init.recv_cq			= ni->rdma.cq;
		init.cap.max_send_wr		= get_param(PTL_MAX_QP_SEND_WR) +
						  get_param(PTL_MAX_RDMA_WR_OUT);
		init.cap.max_recv_wr		= 0;
		init.cap.max_send_sge		= max(get_param(PTL_MAX_INLINE_SGE), get_param(PTL_MAX_QP_SEND_SGE));
		init.cap.max_recv_sge		= get_param(PTL_MAX_QP_RECV_SGE);
		init.cap.max_inline_data = 512;

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

		pthread_mutex_lock(&conn->mutex);

		if (conn->rdma.cm_id == event->id) {
			if (rdma_create_qp(conn->rdma.cm_id, ni->iface->pd, &init)) {
				WARN();
				//todo
				abort();
				//err = PTL_FAIL;
				//goto err1;
			}

			if (rdma_connect(conn->rdma.cm_id, &conn_param)) {
				//todo 
				abort();
			} else {
				conn->state = CONN_STATE_CONNECTING;
			}
		}  else {
			/* That connection attempt got overriden by a higher
			 * priority connect request from the same node we were
			 * trying to connect to. See process_connect_request(). Do
			 * nothing. */	
		}

		pthread_mutex_unlock(&conn->mutex);

		break;

	case RDMA_CM_EVENT_ESTABLISHED: {
		pthread_mutex_lock(&conn->mutex);

		conn->state = CONN_STATE_CONNECTED;

		ni = conn->ni;

		if ((ni->options & PTL_NI_LOGICAL) &&
			(event->param.conn.private_data_len)) {
			/* If we have private data, it's that side asked for the
			 * connection (as opposed to accepting an incoming
			 * request). */
#ifdef USE_XRC
			const struct cm_priv_accept *priv_accept = event->param.conn.private_data;
			struct rank_entry *entry = container_of(conn, struct rank_entry, connect);

			/* Should not be set yet. */
			assert(entry->remote_xrc_srq_num == 0);

			entry->remote_xrc_srq_num = priv_accept->xrc_srq_num;
#endif

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

			ni = conn->ni;

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

		pthread_mutex_unlock(&conn->mutex);

		/* todo: destroy QP and reset connect. */
		break;

	case RDMA_CM_EVENT_DISCONNECTED:
		break;

	default:
		ptl_warn("Got unexpected CM event: %d\n", event->event);
		break;
	};

	rdma_ack_cm_event(event);

	return;
}
