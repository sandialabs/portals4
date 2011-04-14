/*
 * ptl_conn.c - connection management
 */

#include "ptl_loc.h"

void conn_init(ni_t *ni, conn_t *conn)
{
	memset(conn, 0, sizeof(*conn));

	pthread_mutex_init(&conn->mutex, NULL);
	pthread_spin_init(&conn->wait_list_lock, PTHREAD_PROCESS_PRIVATE);

	conn->ni = ni;
	conn->state = CONN_STATE_DISCONNECTED;

	INIT_LIST_HEAD(&conn->xi_list);
	INIT_LIST_HEAD(&conn->xt_list);
	INIT_LIST_HEAD(&conn->list);
}

void conn_fini(conn_t *conn)
{
	pthread_mutex_destroy(&conn->mutex);
	pthread_spin_destroy(&conn->wait_list_lock);
}

static int compare_id(const void *a, const void *b)
{
	const conn_t *c1 = a;
	const conn_t *c2 = b;

	return (c1->id.phys.nid != c2->id.phys.nid) ? (c1->id.phys.nid - c2->id.phys.nid)
						    : (c1->id.phys.pid - c2->id.phys.pid);
}

conn_t *get_conn(ni_t *ni, const ptl_process_t *id)
{
	conn_t *conn;
	void **ret;

	if (ni->options & PTL_NI_LOGICAL) {
		if (unlikely(id->rank >= ni->logical.map_size)) {
			ptl_warn("Invalid rank (%d >= %d)\n",
				 id->rank, ni->logical.map_size);
			return NULL;
		}

		conn = &ni->logical.rank_table[id->rank].connect;
	} else {
		pthread_spin_lock(&ni->physical.lock);

		/* lookup in binary tree */
		ret = tfind(id, &ni->physical.tree, compare_id);
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

			conn_init(ni, conn);
			conn->id = *id;

			/* Get the IP address from the NID. */
			conn->sin.sin_family = AF_INET;
			conn->sin.sin_addr.s_addr = nid_to_addr(id->phys.nid);
			conn->sin.sin_port = pid_to_port(id->phys.pid);

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

int init_connect(ni_t *ni, conn_t *conn)
{
	if (debug)
		printf("Initiate connect with %x:%d\n",
			   conn->sin.sin_addr.s_addr, conn->sin.sin_port);

	assert(conn->state == CONN_STATE_DISCONNECTED);

	conn->retry_resolve_addr = 3;
	conn->retry_resolve_route = 3;
	conn->retry_connect = 3;

	if (rdma_create_id(ni->iface->cm_channel, &conn->cm_id,
			   conn, RDMA_PS_TCP)) {
		WARN();
		return PTL_FAIL;
	}

	conn->state = CONN_STATE_RESOLVING_ADDR;

	if (rdma_resolve_addr(conn->cm_id, NULL,
			      (struct sockaddr *)&conn->sin, 2000)) {
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

static int accept_connection_request(ni_t *ni, conn_t *conn,
				     struct rdma_cm_event *event)
{
	struct rdma_conn_param conn_param;
	struct ibv_qp_init_attr init_attr;
	struct cm_priv_accept priv;

	conn->state = CONN_STATE_CONNECTING;

	memset(&init_attr, 0, sizeof(init_attr));

	if (ni->options & PTL_NI_LOGICAL) {
		init_attr.qp_type = IBV_QPT_XRC;
		init_attr.xrc_domain = ni->logical.xrc_domain;
		init_attr.cap.max_send_wr = 0;
	} else {
		init_attr.qp_type = IBV_QPT_RC;
		init_attr.cap.max_send_wr = MAX_QP_SEND_WR + MAX_RDMA_WR_OUT;
	}
	init_attr.send_cq = ni->cq;
	init_attr.recv_cq = ni->cq;
	init_attr.srq = ni->srq;
	init_attr.cap.max_send_sge = MAX_INLINE_SGE;

	if (rdma_create_qp(event->id, NULL, &init_attr)) {
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	conn->cm_id = event->id;
	event->id->context = conn;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;

	if (ni->options & PTL_NI_LOGICAL) {
		conn_param.private_data = &priv;
		conn_param.private_data_len = sizeof(priv);

		priv.xrc_srq_num = ni->srq->xrc_srq_num;
	}

	if (rdma_accept(event->id, &conn_param)) {
		rdma_destroy_qp(event->id);
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	return PTL_OK;
}

/* Accept a connection request from/to a logical NI. */
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

	conn_init(ni, conn);

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

/*
 * accept an RC connection request to self
 *	called while holding connect->mutex
 *	only used for physical NIs
 */
static int accept_connection_self(ni_t *ni, conn_t *conn,
				  struct rdma_cm_event *event)
{
	struct rdma_conn_param conn_param;
	struct ibv_qp_init_attr init_attr;

	conn->state = CONN_STATE_CONNECTING;

	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.qp_type = IBV_QPT_RC;
	init_attr.send_cq = ni->cq;
	init_attr.recv_cq = ni->cq;
	init_attr.srq = ni->srq;
	init_attr.cap.max_send_wr = MAX_QP_SEND_WR + MAX_RDMA_WR_OUT;
	init_attr.cap.max_send_sge = MAX_INLINE_SGE;

	if (rdma_create_qp(event->id, NULL, &init_attr)) {
		conn->state = CONN_STATE_DISCONNECTED;
		return PTL_FAIL;
	}

	conn->cm_id = event->id;
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

void flush_pending_xi_xt(conn_t *conn)
{
	xi_t *xi;
	xt_t *xt;

	pthread_spin_lock(&conn->wait_list_lock);
	while(!list_empty(&conn->xi_list)) {
		xi = list_first_entry(&conn->xi_list, xi_t, list);
		list_del_init(&xi->list);
		pthread_spin_unlock(&conn->wait_list_lock);
		process_init(xi);
			
		pthread_spin_lock(&conn->wait_list_lock);
	}

	while(!list_empty(&conn->xt_list)) {
		xt = list_first_entry(&conn->xt_list, xt_t, list);
		list_del_init(&xt->list);
		pthread_spin_unlock(&conn->wait_list_lock);
		process_tgt(xt);

		pthread_spin_lock(&conn->wait_list_lock);
	}
	pthread_spin_unlock(&conn->wait_list_lock);
}

/*
 * process RC connection request event
 *	only used for physical NIs
 */
static int process_connect_request(struct iface *iface, struct rdma_cm_event *event)
{
	const struct cm_priv_request *priv;
	struct cm_priv_reject rej;
	conn_t *conn;
	int ret;
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

	if (ni->options & PTL_NI_LOGICAL) {
		if (ni->logical.is_main) {
			ret = accept_connection_request_logical(ni, event);
			if (!ret) {
				goto done;
			}
			
			WARN();
			rej.reason = REJECT_REASON_ERROR;
			rej.xrc_srq_num = ni->srq->xrc_srq_num;

		} else {
			/* If this is not the main process on this node, reject
			 * the connection but give out SRQ number. */	
			rej.reason = REJECT_REASON_GOOD_SRQ;
			rej.xrc_srq_num = ni->srq->xrc_srq_num;
		}

		goto reject;
	}

	/* From now on, it's only for connections to a physical NI. */
	conn = get_conn(ni, &priv->src_id);

	pthread_mutex_lock(&conn->mutex);

	switch (conn->state) {
	case CONN_STATE_CONNECTED:
		/* We received a connection request but we are already connected. Reject it. */
		rej.reason = REJECT_REASON_CONNECTED;
		pthread_mutex_unlock(&conn->mutex);

		goto reject;
		break;

	case CONN_STATE_CONNECTING:
		assert(ni->options & PTL_NI_PHYSICAL);

		if (ni->options & PTL_NI_PHYSICAL) {
			/* we received a connection request but we are already connecting
			 * - accept connection from higher id
			 * - reject connection from lower id
			 * - accept connection from self, but cleanup
			 */
			c = compare_id(&priv->src_id, &ni->id);
			if (c > 0) {
				ret = accept_connection_request(ni, conn, event);
			} else if (c < 0) {
				ret = rdma_reject (event->id, NULL, 0);
			} else {
				ret = accept_connection_self(ni, conn, event);
			}
		}
		break;

	case CONN_STATE_DISCONNECTED:
		/* we received a connection request and we are disconnected
		   - accept it
		*/
		ret = accept_connection_request(ni, conn, event);
		break;

	default:
		/* should never happen */
		assert(0);
		break;
	}

	pthread_mutex_unlock(&conn->mutex);

 done:
	return ret;

 reject:
	rdma_reject(event->id, &rej, sizeof(rej));
	return 1;
}

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

		assert(conn->cm_id == event->id);
		if (rdma_resolve_route(conn->cm_id, 2000)) {
			//todo 
			abort();
		} else {
			conn->state = CONN_STATE_RESOLVING_ROUTE;
		}
		pthread_mutex_unlock(&conn->mutex);
		break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		assert(conn->cm_id == event->id);

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
		init.send_cq			= ni->cq;
		init.recv_cq			= ni->cq;
		init.cap.max_send_wr		= MAX_QP_SEND_WR + MAX_RDMA_WR_OUT;
		init.cap.max_recv_wr		= 0;
		init.cap.max_send_sge		= MAX_INLINE_SGE;
		init.cap.max_recv_sge		= 10;

		if (ni->options & PTL_NI_LOGICAL) {
			init.qp_type			= IBV_QPT_XRC;
			init.xrc_domain			= ni->logical.xrc_domain;
			priv.src_id.rank		= ni->id.rank;
		} else {
			init.qp_type			= IBV_QPT_RC;
			init.srq			= ni->srq;
			priv.src_id			= conn->id;
		}
		priv.options			= ni->options;

		pthread_mutex_lock(&conn->mutex);

		if (rdma_create_qp(conn->cm_id, NULL, &init)) {
			WARN();
			//todo
			abort();
			//err = PTL_FAIL;
			//goto err1;
		}

		if (rdma_connect(conn->cm_id, &conn_param)) {
			//todo 
			abort();
		} else {
			conn->state = CONN_STATE_CONNECTING;
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

		flush_pending_xi_xt(conn);
		pthread_mutex_unlock(&conn->mutex);
	}
		break;

	case RDMA_CM_EVENT_CONNECT_REQUEST:
		process_connect_request(iface, event);
		break;

	case RDMA_CM_EVENT_REJECTED:
		pthread_mutex_lock(&conn->mutex);

		conn->state = CONN_STATE_DISCONNECTED;

		if (!event->param.conn.private_data ||
			(event->param.conn.private_data_len < sizeof(struct cm_priv_reject))) {
			ptl_warn("Invalid reject private data size (%d, %ld)\n",
					 event->param.conn.private_data_len, 
					 sizeof(struct cm_priv_reject));
			pthread_mutex_unlock(&conn->mutex);
			break;
		}

		rej = event->param.conn.private_data;

		/* TODO: handle other reject cases. */
		assert(rej->reason == REJECT_REASON_GOOD_SRQ);

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
				list_splice_init(&conn->xi_list, &main_connect->xi_list);
				list_splice_init(&conn->xt_list, &main_connect->xt_list);
				pthread_spin_unlock(&conn->wait_list_lock);
				pthread_spin_unlock(&main_connect->wait_list_lock);
			}
		}

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
