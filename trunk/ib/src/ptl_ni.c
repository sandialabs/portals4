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

/* Retrieve the shared memory filename, and mmap it. */
static int get_rank_table(ni_t *ni)
{
	struct rpc_msg rpc_msg;
	int err;
	void *m;
	gbl_t *gbl = ni->gbl;

	assert(ni->options & PTL_NI_LOGICAL);

	memset(&rpc_msg, 0, sizeof(rpc_msg));
	rpc_msg.type = QUERY_RANK_TABLE;
	rpc_msg.query_rank_table.rank = gbl->rank;
	rpc_msg.query_rank_table.local_rank = gbl->local_rank;
	rpc_msg.query_rank_table.xrc_srq_num  = ni->srq->xrc_srq_num;
	rpc_msg.query_rank_table.addr = ni->addr;
	err = rpc_get(gbl->rpc->to_server, &rpc_msg, &rpc_msg);
	if (err)
		goto err;

	if (rpc_msg.reply_rank_table.shmem_filesize == 0)
		goto err;

	ni->shmem.fd = open(rpc_msg.reply_rank_table.shmem_filename, O_RDONLY);
	if (ni->shmem.fd == -1)
		goto err;

	m = mmap(NULL, rpc_msg.reply_rank_table.shmem_filesize,
			 PROT_READ, MAP_SHARED, ni->shmem.fd, 0);
	if (m == MAP_FAILED)
		goto err;

	ni->shmem.m = (struct shared_config *)m;

	ni->shmem.rank_table = m + ni->shmem.m->rank_table_offset;

	return PTL_OK;

 err:
	if (ni->shmem.fd != -1) {
		close(ni->shmem.fd);
		ni->shmem.fd = -1;
	}
	return PTL_FAIL;
}

static void init_nid_connect(struct nid_connect *connect)
{
	memset(connect, 0, sizeof(*connect));

	pthread_mutex_init(&connect->mutex, NULL);
	connect->state = GBLN_DISCONNECTED;
	INIT_LIST_HEAD(&connect->xi_list);
	INIT_LIST_HEAD(&connect->xt_list);
}

static int compare_nid(const void *a, const void *b)
{
	const struct rank_to_nid *nid1 = a;
	const struct rank_to_nid *nid2 = b;

	return(nid1->nid - nid2->nid);
}

static int compare_rank(const void *a, const void *b)
{
	const struct rank_to_nid *nid1 = a;
	const struct rank_to_nid *nid2 = b;

	return(nid1->rank - nid2->rank);
}

/* Create a mapping from rank to NID. We needs this because the rank
 * table is in shared memory, and we need one cm_id per remote
 * node. */
static int create_rank_to_nid_table(ni_t *ni)
{
	gbl_t *gbl = ni->gbl;
	int i;
	ptl_nid_t prev_nid;
	struct nid_connect *connect;

	assert(ni->options & PTL_NI_LOGICAL);

	ni->logical.rank_to_nid_table = calloc(gbl->nranks, sizeof(struct rank_to_nid));
	if (ni->logical.rank_to_nid_table == NULL)
		goto error;

	ni->logical.nid_table = calloc(gbl->num_nids, sizeof(struct nid_connect));
	if (ni->logical.nid_table == NULL)
		goto error;

	for (i=0; i<gbl->nranks; i++) {
		struct rank_to_nid *elem1 = &ni->logical.rank_to_nid_table[i];
		struct rank_entry *elem2 = &ni->shmem.rank_table->elem[i];
		elem1->nid = elem2->nid;
		elem1->rank = elem2->rank;
	}

	/* Sort the rank_to_nid table to find the unique nids, and build the nid table. */
	prev_nid = ni->logical.rank_to_nid_table[0].nid + 1;
	connect = ni->logical.nid_table;
	connect --;
	qsort(ni->logical.rank_to_nid_table, gbl->nranks, sizeof(struct rank_to_nid), compare_nid);
	for (i=0; i<gbl->nranks; i++) {
		struct rank_to_nid *rtn = &ni->logical.rank_to_nid_table[i];
		
		if (rtn->nid != prev_nid) {
			/* New NID. */
			connect ++;

			init_nid_connect(connect);

			/* Get the IP address from the rank table for that NID. */
			connect->sin.sin_family = AF_INET;
			connect->sin.sin_addr.s_addr = ni->shmem.rank_table->elem[rtn->rank].addr;
			connect->sin.sin_port = htons(PTL_XRC_PORT);

			prev_nid = rtn->nid;
		}

		rtn->connect = connect;
	}

	/* Ensure we got the algo right. */
	assert(connect == &ni->logical.nid_table[gbl->num_nids-1]);

	/* Sort the rank_to_nid table based on the rank. */
	qsort(ni->logical.rank_to_nid_table, gbl->nranks, sizeof(struct rank_to_nid), compare_rank);

	return 0;

 error:
	return PTL_FAIL;
}

static int compare_nids(const void *a, const void *b)
{
	const struct nid_connect *c1 = a;
	const struct nid_connect *c2 = b;

	return(c1->id.phys.nid - c2->id.phys.nid);
}

/* Find the connection for a destination. In case of a physical NI,
 * the connection record will be created if it doesn't exist. */
struct nid_connect *get_connect_for_id(ni_t *ni, const ptl_process_t *id)
{
	struct nid_connect *connect;

	if (ni->options & PTL_NI_LOGICAL) {
		/* Logical */
		if (unlikely(id->rank >= ni->gbl->nranks)) {
			ptl_warn("Invalid rank (%d >= %d)\n",
					 id->rank, ni->gbl->nranks);
			return NULL;
		}

		connect = ni->logical.rank_to_nid_table[id->rank].connect;
	} else {
		struct nid_connect c;
		void **ret;

		/* Physical */
		pthread_mutex_lock(&ni->physical.lock);

		c.id.phys.nid = id->phys.nid;
		ret = tfind(&c, &ni->physical.tree, compare_nids);

		if (!ret) {
			/* Not found. Allocate and insert. */
			connect = malloc(sizeof(*connect));
			init_nid_connect(connect);
			connect->id = *id;

			/* Get the IP address from the NID. */
			connect->sin.sin_family = AF_INET;
			connect->sin.sin_addr.s_addr = nid_to_addr(id->phys.nid);
			connect->sin.sin_port = pid_to_port(id->phys.pid);

			if (connect) {
				ret = tsearch(connect, &ni->physical.tree, compare_nids);
				if (!ret) {
					/* Insertion failed. */
					free(connect);
					connect = NULL;
				}
			}
		} else {
			connect = *ret;
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

	if (ni->options & PTL_NI_LOGICAL) {
		for (i=0; i<ni->gbl->num_nids; i++) {
			struct nid_connect *connect = &ni->logical.nid_table[i];
		
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

/* Establish a new connection. connect is already locked. */
int init_connect(ni_t *ni, struct nid_connect *connect)
{
	if (debug)
		printf("Initiate connect with %x:%x\n",
			   connect->sin.sin_addr.s_addr, connect->sin.sin_port);
		
	assert(connect->state == GBLN_DISCONNECTED);

	connect->retry_resolve_addr = 3;
	connect->retry_resolve_route = 3;
	connect->retry_connect = 3;

	if (rdma_create_id(ni->cm_channel, &connect->cm_id,
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

static int process_connect_request(ni_t *ni, struct rdma_cm_event *event)
{
	const struct cm_priv_request *priv;
	struct rdma_conn_param conn_param;
	struct ibv_qp_init_attr init_attr;
	struct nid_connect *connect;

	assert(ni->options & PTL_NI_PHYSICAL);

	if (!event->param.conn.private_data ||
		(event->param.conn.private_data_len <
		sizeof(struct cm_priv_request)))
		return 1;

	priv = event->param.conn.private_data;

	connect = get_connect_for_id(ni, &priv->src_id);

	pthread_mutex_lock(&connect->mutex);

	if (connect->state != GBLN_DISCONNECTED) {
		/* Already connected. Should not get there. */
		abort();
		goto err;
	}

	connect->state = GBLN_CONNECTING;

	memset(&init_attr, 0, sizeof(struct ibv_qp_init_attr));
	init_attr.qp_type = IBV_QPT_RC;
	init_attr.send_cq = ni->cq;
	init_attr.recv_cq = ni->cq;
	init_attr.srq = ni->srq;
	init_attr.cap.max_send_wr = 50;
	init_attr.cap.max_send_sge = 1;

	if (rdma_create_qp(event->id, NULL, &init_attr)) {
		connect->state = GBLN_DISCONNECTED;
		goto err;
	}

	connect->cm_id = event->id;
	event->id->context = connect;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;

	if (rdma_accept(event->id, &conn_param)) {
		rdma_destroy_qp(event->id);
		connect->state = GBLN_DISCONNECTED;
		goto err;
	}

	pthread_mutex_unlock(&connect->mutex);
	return 0;

err:
	pthread_mutex_unlock(&connect->mutex);
	return 1;
}

static void process_cm_event(EV_P_ ev_io *w, int revents)
{
	ni_t *ni = w->data;
	struct rdma_cm_event *event;
	struct nid_connect *connect;
    struct rdma_conn_param conn_param;
	struct cm_priv_request priv;
	struct ibv_qp_init_attr init;

	if (debug)
		printf("Rank got a CM event\n");

	if (rdma_get_cm_event(ni->cm_channel, &event)) 
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
			init.xrc_domain			= ni->xrc_domain;

			priv.src_id.rank = ni->gbl->rank;
		} else {
			init.qp_type			= IBV_QPT_RC;
			init.srq = ni->srq;
			priv.src_id = connect->id;
		}

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
		connect->state = GBLN_CONNECTED;
		struct list_head *elem;
		xi_t *xi;
		xt_t *xt;

		pthread_mutex_lock(&connect->mutex);

		while(!list_empty(&connect->xi_list)) {
			elem = connect->xi_list.next;
			list_del(elem);
			xi = list_entry(elem, xi_t, connect_pending_list);
			pthread_mutex_unlock(&connect->mutex);
			process_init(xi);
			
			pthread_mutex_lock(&connect->mutex);
		}

		while(!list_empty(&connect->xt_list)) {
			elem = connect->xt_list.next;
			list_del(elem);
			xt = list_entry(elem, xt_t, connect_pending_list);
			pthread_mutex_unlock(&connect->mutex);
			process_tgt(xt);

			pthread_mutex_lock(&connect->mutex);
		}

		pthread_mutex_unlock(&connect->mutex);
	}
		break;

	case RDMA_CM_EVENT_CONNECT_REQUEST:
		process_connect_request(ni, event);
		break;

	case RDMA_CM_EVENT_REJECTED:
		connect->state = GBLN_DISCONNECTED;
		/* todo: destroy QP and reset connect. */
		break;

	default:
		ptl_warn("Got unknown CM event: %d\n", event->event);
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

static int cleanup_ib(ni_t *ni)
{
	if (ni->listen_id) {
		rdma_destroy_id(ni->listen_id);
		ni->listen_id = NULL;
	}

	if (ni->srq) {
		ibv_destroy_srq(ni->srq);
		ni->srq = NULL;
	}

	if (ni->xrc_domain_fd) {
		close(ni->xrc_domain_fd);
		ni->xrc_domain_fd = -1;
	}

	if (ni->xrc_domain) {
		ibv_close_xrc_domain(ni->xrc_domain);
		ni->xrc_domain = NULL;
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

static int init_ib(ni_t *ni)
{
	struct ibv_srq_init_attr srq_init_attr;
	gbl_t *gbl = ni->gbl;
	int err;
	struct sockaddr_in sin;
	int i;
	int flags;

	/* Currently the interface name is ib followed by the interface
	 * number. In the future we may have a system to override that,
	 * for instance, by having a table or environment variable
	 * (PORTALS4_INTERFACE_0=10.2.0.0/16) */
	sprintf(ni->ifname, "ib%d", ni->iface);
	if (if_nametoindex(ni->ifname) == 0) {
		ptl_warn("The interface %s doesn't exist\n", ni->ifname);
		return PTL_FAIL;
	}

	ni->addr = get_ip_address(ni->ifname);
	if (ni->addr == htonl(INADDR_ANY)) {
		ptl_warn("The interface %s doesn't have an IPv4 address\n", ni->ifname);
		return PTL_FAIL;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = ni->addr;

	if (ni->options & PTL_NI_LOGICAL) {
		struct rpc_msg rpc_msg;

		/* Retrieve the IB interface and the XRC domain name. */

		memset(&rpc_msg, 0, sizeof(rpc_msg));
		rpc_msg.type = QUERY_XRC_DOMAIN;
		strcpy(rpc_msg.query_xrc_domain.net_name, ni->ifname);
		err = rpc_get(gbl->rpc->to_server, &rpc_msg, &rpc_msg);
		if (err) {
			ptl_warn("rpc_get(QUERY_XRC_DOMAIN) failed\n");
			return PTL_FAIL;
		}

		if (strlen(rpc_msg.reply_xrc_domain.xrc_domain_fname) == 0) {
			ptl_warn("bad xrc domain fname\n");
			return PTL_FAIL;
		}

		ni->xrc_domain_fd = open(rpc_msg.reply_xrc_domain.xrc_domain_fname, O_RDONLY);
		if (ni->xrc_domain_fd == -1) {
			ptl_warn("unable to open xrc domain file = %s\n", rpc_msg.reply_xrc_domain.xrc_domain_fname);
			return PTL_FAIL;
		}

		sin.sin_port = 0;

	} else {
		ni->id.phys.nid = addr_to_nid(ni->addr);
		sin.sin_port = pid_to_port(ni->id.phys.pid);
	}

	ni->cm_channel = rdma_create_event_channel();
	if (!ni->cm_channel) {
		ptl_warn("unable to create CM event channel\n");
		goto err1;
	}

	/* Create a RDMA CM ID and bind it to retrieve the context and
	 * PD. These will be valid for as long as librdmacm is not
	 * unloaded, ie. when the program exits. */
	if (rdma_create_id(ni->cm_channel, &ni->listen_id, NULL, RDMA_PS_TCP)) {
		ptl_warn("unable to create CM ID\n");
		goto err1;
	}

	if (rdma_bind_addr(ni->listen_id, (struct sockaddr *)&sin)) {
		ptl_warn("unable to bind to local address %x\n", ni->addr);
		goto err1;
	}

	if ((ni->options & PTL_NI_PHYSICAL) &&
		(ni->id.phys.pid == PTL_PID_ANY)) {
		/* No well know PID was given. Retrieve the pid given by
		 * bind. */
		ni->id.phys.pid = ntohs(rdma_get_src_port(ni->listen_id));
	}

	rdma_query_id(ni->listen_id, &ni->ibv_context, &ni->pd);
	if (ni->ibv_context == NULL || ni->pd == NULL) {
		ptl_warn("unable to get the CM ID context or PD\n");
		goto err1;
	}

	/* Create CC, CQ, SRQ. */
	ni->ch = ibv_create_comp_channel(ni->ibv_context);
	if (!ni->ch) {
		ptl_warn("unable to create comp channel\n");
		goto err1;
	}

	flags = fcntl(ni->ch->fd, F_GETFL);
	if (fcntl(ni->ch->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		ptl_warn("Cannot set completion event channel to non blocking\n");
		goto err1;
	}

	ni->cq = ibv_create_cq(ni->ibv_context, MAX_QP_SEND_WR + MAX_QP_RECV_WR,
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

	srq_init_attr.srq_context = ni;
	srq_init_attr.attr.max_wr = 100; /* todo: adjust */
	srq_init_attr.attr.max_sge = 1;
	srq_init_attr.attr.srq_limit = 0; /* should be ignored */

	if (ni->options & PTL_NI_LOGICAL) {
		/* Create XRC SRQ. */

		/* Open XRC domain. */
		ni->xrc_domain = ibv_open_xrc_domain(ni->ibv_context,
											 ni->xrc_domain_fd, O_CREAT);
		if (!ni->xrc_domain) {
			ptl_warn("unable to open xrc domain\n");
			goto err1;
		}

		ni->srq = ibv_create_xrc_srq(ni->pd, ni->xrc_domain,
									 ni->cq, &srq_init_attr);
	} else {
		/* Create regular SRQ. */
		ni->srq = ibv_create_srq(ni->pd, &srq_init_attr);
	}

	if (!ni->srq) {
		ptl_fatal("unable to create srq\n");
		goto err1;
	}

	for (i = 0; i < srq_init_attr.attr.max_wr; i++) {
		err = post_recv(ni);
		if (err) {
			WARN();
			err = PTL_FAIL;
			goto err1;
		}
	}

	/* Create a listening CM ID for physical NI that have a well-known port. */
	if (ni->options & PTL_NI_PHYSICAL) {

		if (debug)
			printf("Listening on local NID/PID\n");
			
		if (rdma_listen(ni->listen_id, 0)) {
			goto err1;
		}

		if (debug) {
			printf("CM listening on %x:%x\n", sin.sin_addr.s_addr, sin.sin_port);
		}

	} else {
		/* Logical NIs don't need one. */
		rdma_destroy_id(ni->listen_id);
		ni->listen_id = NULL;
	}

	return PTL_OK;

 err1:
	cleanup_ib(ni);
	return PTL_FAIL;
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

/* convert ni option flags to a 2 bit type */
static inline int ni_options_to_type(unsigned int options)
{
	return (((options & PTL_NI_MATCHING) ? 1 : 0) << 1) |
		((options & PTL_NI_LOGICAL) ? 1 : 0);
}

int PtlNIInit(ptl_interface_t iface,
			  unsigned int options,
			  ptl_pid_t pid,
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

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		return err;
	}

	if (iface == PTL_IFACE_DEFAULT) {
		iface = get_default_iface(gbl);
		if (iface == PTL_IFACE_DEFAULT) {
			goto err1;
		}
	}

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

	/* TODO: is this test correct ? */
	if ((options & PTL_NI_LOGICAL) && (pid != PTL_PID_ANY)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (options & PTL_NI_LOGICAL) {
		if (unlikely(map_size && desired_mapping &&
					 CHECK_RANGE(desired_mapping, ptl_process_t, map_size))) {
			err = PTL_ARG_INVALID;
			goto err1;
		}

		if (unlikely(map_size &&
					 CHECK_RANGE(actual_mapping, ptl_process_t, map_size))) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	}

	ni_type = ni_options_to_type(options);

	pthread_mutex_lock(&gbl->gbl_mutex);
	ni = gbl_lookup_ni(gbl, iface, ni_type);
	if (ni) {
		(void)__sync_add_and_fetch(&ni->ref_cnt, 1);
		goto done;
	}

	err = ni_alloc(&ni);
	if (unlikely(err))
		goto err2;

	ni->iface = iface;
	ni->ni_type = ni_type;
	ni->ref_cnt = 1;
	ni->options = options;
	ni->last_pt = -1;
	ni->gbl = gbl;
	ni->map = NULL;
	ni->xrc_domain_fd = -1;
	set_limits(ni, desired);
	ni->uid = geteuid();
	INIT_LIST_HEAD(&ni->md_list);
	INIT_LIST_HEAD(&ni->ct_list);
	INIT_LIST_HEAD(&ni->xi_wait_list);
	INIT_LIST_HEAD(&ni->xt_wait_list);
	INIT_LIST_HEAD(&ni->mr_list);
	INIT_LIST_HEAD(&ni->send_list);
	INIT_LIST_HEAD(&ni->recv_list);
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

	ni->pt = calloc(ni->limits.max_pt_index, sizeof(*ni->pt));
	if (unlikely(!ni->pt)) {
		err = PTL_NO_SPACE;
		goto err3;
	}

	if (options & PTL_NI_LOGICAL) {
		ni->id.rank = gbl->rank;

		err = ni_map(gbl, ni, map_size, desired_mapping);
		if (unlikely(err)) {
			goto err3;
		}
	} else {
		ni->id.phys.pid = pid;
	}

	err = init_ib(ni);
	if (err) {
		goto err3;
	}

	/* Add a watcher for CM connections. */
	ev_io_init(&ni->cm_watcher, process_cm_event, ni->cm_channel->fd, EV_READ);
	ni->cm_watcher.data = ni;

	EVL_WATCH(ev_io_start(evl.loop, &ni->cm_watcher));

	/* Add a watcher for CQ events. */
	ev_io_init(&ni->cq_watcher, process_recv, ni->ch->fd, EV_READ);
	ni->cq_watcher.data = ni;
	EVL_WATCH(ev_io_start(evl.loop, &ni->cq_watcher));

	err = gbl_add_ni(gbl, ni);
	if (unlikely(err)) {
		goto err3;
	}

	if (ni->options & PTL_NI_LOGICAL) {
		err = get_rank_table(ni);
		if (err) {
			goto err3;
		}

		err = create_rank_to_nid_table(ni);
		if (err) {
			goto err3;
		}
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
	struct list_head *l, *t;
	mr_t *mr;

        pthread_spin_lock(&ni->mr_list_lock);
	list_for_each_safe(l, t, &ni->mr_list) {
		list_del(l);
		mr = list_entry(l, mr_t, list);
		mr_put(mr);
	}
        pthread_spin_unlock(&ni->mr_list_lock);
}

static void ni_cleanup(ni_t *ni)
{
	interrupt_cts(ni);
	cleanup_mr_list(ni);

	ni_rcqp_stop(ni);

	EVL_WATCH(ev_io_stop(evl.loop, &ni->cq_watcher));

	ni_rcqp_cleanup(ni);

	EVL_WATCH(ev_io_stop(evl.loop, &ni->cm_watcher));

	cleanup_ib(ni);

	release_buffers(ni);

	if (ni->pt) {
		free(ni->pt);
		ni->pt = NULL;
	}

	if (ni->map) {
		free(ni->map);
		ni->map = NULL;
	}

	if (ni->shmem.fd != -1) {
		close(ni->shmem.fd);
		ni->shmem.fd = -1;
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
