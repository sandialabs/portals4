/*
 * ptl_ni.c
 */

#include "ptl_loc.h"

#include <sys/ioctl.h>
#include <search.h>

unsigned short ptl_ni_port(ni_t *ni)
{
	return PTL_NI_PORT + ni->ni_type;
}

/* Get the default interface index. Returns PTL_IFACE_DEFAULT if none
 * is found. */
static int get_default_iface(gbl_t *gbl)
{
	int iface;

	/* Default interface is ib0 (iface==0). */
	if (if_nametoindex("ib0") > 0) {
		iface = 0;
		goto done;
	}

	iface = PTL_IFACE_DEFAULT;

done:
	return iface;
}

static void init_nid_connect(ni_t *ni, struct nid_connect *connect)
{
	memset(connect, 0, sizeof(*connect));

	pthread_mutex_init(&connect->mutex, NULL);
	connect->state = GBLN_DISCONNECTED;
	INIT_LIST_HEAD(&connect->xi_list);
	INIT_LIST_HEAD(&connect->xt_list);
	INIT_LIST_HEAD(&connect->list);
	connect->ni = ni;
}

static int compare_nid_pid(const void *a, const void *b)
{
	const struct rank_entry *entry1 = a;
	const struct rank_entry *entry2 = b;

	if (entry1->nid == entry2->nid)
		return(entry1->pid - entry2->pid);
	else
		return(entry1->nid - entry2->nid);
}

static int compare_rank(const void *a, const void *b)
{
	const struct rank_entry *entry1 = a;
	const struct rank_entry *entry2 = b;

	return(entry1->rank - entry2->rank);
}

static int create_tables(ni_t *ni, ptl_size_t map_size, ptl_process_t *actual_mapping)
{
	int i;
	ptl_nid_t prev_nid;
	int err;
	int main_rank;

	assert(ni->options & PTL_NI_LOGICAL);

	ni->logical.rank_table = calloc(map_size, sizeof(struct rank_entry));
	if (ni->logical.rank_table == NULL) {
		err = PTL_NO_SPACE;
		goto error;
	}
	ni->logical.map_size = map_size;

	for (i=0; i<map_size; i++) {
		struct rank_entry *entry = &ni->logical.rank_table[i];
		struct nid_connect *connect = &entry->connect;

		entry->rank = i;
		entry->nid = actual_mapping[i].phys.nid;
		entry->pid = actual_mapping[i].phys.pid;

		init_nid_connect(ni, connect);

		/* Get the IP address from the rank table for that NID. */
		connect->sin.sin_family = AF_INET;
		connect->sin.sin_addr.s_addr = nid_to_addr(entry->nid);
		connect->sin.sin_port = pid_to_port(entry->pid);
	}

	/* Sort the rank table by NID to find the unique nids. Be careful
	 * here because some pointers won't be valid anymore, until the
	 * table is sorted back by ranks. */
	qsort(ni->logical.rank_table, map_size, sizeof(struct rank_entry), compare_nid_pid);

	prev_nid = ni->logical.rank_table[0].nid + 1;
	main_rank = -1;

	for (i=0; i<map_size; i++) {
		struct rank_entry *entry = &ni->logical.rank_table[i];

		/* Find the main rank for that node. */
		if (entry->nid != prev_nid) {
			/* New NID. */
			prev_nid = entry->nid;
			main_rank = entry->rank;

			if (ni->id.rank == main_rank)
				ni->logical.is_main = 1;
		}

		entry->main_rank = main_rank;
	}


	/* Sort back the rank table by rank. */
	qsort(ni->logical.rank_table, map_size, sizeof(struct rank_entry), compare_rank);

	return 0;

 error:
	return err;
}

/*
 * compare the id's of two connect records
 */
static int compare_id(const void *a, const void *b)
{
	const struct nid_connect *c1 = a;
	const struct nid_connect *c2 = b;

	return (c1->id.phys.nid != c2->id.phys.nid) ? (c1->id.phys.nid - c2->id.phys.nid)
						    : (c1->id.phys.pid - c2->id.phys.pid);
}

/* Find the connection for a destination. In case of a physical NI,
 * the connection record will be created if it doesn't exist. */
struct nid_connect *get_connect_for_id(ni_t *ni, const ptl_process_t *id)
{
	struct nid_connect *connect;
	void **ret;

	if (ni->options & PTL_NI_LOGICAL) {
		/* Logical */
		if (unlikely(id->rank >= ni->logical.map_size)) {
			ptl_warn("Invalid rank (%d >= %d)\n",
					 id->rank, ni->logical.map_size);
			return NULL;
		}

		connect = &ni->logical.rank_table[id->rank].connect;

	} else {
		/* Physical */
		pthread_mutex_lock(&ni->physical.lock);

		ret = tfind(id, &ni->physical.tree, compare_id);
		if (ret) {
			connect = *ret;
		} else {
			/* Not found. Allocate and insert. */
			connect = ptl_malloc(sizeof(*connect));
			if (!connect) {
				pthread_mutex_unlock(&ni->physical.lock);
				WARN();
				return NULL;
			}

			init_nid_connect(ni, connect);
			connect->id = *id;

			/* Get the IP address from the NID. */
			connect->sin.sin_family = AF_INET;
			connect->sin.sin_addr.s_addr = nid_to_addr(id->phys.nid);
			connect->sin.sin_port = pid_to_port(id->phys.pid);

			ret = tsearch(connect, &ni->physical.tree, compare_id);
			if (!ret) {
				/* Insertion failed. */
				WARN();
				free(connect);
				connect = NULL;
			}
		}
		
		pthread_mutex_unlock(&ni->physical.lock);
	}

	return connect;
}

/* TODO finish this */
static ptl_ni_limits_t default_ni_limits = {
	.max_entries		= 123,
	.max_overflow_entries	= 123,
	.max_mds		= 123,
	.max_cts		= 123,
	.max_eqs		= 123,
	.max_pt_index		= DEF_PT_INDEX,
	.max_iovecs		= 123,
	.max_list_size		= 123,
	.max_msg_size		= 123,
	.max_atomic_size	= 123,
};

static void set_limits(ni_t *ni, ptl_ni_limits_t *desired)
{
	if (desired)
		ni->limits = *desired;
	else
		ni->limits = default_ni_limits;

	if (ni->limits.max_pt_index > MAX_PT_INDEX)
		ni->limits.max_pt_index		= MAX_PT_INDEX;
	if (ni->limits.max_pt_index < MIN_PT_INDEX)
		ni->limits.max_pt_index		= MIN_PT_INDEX;
}

static int ni_map(gbl_t *gbl, ni_t *ni,
		  ptl_size_t map_size,
		  ptl_process_t *desired_mapping)
{
	ptl_warn("TODO implement mapping\n");
	return PTL_OK;
}

static void ni_rcqp_stop(ni_t *ni)
{
	int i;
	const int map_size = ni->logical.map_size;

	if (ni->options & PTL_NI_LOGICAL) {
		for (i=0; i<map_size; i++) {
			struct nid_connect *connect = &ni->logical.rank_table[i].connect;
		
			pthread_mutex_lock(&connect->mutex);
			if (connect->state != GBLN_DISCONNECTED) {
				rdma_disconnect(connect->cm_id);
			}
			pthread_mutex_unlock(&connect->mutex);
		}
	} else {
		// todo: physical walk and disconnect
	}
}

static int ni_rcqp_cleanup(ni_t *ni)
{
	struct ibv_wc wc;
	int n;
	buf_t *buf;

	while(1) {
		n = ibv_poll_cq(ni->cq, 1, &wc);
		if (n < 0)
			WARN();

		if (n != 1)
			break;

		buf = (buf_t *)(uintptr_t)wc.wr_id;
		buf_put(buf);
	}

	return PTL_OK;
}

/* convert ni option flags to a 2 bit type */
static inline int ni_options_to_type(unsigned int options)
{
	return (((options & PTL_NI_MATCHING) ? 1 : 0) << 1) |
		((options & PTL_NI_LOGICAL) ? 1 : 0);
}

/* Establish a new connection. connect is already locked. */
int init_connect(ni_t *ni, struct nid_connect *connect)
{
	if (debug)
		printf("Initiate connect with %x:%d\n",
			   connect->sin.sin_addr.s_addr, connect->sin.sin_port);

	assert(connect->state == GBLN_DISCONNECTED);

	connect->retry_resolve_addr = 3;
	connect->retry_resolve_route = 3;
	connect->retry_connect = 3;

	if (rdma_create_id(ni->iface->cm_channel, &connect->cm_id,
					   connect, RDMA_PS_TCP)) {
		WARN();
		return 1;
	}

	connect->state = GBLN_RESOLVING_ADDR;

	if (rdma_resolve_addr(connect->cm_id, NULL,
						  (struct sockaddr *)&connect->sin, 2000)) {
		ptl_warn("rdma_resolve_addr failed %x:%d\n",
				 connect->sin.sin_addr.s_addr, connect->sin.sin_port);
		connect->state = GBLN_DISCONNECTED;
		return 1;
	}

	if (debug)
		printf("Connection initiated successfully to %x:%d\n",
			   connect->sin.sin_addr.s_addr, connect->sin.sin_port);

	return 0;
}

/*
 * accept an RC connection request
 *	called while holding connect->mutex
 */
static int accept_connection_request(ni_t *ni, struct nid_connect *connect,
									 struct rdma_cm_event *event)
{
	struct rdma_conn_param conn_param;
	struct ibv_qp_init_attr init_attr;
	struct cm_priv_accept priv;

	connect->state = GBLN_CONNECTING;

	memset(&init_attr, 0, sizeof(init_attr));
	if (ni->options & PTL_NI_LOGICAL) {
		init_attr.qp_type = IBV_QPT_XRC;
		init_attr.xrc_domain = ni->logical.xrc_domain;
		init_attr.cap.max_send_wr = 0;
	} else {
		init_attr.qp_type = IBV_QPT_RC;
		init_attr.cap.max_send_wr = MAX_QP_SEND_WR * MAX_RDMA_WR_OUT;
	}
	init_attr.send_cq = ni->cq;
	init_attr.recv_cq = ni->cq;
	init_attr.srq = ni->srq;
	init_attr.cap.max_send_sge = MAX_INLINE_SGE;

	if (rdma_create_qp(event->id, NULL, &init_attr)) {
		connect->state = GBLN_DISCONNECTED;
		return 1;
	}

	connect->cm_id = event->id;
	event->id->context = connect;

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
		connect->state = GBLN_DISCONNECTED;
		return 1;
	}

	return 0;
}


/* Accept a connection request from/to a logical NI. */
static int accept_connection_request_logical(ni_t *ni, struct rdma_cm_event *event)
{
	int ret;
	struct nid_connect *connect;

	assert(ni->options & PTL_NI_LOGICAL);

	/* Accept the connection and give back our SRQ
	 * number. This will be a passive connection (ie, nothing
	 * will be sent from that side. */
	connect = ptl_malloc(sizeof(*connect));
	if (!connect) {
		WARN();
		return 1;
	}

	init_nid_connect(ni, connect);

	pthread_mutex_lock(&ni->logical.lock);
	list_add_tail(&connect->list, &ni->logical.connect_list);
	pthread_mutex_unlock(&ni->logical.lock);

	pthread_mutex_lock(&connect->mutex);
	ret = accept_connection_request(ni, connect, event);
	if (ret) {
		WARN();
		assert(0);
		pthread_mutex_lock(&ni->logical.lock);
		list_del_init(&connect->list);
		pthread_mutex_unlock(&ni->logical.lock);
		pthread_mutex_unlock(&connect->mutex);

		free(connect);
	} else {
		pthread_mutex_unlock(&connect->mutex);
	}

	return ret;
}

/*
 * accept an RC connection request to self
 *	called while holding connect->mutex
 *	only used for physical NIs
 */
static int accept_connection_self(ni_t *ni, struct nid_connect *connect,
				  struct rdma_cm_event *event)
{
	struct rdma_conn_param conn_param;
	struct ibv_qp_init_attr init_attr;

	connect->state = GBLN_CONNECTING;

	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.qp_type = IBV_QPT_RC;
	init_attr.send_cq = ni->cq;
	init_attr.recv_cq = ni->cq;
	init_attr.srq = ni->srq;
	init_attr.cap.max_send_wr = MAX_QP_SEND_WR * MAX_RDMA_WR_OUT;
	init_attr.cap.max_send_sge = MAX_INLINE_SGE;

	if (rdma_create_qp(event->id, NULL, &init_attr)) {
		connect->state = GBLN_DISCONNECTED;
		return 1;
	}

	connect->cm_id = event->id;
	event->id->context = connect;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;

	if (rdma_accept(event->id, &conn_param)) {
		rdma_destroy_qp(event->id);
		connect->state = GBLN_DISCONNECTED;
		return 1;
	}

	return 0;
}

/* connect is locked. */
void flush_pending_xi_xt(struct nid_connect *connect)
{
	xi_t *xi;
	xt_t *xt;

	assert(pthread_mutex_trylock(&connect->mutex) != 0);

	while(!list_empty(&connect->xi_list)) {
		xi = list_first_entry(&connect->xi_list, xi_t, connect_pending_list);
		list_del_init(&xi->connect_pending_list);
		pthread_mutex_unlock(&connect->mutex);
		process_init(xi);
			
		pthread_mutex_lock(&connect->mutex);
	}

	while(!list_empty(&connect->xt_list)) {
		xt = list_first_entry(&connect->xt_list, xt_t, connect_pending_list);
		list_del_init(&xt->connect_pending_list);
		pthread_mutex_unlock(&connect->mutex);
		process_tgt(xt);

		pthread_mutex_lock(&connect->mutex);
	}
}

/*
 * process RC connection request event
 *	only used for physical NIs
 */
static int process_connect_request(struct iface *iface, struct rdma_cm_event *event)
{
	const struct cm_priv_request *priv;
	struct cm_priv_reject rej;
	struct nid_connect *connect;
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
	connect = get_connect_for_id(ni, &priv->src_id);

	pthread_mutex_lock(&connect->mutex);

	switch (connect->state) {
	case GBLN_CONNECTED:
		/* We received a connection request but we are already connected. Reject it. */
		rej.reason = REJECT_REASON_CONNECTED;
		pthread_mutex_unlock(&connect->mutex);

		goto reject;
		break;

	case GBLN_CONNECTING:
		assert(ni->options & PTL_NI_PHYSICAL);

		if (ni->options & PTL_NI_PHYSICAL) {
			/* we received a connection request but we are already connecting
			 * - accept connection from higher id
			 * - reject connection from lower id
			 * - accept connection from self, but cleanup
			 */
			c = compare_id(&priv->src_id, &ni->id);
			if (c > 0) {
				ret = accept_connection_request(ni, connect, event);
			} else if (c < 0) {
				ret = rdma_reject (event->id, NULL, 0);
			} else {
				ret = accept_connection_self(ni, connect, event);
			}
		}
		break;

	case GBLN_DISCONNECTED:
		/* we received a connection request and we are disconnected
		   - accept it
		*/
		ret = accept_connection_request(ni, connect, event);
		break;

	default:
		/* should never happen */
		assert(0);
		break;
	}

	pthread_mutex_unlock(&connect->mutex);

 done:
	return ret;

 reject:
	rdma_reject(event->id, &rej, sizeof(rej));
	return 1;
}

static void process_cm_event(EV_P_ ev_io *w, int revents)
{
	struct iface *iface = w->data;
	ni_t *ni;
	struct rdma_cm_event *event;
	struct nid_connect *connect;
	struct rdma_conn_param conn_param;
	struct cm_priv_request priv;
	struct ibv_qp_init_attr init;
	const struct cm_priv_reject *rej;

	if (debug)
		printf("Rank got a CM event\n");

	if (rdma_get_cm_event(iface->cm_channel, &event)) 
		return;

	connect = event->id->context;

	if (debug)
		printf("Rank got CM event %d for id %p\n", event->event, event->id);

	switch(event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		pthread_mutex_lock(&connect->mutex);

		assert(connect->cm_id == event->id);
		if (rdma_resolve_route(connect->cm_id, 2000)) {
			//todo 
			abort();
		} else {
			connect->state = GBLN_RESOLVING_ROUTE;
		}
		pthread_mutex_unlock(&connect->mutex);
		break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		assert(connect->cm_id == event->id);

		memset(&conn_param, 0, sizeof conn_param);

		conn_param.responder_resources = 1;
		conn_param.initiator_depth = 1;
		conn_param.retry_count = 5;
		conn_param.private_data = &priv;
		conn_param.private_data_len = sizeof(priv);

		ni = connect->ni;

		/* Create the QP. */
		memset(&init, 0, sizeof(init));
		init.qp_context			= ni;
		init.send_cq			= ni->cq;
		init.recv_cq			= ni->cq;
		init.cap.max_send_wr		= MAX_QP_SEND_WR * MAX_RDMA_WR_OUT;
		init.cap.max_recv_wr		= 0;
		init.cap.max_send_sge		= MAX_INLINE_SGE;
		init.cap.max_recv_sge		= 10;

		if (ni->options & PTL_NI_LOGICAL) {
			init.qp_type			= IBV_QPT_XRC;
			init.xrc_domain			= ni->logical.xrc_domain;

			priv.src_id.rank = ni->id.rank; /* todo: ni->id same as connect->id ? */
		} else {
			init.qp_type			= IBV_QPT_RC;
			init.srq = ni->srq;
			priv.src_id = connect->id;
		}
		priv.options = ni->options;

		pthread_mutex_lock(&connect->mutex);

		if (rdma_create_qp(connect->cm_id, NULL, &init)) {
			WARN();
			//todo
			abort();
			//err = PTL_FAIL;
			//goto err1;
		}

		if (rdma_connect(connect->cm_id, &conn_param)) {
			//todo 
			abort();
		} else {
			connect->state = GBLN_CONNECTING;
		}

		pthread_mutex_unlock(&connect->mutex);

		break;

	case RDMA_CM_EVENT_ESTABLISHED: {
		pthread_mutex_lock(&connect->mutex);

		connect->state = GBLN_CONNECTED;

		ni = connect->ni;

		if ((ni->options & PTL_NI_LOGICAL) &&
			(event->param.conn.private_data_len)) {
			/* If we have private data, it's that side asked for the
			 * connection (as opposed to accepting an incoming
			 * request). */
			const struct cm_priv_accept *priv_accept = event->param.conn.private_data;
			struct rank_entry *entry = container_of(connect, struct rank_entry, connect);

			/* Should not be set yet. */
			assert(entry->remote_xrc_srq_num == 0);

			entry->remote_xrc_srq_num = priv_accept->xrc_srq_num;

			/* Flush the posted requests/replies. */
			while(!list_empty(&connect->list)) {
				struct nid_connect *conn = list_first_entry(&connect->list, struct nid_connect, list);

				list_del_init(&conn->list);

				pthread_mutex_unlock(&connect->mutex);

				pthread_mutex_lock(&conn->mutex);
				flush_pending_xi_xt(conn);
				pthread_mutex_unlock(&conn->mutex);

				pthread_mutex_lock(&connect->mutex);
			}
		}

		flush_pending_xi_xt(connect);
		pthread_mutex_unlock(&connect->mutex);
	}
		break;

	case RDMA_CM_EVENT_CONNECT_REQUEST:
		process_connect_request(iface, event);
		break;

	case RDMA_CM_EVENT_REJECTED:
		pthread_mutex_lock(&connect->mutex);

		connect->state = GBLN_DISCONNECTED;

		if (!event->param.conn.private_data ||
			(event->param.conn.private_data_len < sizeof(struct cm_priv_reject))) {
			ptl_warn("Invalid reject private data size (%d, %ld)\n",
					 event->param.conn.private_data_len, 
					 sizeof(struct cm_priv_reject));
			pthread_mutex_unlock(&connect->mutex);
			break;
		}

		rej = event->param.conn.private_data;

		/* TODO: handle other reject cases. */
		assert(rej->reason == REJECT_REASON_GOOD_SRQ);

		if ((connect->ni->options & PTL_NI_LOGICAL) &&
			rej->reason == REJECT_REASON_GOOD_SRQ) {

			struct rank_entry *entry;
			struct nid_connect *main_connect;

			/* The connection list must be empty, since we're still
			 * trying to connect. */
			assert(list_empty(&connect->list));

			ni = connect->ni;

			entry = container_of(connect, struct rank_entry, connect);
			main_connect = &ni->logical.rank_table[entry->main_rank].connect;

			assert(connect != main_connect);

			entry->remote_xrc_srq_num = rej->xrc_srq_num;

			/* We can now connect to the real endpoint. */
			connect->state = GBLN_DISCONNECTED;

			pthread_mutex_lock(&main_connect->mutex);

			connect->main_connect = main_connect;

			if (main_connect->state == GBLN_DISCONNECTED) {
				list_add_tail(&connect->list, &main_connect->list);
				init_connect(ni, main_connect);
				pthread_mutex_unlock(&main_connect->mutex);
			}
			else if (main_connect->state == GBLN_CONNECTED) {
				pthread_mutex_unlock(&main_connect->mutex);
				flush_pending_xi_xt(connect);
			}
			else {
				/* move xi/xt so they will be processed when the node is
				 * connected. */
				list_splice_init(&connect->xi_list, &main_connect->xi_list);
				list_splice_init(&connect->xt_list, &main_connect->xt_list);
				pthread_mutex_unlock(&main_connect->mutex);
			}
		}

		pthread_mutex_unlock(&connect->mutex);

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

/* Get the first IPv4 address for a device. returns INADDR_ANY on
 * error or if none exist. */
static in_addr_t get_ip_address(const char *ifname)
{
	int fd;
	struct ifreq devinfo;
	struct sockaddr_in *sin = (struct sockaddr_in*)&devinfo.ifr_addr;
	in_addr_t addr;

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	strncpy(devinfo.ifr_name, ifname, IFNAMSIZ);

	if (ioctl(fd, SIOCGIFADDR, &devinfo) == 0 &&
		sin->sin_family == AF_INET) {
		addr = sin->sin_addr.s_addr;
	} else {
		addr = htonl(INADDR_ANY);
	}

	close(fd);

	return addr;
}

/* If this rank is the main one on the NID, create the domain, else
 * attach to an existing one. */
static int get_xrc_domain(ni_t *ni)
{
	struct rank_entry *entry;
	struct rank_entry *main_entry;
	char domain_fname[100];
	int err;

	assert(ni->options & PTL_NI_LOGICAL);

	/* Create filename for our domain. */
	entry = &ni->logical.rank_table[ni->id.rank];
	main_entry = &ni->logical.rank_table[entry->main_rank];
	sprintf(domain_fname, "/tmp/p4-xrc-%u-%u-%u", 
			ni->gbl->jid, main_entry->nid, main_entry->pid);

	if (ni->logical.is_main) {

		ni->logical.xrc_domain_fd = open(domain_fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (ni->logical.xrc_domain_fd == -1) {
			err = PTL_FAIL;
			goto done;
		}

		/* Create XRC domain. */
		ni->logical.xrc_domain = ibv_open_xrc_domain(ni->iface->ibv_context,
											 ni->logical.xrc_domain_fd, O_CREAT);
		if (!ni->logical.xrc_domain) {
			ptl_warn("unable to open xrc domain\n");
			err = PTL_FAIL;
			goto done;
		}

	} else {
		int try;
		
		/* Open domain file. Try for 10 seconds. */
		try = 50;
		do {
			ni->logical.xrc_domain_fd = open(domain_fname, O_RDWR, S_IRUSR | S_IWUSR);
			if (ni->logical.xrc_domain_fd != -1)
				break;
			try --;
			usleep(200000);
		} while(try && ni->logical.xrc_domain_fd == -1);

		if (ni->logical.xrc_domain_fd == -1) {
			ptl_warn("unable to open xrc domain file = %s\n", domain_fname);
			err = PTL_FAIL;
			goto done;
		}

		
		/* Open XRC domain. Try for 10 seconds. */
		try = 10;
		do {
			ni->logical.xrc_domain = ibv_open_xrc_domain(ni->iface->ibv_context,
												 ni->logical.xrc_domain_fd, 0);
		} while(try && !ni->logical.xrc_domain);

		if (!ni->logical.xrc_domain) {
			ptl_warn("unable to open xrc domain\n");
			err = PTL_FAIL;
			goto done;
		}
	}

	err = PTL_OK;

 done:
	return err;
}

static int init_ib_srq(ni_t *ni)
{
	struct ibv_srq_init_attr srq_init_attr;
	struct iface *iface = ni->iface;
	int err;
	int i;

	srq_init_attr.srq_context = ni;
	srq_init_attr.attr.max_wr = 100; /* todo: adjust */
	srq_init_attr.attr.max_sge = 1;
	srq_init_attr.attr.srq_limit = 0; /* should be ignored */

	if (ni->options & PTL_NI_LOGICAL) {
		/* Create XRC SRQ. */
		ni->srq = ibv_create_xrc_srq(iface->pd, ni->logical.xrc_domain,
									 ni->cq, &srq_init_attr);
	} else {
		/* Create regular SRQ. */
		ni->srq = ibv_create_srq(iface->pd, &srq_init_attr);
	}

	if (!ni->srq) {
		ptl_fatal("unable to create srq\n");
		goto done;
	}

	for (i = 0; i < srq_init_attr.attr.max_wr; i++) {
		err = post_recv(ni);
		if (err) {
			WARN();
			err = PTL_FAIL;
			goto done;
		}
	}

	err = PTL_OK;

done:
	return err;
}

static int cleanup_ib(ni_t *ni)
{
	if (ni->srq) {
		ibv_destroy_srq(ni->srq);
		ni->srq = NULL;
	}

	if (ni->logical.xrc_domain_fd != -1) {
		close(ni->logical.xrc_domain_fd);
		ni->logical.xrc_domain_fd = -1;
	}

	if (ni->logical.xrc_domain) {
		ibv_close_xrc_domain(ni->logical.xrc_domain);
		ni->logical.xrc_domain = NULL;
	}

	if (ni->cq) {
		ibv_destroy_cq(ni->cq);
		ni->cq = NULL;
	}

	if (ni->ch) {
		ibv_destroy_comp_channel(ni->ch);
		ni->ch = NULL;
	}

	return PTL_OK;
}

/* Must be locked by gbl_mutex. port is in network order. */
static int bind_iface(struct iface *iface, unsigned int port)
{
	if (iface->listen_id) {
		/* Already bound. If we want to bind to the same port, or a
		 * random port then it's ok. */
		if (port == 0 || port == iface->sin.sin_port)
			return PTL_OK;

		ptl_warn("Interface already bound\n");
		return PTL_FAIL;
	}

	iface->sin.sin_port = port;

	/* Create a RDMA CM ID and bind it to retrieve the context and
	 * PD. These will be valid for as long as librdmacm is not
	 * unloaded, ie. when the program exits. */
	if (rdma_create_id(iface->cm_channel, &iface->listen_id, NULL, RDMA_PS_TCP)) {
		ptl_warn("unable to create CM ID\n");
		goto err1;
	}

	if (rdma_bind_addr(iface->listen_id, (struct sockaddr *)&iface->sin)) {
		ptl_warn("unable to bind to local address %x\n", iface->sin.sin_addr.s_addr);
		goto err1;
	}

	iface->sin.sin_port = rdma_get_src_port(iface->listen_id);

	rdma_query_id(iface->listen_id, &iface->ibv_context, &iface->pd);
	if (iface->ibv_context == NULL || iface->pd == NULL) {
		ptl_warn("unable to get the CM ID context or PD\n");
		goto err1;
	}

	return PTL_OK;

 err1:
	iface->ibv_context = NULL;
	iface->pd = NULL;
	if (iface->listen_id) {
		rdma_destroy_id(iface->listen_id);
		iface->listen_id = NULL;
	}

	return PTL_FAIL;
}

/* Must be locked by gbl_mutex. */
static int init_ib(struct iface *iface, ni_t *ni)
{
	int err;
	int flags;

	/* If it is a physical address, then we bind it. */
	if (ni->options & PTL_NI_PHYSICAL) {

		ni->id.phys.nid = addr_to_nid(&iface->sin);

		err = bind_iface(iface, pid_to_port(ni->id.phys.pid));
		if (err) {
			ptl_warn("Binding failed\n");
			goto err1;
		}
	}

	if ((ni->options & PTL_NI_PHYSICAL) &&
		(ni->id.phys.pid == PTL_PID_ANY)) {
		/* No well know PID was given. Retrieve the pid given by
		 * bind. */
		ni->id.phys.pid = ntohs(rdma_get_src_port(iface->listen_id));
	}

	/* Create CC, CQ, SRQ. */
	ni->ch = ibv_create_comp_channel(iface->ibv_context);
	if (!ni->ch) {
		ptl_warn("unable to create comp channel\n");
		goto err1;
	}

	flags = fcntl(ni->ch->fd, F_GETFL);
	if (fcntl(ni->ch->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		ptl_warn("Cannot set completion event channel to non blocking\n");
		goto err1;
	}

	ni->cq = ibv_create_cq(iface->ibv_context, MAX_QP_SEND_WR + MAX_QP_RECV_WR,
						   ni, ni->ch, 0);
	if (!ni->cq) {
		WARN();
		ptl_warn("unable to create cq\n");
		goto err1;
	}

	err = ibv_req_notify_cq(ni->cq, 0);
	if (err) {
		ptl_warn("unable to req notify\n");
		goto err1;
	}

	return PTL_OK;

 err1:
	cleanup_ib(ni);
	return PTL_FAIL;
}

static void cleanup_iface(struct iface *iface)
{
	if (iface->listen_id) {
		rdma_destroy_id(iface->listen_id);
		iface->listen_id = NULL;
		iface->listen = 0;
	}

	if (iface->cm_channel)
		rdma_destroy_event_channel(iface->cm_channel);

	iface->sin.sin_addr.s_addr = htonl(INADDR_ANY);
	iface->ifname[0] = 0;

	EVL_WATCH(ev_io_stop(evl.loop, &iface->cm_watcher));
}

/* Interface is being used by its first NI. gbl_mutex is already taken. */
static int init_iface(struct iface *iface, unsigned int ifacenum)
{
	int err;

	if (iface->ifname[0]) {
		/* Already initialized. */
		return PTL_OK;
	}

	/* Currently the interface name is ib followed by the interface
	 * number. In the future we may have a system to override that,
	 * for instance, by having a table or environment variable
	 * (PORTALS4_INTERFACE_0=10.2.0.0/16) */
	sprintf(iface->ifname, "ib%d", ifacenum);
	if (if_nametoindex(iface->ifname) == 0) {
		ptl_warn("The interface %s doesn't exist\n", iface->ifname);
		err = PTL_FAIL;
		goto err1;
	}

	iface->sin.sin_family = AF_INET;
	iface->sin.sin_addr.s_addr = get_ip_address(iface->ifname);
	if (iface->sin.sin_addr.s_addr == htonl(INADDR_ANY)) {
		ptl_warn("The interface %s doesn't have an IPv4 address\n", iface->ifname);
		err = PTL_FAIL;
		goto err1;
	}

	iface->cm_channel = rdma_create_event_channel();
	if (!iface->cm_channel) {
		ptl_warn("unable to create interface CM event channel\n");
		err = PTL_FAIL;
		goto err1;
	}

	/* Add a watcher for CM connections. */
	ev_io_init(&iface->cm_watcher, process_cm_event, iface->cm_channel->fd, EV_READ);
	iface->cm_watcher.data = iface;

	EVL_WATCH(ev_io_start(evl.loop, &iface->cm_watcher));

	return PTL_OK;

 err1:
		cleanup_iface(iface);
	return err;
}

/* Release the buffers still on the send_list and recv_list. */
static void release_buffers(ni_t *ni)
{
	buf_t *buf;

	while(!list_empty(&ni->send_list)) {
		struct list_head *entry = ni->send_list.next;
		list_del(entry);
		buf = list_entry(entry, buf_t, list);
		buf_put(buf);
	}

	while(!list_empty(&ni->recv_list)) {
		struct list_head *entry = ni->recv_list.next;
		list_del(entry);
		buf = list_entry(entry, buf_t, list);
		buf_put(buf);
	}
}

int PtlNIInit(ptl_interface_t ifacenum,
			  unsigned int options,
			  ptl_process_t *id,
			  ptl_ni_limits_t *desired,
			  ptl_ni_limits_t *actual,
			  ptl_size_t map_size,
			  ptl_process_t *desired_mapping,
			  ptl_process_t *actual_mapping,
			  ptl_handle_ni_t *ni_handle)
{
	int err;
	ni_t *ni;
	gbl_t *gbl;
	int ni_type;
	struct iface *iface;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		return err;
	}

	if (ifacenum == PTL_IFACE_DEFAULT) {
		ifacenum = get_default_iface(gbl);
		if (ifacenum == PTL_IFACE_DEFAULT) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	}
	iface = &gbl->iface[ifacenum];

	if (unlikely(CHECK_POINTER(ni_handle, ptl_handle_ni_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(desired && CHECK_POINTER(desired, ptl_ni_limits_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(actual && CHECK_POINTER(actual, ptl_ni_limits_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(options & ~_PTL_NI_INIT_OPTIONS)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!!(options & PTL_NI_MATCHING)
				 ^ !(options & PTL_NI_NO_MATCHING))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!!(options & PTL_NI_LOGICAL)
				 ^ !(options & PTL_NI_PHYSICAL))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (options & PTL_NI_LOGICAL) {
		/* We must have a desired mapping, either from that init, or from
		 * a previous one. */
		if ((!desired_mapping && !iface->actual_mapping) ||
			(desired_mapping && map_size == 0)) {
			err = PTL_ARG_INVALID;
			goto err1;
		}

		if (unlikely(map_size && desired_mapping &&
					 CHECK_RANGE(desired_mapping, ptl_process_t, map_size))) {
			err = PTL_ARG_INVALID;
			goto err1;
		}

		if (map_size &&
			iface->map_size &&
			map_size != iface->map_size) {
			err = PTL_ARG_INVALID;
			goto err1;
		}

		if (actual_mapping && 
			unlikely(map_size && CHECK_RANGE(actual_mapping, ptl_process_t, map_size))) {
			err = PTL_ARG_INVALID;
			goto err1;
		}

		ptl_test_rank = id->rank;
	}

	ni_type = ni_options_to_type(options);
	pthread_mutex_lock(&gbl->gbl_mutex);
	ni = gbl_lookup_ni(gbl, ifacenum, ni_type);
	if (ni) {
		(void)__sync_add_and_fetch(&ni->ref_cnt, 1);
		goto done;
	}
	err = init_iface(iface, ifacenum);
	if (err)
		goto err2;

	err = ni_alloc(&ni);
	if (unlikely(err))
		goto err2;

	ni->iface = iface;
	ni->ni_type = ni_type;
	ni->ref_cnt = 1;
	ni->options = options;
	ni->last_pt = -1;
	ni->gbl = gbl;
	ni->logical.xrc_domain_fd = -1;
	set_limits(ni, desired);
	ni->uid = geteuid();
	INIT_LIST_HEAD(&ni->md_list);
	INIT_LIST_HEAD(&ni->ct_list);
	INIT_LIST_HEAD(&ni->xi_wait_list);
	INIT_LIST_HEAD(&ni->xt_wait_list);
	INIT_LIST_HEAD(&ni->mr_list);
	INIT_LIST_HEAD(&ni->send_list);
	INIT_LIST_HEAD(&ni->recv_list);
	INIT_LIST_HEAD(&ni->logical.connect_list);
	pthread_spin_init(&ni->md_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->ct_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->xi_wait_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->xt_wait_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->mr_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->send_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->recv_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_mutex_init(&ni->pt_mutex, NULL);
	pthread_mutex_init(&ni->eq_wait_mutex, NULL);
	pthread_cond_init(&ni->eq_wait_cond, NULL);
	pthread_mutex_init(&ni->ct_wait_mutex, NULL);
	pthread_cond_init(&ni->ct_wait_cond, NULL);
	pthread_mutex_init(&ni->physical.lock, NULL);
	pthread_mutex_init(&ni->logical.lock, NULL);

	ni->pt = ptl_calloc(ni->limits.max_pt_index, sizeof(*ni->pt));
	if (unlikely(!ni->pt)) {
		err = PTL_NO_SPACE;
		goto err3;
	}

	/* For logical NIs, this contains the rank. For physical NIs, we
	 * get the PID, and the NID is ignored; both can be overriden later. */
	ni->id = *id;

	if (options & PTL_NI_LOGICAL) {
		err = ni_map(gbl, ni, map_size, desired_mapping);
		if (unlikely(err)) {
			goto err3;
		}
	}

	err = init_ib(iface, ni);
	if (err) {
		goto err3;
	}

	/* Create the rank table. */
	if (ni->options & PTL_NI_LOGICAL) {
		if (!iface->actual_mapping) {
			/* Don't have one yet. Allocate and fill-up now. */
			const int size = map_size * sizeof(ptl_process_t);

			iface->map_size = map_size;
			iface->actual_mapping = malloc(size);
			if (!iface->actual_mapping) {
				err = PTL_NO_SPACE;
				goto err3;
			}

			memcpy(iface->actual_mapping, desired_mapping, size);

		} else {
			/* Already have one. Ignore the given parameter and use
			 * the existing mapping. */
			desired_mapping = iface->actual_mapping;
			map_size = iface->map_size;
		}

		/* return mapping to caller. */
		if (actual_mapping) {
			const int size = map_size * sizeof(ptl_process_t);

			memcpy(actual_mapping, desired_mapping, size);
		}

		err = create_tables(ni, map_size, desired_mapping);
		if (err) {
			goto err3;
		}

		/* Retrieve the XRC domain name. */
		err = get_xrc_domain(ni);
		if (err) {
			goto err3;
		}
	}

	/* Create own SRQ. */
	err = init_ib_srq(ni);
	if (err) {
		goto err3;
	}

	/* Add a watcher for CQ events. */
	ev_io_init(&ni->cq_watcher, process_recv, ni->ch->fd, EV_READ);
	ni->cq_watcher.data = ni;
	EVL_WATCH(ev_io_start(evl.loop, &ni->cq_watcher));

	/* Ready to listen. */
	if ((ni->options & PTL_NI_PHYSICAL) &&
		!iface->listen) {
		if (rdma_listen(iface->listen_id, 0)) {
			ptl_warn("Failed to listen\n");
			goto err1;
		}

		iface->listen = 1;
		
		if (debug) {
			printf("CM listening on %x:%d\n", iface->sin.sin_addr.s_addr, iface->sin.sin_port);
		}
	}

	err = gbl_add_ni(gbl, ni);
	if (unlikely(err)) {
		goto err3;
	}

 done:
	pthread_mutex_unlock(&gbl->gbl_mutex);

	if (actual)
		*actual = ni->limits;

	if (actual_mapping) {
		// TODO write out mapping
	}

	*ni_handle = ni_to_handle(ni);

	gbl_put(gbl);
	return PTL_OK;

 err3:
	ni_put(ni);
 err2:
	pthread_mutex_unlock(&gbl->gbl_mutex);
 err1:
	gbl_put(gbl);
	return err;
}

static void interrupt_cts(ni_t *ni)
{
	struct list_head *l;
	ct_t *ct;

	pthread_spin_lock(&ni->ct_list_lock);
	pthread_mutex_lock(&ni->ct_wait_mutex);
	list_for_each(l, &ni->ct_list) {
		ct = list_entry(l, ct_t, list);
		ct->interrupt = 1;
	}
	pthread_cond_broadcast(&ni->ct_wait_cond);
	pthread_mutex_unlock(&ni->ct_wait_mutex);
	pthread_spin_unlock(&ni->ct_list_lock);
}

static void cleanup_mr_list(ni_t *ni)
{
#if 0
	struct list_head *l, *t;
	mr_t *mr;

	pthread_spin_lock(&ni->mr_list_lock);
	list_for_each_safe(l, t, &ni->mr_list) {
		list_del(l);
		mr = list_entry(l, mr_t, list);
		mr_put(mr);
	}
	pthread_spin_unlock(&ni->mr_list_lock);
#endif
}

static void ni_cleanup(ni_t *ni)
{
	interrupt_cts(ni);
	cleanup_mr_list(ni);

	ni_rcqp_stop(ni);

	EVL_WATCH(ev_io_stop(evl.loop, &ni->cq_watcher));

	ni_rcqp_cleanup(ni);

	cleanup_ib(ni);

	release_buffers(ni);

	if (ni->pt) {
		free(ni->pt);
		ni->pt = NULL;
	}

	pthread_mutex_destroy(&ni->ct_wait_mutex);
	pthread_cond_destroy(&ni->ct_wait_cond);
	pthread_mutex_destroy(&ni->eq_wait_mutex);
	pthread_cond_destroy(&ni->eq_wait_cond);
	pthread_mutex_destroy(&ni->pt_mutex);
	pthread_spin_destroy(&ni->md_list_lock);
	pthread_spin_destroy(&ni->ct_list_lock);
	pthread_spin_destroy(&ni->xi_wait_list_lock);
	pthread_spin_destroy(&ni->xt_wait_list_lock);
	pthread_spin_destroy(&ni->mr_list_lock);
	pthread_spin_destroy(&ni->send_list_lock);
}

int PtlNIFini(ptl_handle_ni_t ni_handle)
{
	int err;
	ni_t *ni;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		return err;
	}

	err = ni_get(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	pthread_mutex_lock(&gbl->gbl_mutex);
	if (__sync_sub_and_fetch(&ni->ref_cnt, 1) <= 0) {
		err = gbl_remove_ni(gbl, ni);
		if (err) {
			pthread_mutex_unlock(&gbl->gbl_mutex);
			goto err2;
		}

		ni_cleanup(ni);
		ni_put(ni);
	}
	pthread_mutex_unlock(&gbl->gbl_mutex);

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err2:
	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}

int PtlNIStatus(ptl_handle_ni_t ni_handle, ptl_sr_index_t index,
		ptl_sr_value_t *status)
{
	int err;
	ni_t *ni;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(CHECK_POINTER(status, ptl_sr_value_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(index >= _PTL_SR_LAST)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = ni_get(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	*status = ni->status[index];

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

int PtlNIHandle(ptl_handle_any_t handle, ptl_handle_ni_t *ni_handle)
{
	obj_t *obj;
	int err;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(CHECK_POINTER(ni_handle, ptl_handle_ni_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = obj_get(NULL, handle, &obj);
	if (unlikely(err))
		goto err1;

	if (!obj) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	*ni_handle = ni_to_handle(to_ni(obj));

	obj_put(obj);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}
