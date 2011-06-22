/*
 * ptl_ni.c
 */

#include "ptl_loc.h"

static int compare_nid_pid(const void *a, const void *b)
{
	const entry_t *entry1 = a;
	const entry_t *entry2 = b;

	if (entry1->nid == entry2->nid)
		return(entry1->pid - entry2->pid);
	else
		return(entry1->nid - entry2->nid);
}

static int compare_rank(const void *a, const void *b)
{
	const entry_t *entry1 = a;
	const entry_t *entry2 = b;

	return(entry1->rank - entry2->rank);
}

static void set_limits(ni_t *ni, ptl_ni_limits_t *desired)
{
	if (desired) {
		ni->limits.max_entries =
			chk_param(PTL_LIM_MAX_ENTRIES,
				  desired->max_entries);
		ni->limits.max_unexpected_headers =
			chk_param(PTL_LIM_MAX_UNEXPECTED_HEADERS,
				  desired->max_unexpected_headers);
		ni->limits.max_mds =
			chk_param(PTL_LIM_MAX_MDS, desired->max_mds);
		ni->limits.max_cts =
			chk_param(PTL_LIM_MAX_CTS, desired->max_cts);
		ni->limits.max_eqs =
			chk_param(PTL_LIM_MAX_EQS, desired->max_eqs);
		ni->limits.max_pt_index =
			chk_param(PTL_LIM_MAX_PT_INDEX,
				  desired->max_pt_index);
		ni->limits.max_iovecs =
			chk_param(PTL_LIM_MAX_IOVECS,
				  desired->max_iovecs);
		ni->limits.max_list_size =
			chk_param(PTL_LIM_MAX_LIST_SIZE,
				  desired->max_list_size);
		ni->limits.max_triggered_ops =
			chk_param(PTL_LIM_MAX_TRIGGERED_OPS,
				  desired->max_triggered_ops);
		ni->limits.max_msg_size =
			chk_param(PTL_LIM_MAX_MSG_SIZE,
				  desired->max_msg_size);
		ni->limits.max_atomic_size =
			chk_param(PTL_LIM_MAX_ATOMIC_SIZE,
				  desired->max_atomic_size);
		ni->limits.max_fetch_atomic_size =
			chk_param(PTL_LIM_MAX_FETCH_ATOMIC_SIZE,
				  desired->max_fetch_atomic_size);
		ni->limits.max_waw_ordered_size =
			chk_param(PTL_LIM_MAX_WAW_ORDERED_SIZE,
				  desired->max_waw_ordered_size);
		ni->limits.max_war_ordered_size =
			chk_param(PTL_LIM_MAX_WAR_ORDERED_SIZE,
				  desired->max_war_ordered_size);
		ni->limits.max_volatile_size =
			chk_param(PTL_LIM_MAX_VOLATILE_SIZE,
				  desired->max_volatile_size);
		ni->limits.features =
			chk_param(PTL_LIM_FEATURES,
				  desired->features);
	} else {
		ni->limits.max_entries =
			get_param(PTL_LIM_MAX_ENTRIES);
		ni->limits.max_unexpected_headers =
			get_param(PTL_LIM_MAX_UNEXPECTED_HEADERS);
		ni->limits.max_mds =
			get_param(PTL_LIM_MAX_MDS);
		ni->limits.max_cts =
			get_param(PTL_LIM_MAX_CTS);
		ni->limits.max_eqs =
			get_param(PTL_LIM_MAX_EQS);
		ni->limits.max_pt_index =
			get_param(PTL_LIM_MAX_PT_INDEX);
		ni->limits.max_iovecs =
			get_param(PTL_LIM_MAX_IOVECS);
		ni->limits.max_list_size =
			get_param(PTL_LIM_MAX_LIST_SIZE);
		ni->limits.max_triggered_ops =
			get_param(PTL_LIM_MAX_TRIGGERED_OPS);
		ni->limits.max_msg_size =
			get_param(PTL_LIM_MAX_MSG_SIZE);
		ni->limits.max_atomic_size =
			get_param(PTL_LIM_MAX_ATOMIC_SIZE);
		ni->limits.max_fetch_atomic_size =
			get_param(PTL_LIM_MAX_FETCH_ATOMIC_SIZE);
		ni->limits.max_waw_ordered_size =
			get_param(PTL_LIM_MAX_WAW_ORDERED_SIZE);
		ni->limits.max_war_ordered_size =
			get_param(PTL_LIM_MAX_WAR_ORDERED_SIZE);
		ni->limits.max_volatile_size =
			get_param(PTL_LIM_MAX_VOLATILE_SIZE);
		ni->limits.features =
			get_param(PTL_LIM_FEATURES);
	}
}

static void ni_rcqp_stop(ni_t *ni)
{
	int i;
	const int map_size = ni->logical.map_size;

	if (ni->options & PTL_NI_LOGICAL) {
		for (i = 0; i < map_size; i++) {
			conn_t *connect = &ni->logical.rank_table[i].connect;
		
			pthread_mutex_lock(&connect->mutex);
			if (connect->state != CONN_STATE_DISCONNECTED
#ifdef USE_XRC
				&& connect->state != CONN_STATE_XRC_CONNECTED
#endif
				) {
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

		switch (buf->type) {
		case BUF_SEND:
		case BUF_RDMA:
			pthread_spin_lock(&ni->send_list_lock);
			list_del(&buf->list);
			pthread_spin_unlock(&ni->send_list_lock);
			break;
		case BUF_RECV:
			pthread_spin_lock(&ni->recv_list_lock);
			list_del(&buf->list);
			pthread_spin_unlock(&ni->recv_list_lock);
			break;
		}

		buf_put(buf);
	}

	return PTL_OK;
}

static int init_ib_srq(ni_t *ni)
{
	struct ibv_srq_init_attr srq_init_attr;
	iface_t *iface = ni->iface;
	int err;
	int i;

	srq_init_attr.srq_context = ni;
	srq_init_attr.attr.max_wr = get_param(PTL_MAX_SRQ_RECV_WR);
	srq_init_attr.attr.max_sge = 1;
	srq_init_attr.attr.srq_limit = 0; /* should be ignored */

#ifdef USE_XRC
	if (ni->options & PTL_NI_LOGICAL) {
		/* Create XRC SRQ. */
		ni->srq = ibv_create_xrc_srq(iface->pd, ni->logical.xrc_domain,
					     ni->cq, &srq_init_attr);
	} else
#endif
	{
		/* Create regular SRQ. */
		ni->srq = ibv_create_srq(iface->pd, &srq_init_attr);
	}

	if (!ni->srq) {
		WARN();
		return PTL_FAIL;
	}

	for (i = 0; i < srq_init_attr.attr.max_wr; i++) {
		err = ptl_post_recv(ni);
		if (err) {
			WARN();
			return PTL_FAIL;
		}
	}

	return PTL_OK;
}

/* Must be locked by gbl_mutex. port is in network order. */
static int bind_iface(iface_t *iface, unsigned int port)
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

#ifdef USE_XRC
	rdma_query_id(iface->listen_id, &iface->ibv_context, &iface->pd);
#else
	iface->ibv_context = iface->listen_id->verbs;

	iface->pd = ibv_alloc_pd(iface->ibv_context);
#endif

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

static int cleanup_ib(ni_t *ni)
{
	if (ni->srq) {
		ibv_destroy_srq(ni->srq);
		ni->srq = NULL;
	}

#ifdef USE_XRC
	if (ni->logical.xrc_domain_fd != -1) {
		close(ni->logical.xrc_domain_fd);
		ni->logical.xrc_domain_fd = -1;
	}

	if (ni->logical.xrc_domain) {
		ibv_close_xrc_domain(ni->logical.xrc_domain);
		ni->logical.xrc_domain = NULL;
	}
#endif

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

/* Must be locked by gbl_mutex. */
static int init_ib(iface_t *iface, ni_t *ni)
{
	int err;
	int flags;
	int cqe;

	/* If it is a physical address, then we bind it. */
	if (ni->options & PTL_NI_PHYSICAL) {

		ni->id.phys.nid = addr_to_nid(&iface->sin);

		if (iface->id.phys.nid == PTL_NID_ANY) {
			iface->id.phys.nid = ni->id.phys.nid;
		} else if (iface->id.phys.nid != ni->id.phys.nid) {
			WARN();
			goto err1;
		}

		if (debug)
			printf("setting ni->id.phys.nid = %x\n", ni->id.phys.nid);

		err = bind_iface(iface, pid_to_port(ni->id.phys.pid));
		if (err) {
			ptl_warn("Binding failed\n");
			WARN();
			goto err1;
		}
	}

	if ((ni->options & PTL_NI_PHYSICAL) &&
		(ni->id.phys.pid == PTL_PID_ANY)) {
		/* No well know PID was given. Retrieve the pid given by
		 * bind. */
		ni->id.phys.pid = port_to_pid(rdma_get_src_port(iface->listen_id));

		/* remember the physical pid in case application creates another NI */
		iface->id.phys.pid = ni->id.phys.pid;

		if (debug)
			printf("set iface pid(1) = %x\n", iface->id.phys.pid);
	}

	/* Create CC, CQ, SRQ. */
	ni->ch = ibv_create_comp_channel(iface->ibv_context);
	if (!ni->ch) {
		ptl_warn("unable to create comp channel\n");
		WARN();
		goto err1;
	}

	flags = fcntl(ni->ch->fd, F_GETFL);
	if (fcntl(ni->ch->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		ptl_warn("Cannot set completion event channel to non blocking\n");
		WARN();
		goto err1;
	}

	cqe = get_param(PTL_MAX_QP_SEND_WR) + get_param(PTL_MAX_RDMA_WR_OUT) +
	      get_param(PTL_MAX_QP_RECV_WR) + 10;

	ni->cq = ibv_create_cq(iface->ibv_context, cqe, ni, ni->ch, 0);
	if (!ni->cq) {
		WARN();
		ptl_warn("unable to create cq\n");
		WARN();
		goto err1;
	}

	err = ibv_req_notify_cq(ni->cq, 0);
	if (err) {
		ptl_warn("unable to req notify\n");
		WARN();
		goto err1;
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

/*
 * init_pools - initialize resource pools for NI
 */
static int init_pools(ni_t *ni)
{
	int err;

	ni->mr_pool.free = mr_release;

	err = pool_init(&ni->mr_pool, "mr", sizeof(mr_t),
					POOL_MR, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->md_pool.free = md_release;

	err = pool_init(&ni->md_pool, "md", sizeof(md_t),
					POOL_MD, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	if (ni->options & PTL_NI_MATCHING) {
		ni->me_pool.init = me_init;
		ni->me_pool.free = me_release;

		err = pool_init(&ni->me_pool, "me", sizeof(me_t),
						POOL_ME, (obj_t *)ni);
		if (err) {
			WARN();
			return err;
		}
	} else {
		ni->le_pool.free = le_release;
		ni->le_pool.init = le_init;

		err = pool_init(&ni->le_pool, "le", sizeof(le_t),
						POOL_LE, (obj_t *)ni);
		if (err) {
			WARN();
			return err;
		}
	}

	ni->eq_pool.free = eq_release;

	err = pool_init(&ni->eq_pool, "eq", sizeof(eq_t),
					POOL_EQ, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->ct_pool.free = ct_release;

	err = pool_init(&ni->ct_pool, "ct", sizeof(ct_t),
					POOL_CT, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->xi_pool.alloc = xi_new;

	err = pool_init(&ni->xi_pool, "xi", sizeof(xi_t),
					POOL_XI, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->xt_pool.alloc = xt_new;

	err = pool_init(&ni->xt_pool, "xt", sizeof(xt_t),
					POOL_XT, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	ni->buf_pool.init = buf_init;
	ni->buf_pool.fini = buf_release;
	ni->buf_pool.segment_size = 128*1024;

	err = pool_init(&ni->buf_pool, "buf", sizeof(buf_t),
					POOL_BUF, (obj_t *)ni);
	if (err) {
		WARN();
		return err;
	}

	return PTL_OK;
}

/*
 * create_tables
 *	initialize private rank table in NI
 *	we are not yet connected to remote QPs
 */
static int create_tables(ni_t *ni, ptl_size_t map_size,
			 ptl_process_t *actual_mapping)
{
	int i;
	ptl_nid_t curr_nid;
	int main_rank;	/* rank of lowest pid in each nid */
	entry_t *entry;
	conn_t *conn;

	ni->logical.rank_table = calloc(map_size, sizeof(entry_t));
	if (!ni->logical.rank_table) {
		WARN();
		return PTL_NO_SPACE;
	}

	ni->logical.map_size = map_size;

	for (i = 0; i < map_size; i++) {
		entry = &ni->logical.rank_table[i];
		conn = &entry->connect;

		entry->rank = i;
		entry->nid = actual_mapping[i].phys.nid;
		entry->pid = actual_mapping[i].phys.pid;

		conn_init(ni, conn);

		/* convert nid/pid to ipv4 address */
		conn->sin.sin_family = AF_INET;
		conn->sin.sin_addr.s_addr = nid_to_addr(entry->nid);
		conn->sin.sin_port = pid_to_port(entry->pid);
	}

	/* temporarily sort the rank table by NID/PID */
	qsort(ni->logical.rank_table, map_size,
	      sizeof(entry_t), compare_nid_pid);

	/* anything that doesn't match first nid */
	curr_nid = ni->logical.rank_table[0].nid + 1;
	main_rank = -1;

	for (i = 0; i < map_size; i++) {
		entry = &ni->logical.rank_table[i];

		if (entry->nid != curr_nid) {
			/* start new NID. */
			curr_nid = entry->nid;
			main_rank = entry->rank;

#ifdef USE_XRC
			if (ni->id.rank == main_rank)
				ni->logical.is_main = 1;
#endif
		}

		entry->main_rank = main_rank;
	}

	/* Sort back the rank table by rank. */
	qsort(ni->logical.rank_table, map_size,
	      sizeof(entry_t), compare_rank);

	return PTL_OK;
}

#ifdef USE_XRC
/* If this rank is the main one on the NID, create the domain, else
 * attach to an existing one. */
static int get_xrc_domain(ni_t *ni)
{
	entry_t *entry;
	entry_t *main_entry;
	char domain_fname[100];

	/* Create filename for our domain. */
	entry = &ni->logical.rank_table[ni->id.rank];
	main_entry = &ni->logical.rank_table[entry->main_rank];
	sprintf(domain_fname, "/tmp/p4-xrc-%u-%u-%u", 
			ni->gbl->jid, main_entry->nid, main_entry->pid);

	if (ni->logical.is_main) {
		ni->logical.xrc_domain_fd = open(domain_fname, O_RDWR|O_CREAT,
						 S_IRUSR|S_IWUSR);
		if (ni->logical.xrc_domain_fd < 0) {
			WARN();
			return PTL_FAIL;
		}

		/* Create XRC domain. */
		ni->logical.xrc_domain = ibv_open_xrc_domain(ni->iface->ibv_context,
							     ni->logical.xrc_domain_fd, O_CREAT);
		if (!ni->logical.xrc_domain) {
			close(ni->logical.xrc_domain_fd);
			WARN();
			return PTL_FAIL;
		}
	} else {
		int try;
		
		/* Open domain file. Try for 10 seconds. */
		try = 50;
		do {
			ni->logical.xrc_domain_fd = open(domain_fname,
							 O_RDWR, S_IRUSR | S_IWUSR);
			if (ni->logical.xrc_domain_fd >= 0)
				break;
			try --;
			usleep(200000);
		} while(try && ni->logical.xrc_domain_fd < 0);

		if (ni->logical.xrc_domain_fd < 0) {
			WARN();
			return PTL_FAIL;
		}

		/* Open XRC domain. Try for 10 seconds. */
		try = 10;
		do {
			ni->logical.xrc_domain = ibv_open_xrc_domain(ni->iface->ibv_context,
								     ni->logical.xrc_domain_fd, 0);
		} while(try && !ni->logical.xrc_domain);

		if (!ni->logical.xrc_domain) {
			WARN();
			return PTL_FAIL;
		}
	}

	return PTL_OK;
}
#endif

/*
 * init_mapping
 */
static int init_mapping(ni_t *ni, iface_t *iface, ptl_size_t map_size,
			   ptl_process_t *desired_mapping,
			   ptl_process_t *actual_mapping)
{
	int i;
	int err;

	if (!iface->actual_mapping) {
		/* Don't have one yet. Allocate and fill-up now. */
		const int size = map_size * sizeof(ptl_process_t);

		iface->map_size = map_size;
		iface->actual_mapping = malloc(size);
		if (!iface->actual_mapping) {
			WARN();
			return PTL_NO_SPACE;
		}

		memcpy(iface->actual_mapping, desired_mapping, size);
	}

	/* lookup our nid/pid to determine rank */
	ni->id.rank = PTL_RANK_ANY;

	for (i = 0; i < iface->map_size; i++) {
		if ((iface->actual_mapping[i].phys.nid == iface->id.phys.nid) &&
		    (iface->actual_mapping[i].phys.pid == iface->id.phys.pid)) {
			ni->id.rank = i;
			ptl_test_rank = i;
		}
	}

	if (ni->id.rank == PTL_RANK_ANY) {
		WARN();
		return PTL_ARG_INVALID;
	}

	/* return mapping to caller. */
	if (actual_mapping)
		memcpy(actual_mapping, iface->actual_mapping,
		       map_size*sizeof(ptl_process_t));

	err = create_tables(ni, iface->map_size, iface->actual_mapping);
	if (err) {
		WARN();
		return err;
	}

#ifdef USE_XRC
	/* Retrieve the XRC domain name. */
	err = get_xrc_domain(ni);
	if (err) {
		WARN();
		return err;
	}
#endif

	return PTL_OK;
}

int PtlNIInit(ptl_interface_t iface_id,
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
	iface_t *iface;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	iface = get_iface(gbl, iface_id);
	if (unlikely(!iface)) {
		err = PTL_NO_INIT;
		WARN();
		goto err1;
	}

	if (unlikely(options & ~PTL_NI_INIT_OPTIONS_MASK)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!!(options & PTL_NI_MATCHING)
				 ^ !(options & PTL_NI_NO_MATCHING))) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!!(options & PTL_NI_LOGICAL)
				 ^ !(options & PTL_NI_PHYSICAL))) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (options & PTL_NI_LOGICAL && !iface->actual_mapping) {
		if (!desired_mapping || map_size == 0) {
			WARN();
			err = PTL_ARG_INVALID;
			goto err1;
		}
	}

	ni_type = ni_options_to_type(options);

	pthread_mutex_lock(&gbl->gbl_mutex);

	/* check to see if ni of type ni_type already exists */
	ni = iface_get_ni(iface, ni_type);
	if (ni)
		goto done;

	/* check to see if iface is configured and waiting for cm events
	   if not, initialize it */
	err = init_iface(iface);
	if (err) {
		WARN();
		goto err2;
	}

	err = ni_alloc(&gbl->ni_pool, &ni);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	OBJ_NEW(ni);

	if (options & PTL_NI_PHYSICAL) {
		ni->id.phys.nid = PTL_NID_ANY;
		ni->id.phys.pid = pid;

		if (pid == PTL_PID_ANY && iface->id.phys.pid != PTL_PID_ANY) {
			ni->id.phys.pid = iface->id.phys.pid;
		} else if (iface->id.phys.pid == PTL_PID_ANY && pid != PTL_PID_ANY) {
			iface->id.phys.pid = pid;

			if (debug)
				printf("set iface pid(2) = %x\n", iface->id.phys.pid);
		} else if (pid != iface->id.phys.pid) {
			WARN();
			err = PTL_ARG_INVALID;
			goto err3;
		}
	} else {
		/* currently we must always create a physical NI first
		 * to establish the PID */
		if (iface->id.phys.pid == PTL_PID_ANY) {
			ptl_warn("no PID established before creating logical NI\n");
			WARN();
			err = PTL_ARG_INVALID;
			goto err3;
		}
	}

	ni->iface = iface;
	ni->ni_type = ni_type;
	ni->ref_cnt = 1;
	ni->options = options;
	ni->last_pt = -1;
	ni->gbl = gbl;
#ifdef USE_XRC
	ni->logical.xrc_domain_fd = -1;
#endif
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
	pthread_spin_init(&ni->physical.lock, PTHREAD_PROCESS_PRIVATE);
	pthread_mutex_init(&ni->logical.lock, NULL);

	ni->pt = calloc(ni->limits.max_pt_index, sizeof(*ni->pt));
	if (unlikely(!ni->pt)) {
		WARN();
		err = PTL_NO_SPACE;
		goto err3;
	}

	err = init_pools(ni);
	if (unlikely(err))
		goto err3;

	err = init_ib(iface, ni);
	if (unlikely(err))
		goto err3;

	if (options & PTL_NI_LOGICAL) {
		err = init_mapping(ni, iface, map_size,
				   desired_mapping, actual_mapping);
		if (unlikely(err))
			goto err3;
	}

	/* Create shared receive queue */
	err = init_ib_srq(ni);
	if (unlikely(err))
		goto err3;

	/* Add a watcher for CQ events. */
	ev_io_init(&ni->cq_watcher, process_recv, ni->ch->fd, EV_READ);
	ni->cq_watcher.data = ni;
	EVL_WATCH(ev_io_start(evl.loop, &ni->cq_watcher));

	/* Ready to listen. */
	if ((ni->options & PTL_NI_PHYSICAL) && !iface->listen) {
		if (rdma_listen(iface->listen_id, 0)) {
			ptl_warn("Failed to listen\n");
			WARN();
			goto err1;
		}

		iface->listen = 1;
	}

	err = iface_add_ni(iface, ni);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

 done:
	pthread_mutex_unlock(&gbl->gbl_mutex);

	if (actual)
		*actual = ni->limits;

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
		pthread_cond_broadcast(&ct->cond);
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

	pool_fini(&ni->buf_pool);
	pool_fini(&ni->xt_pool);
	pool_fini(&ni->xi_pool);
	pool_fini(&ni->ct_pool);
	pool_fini(&ni->eq_pool);
	pool_fini(&ni->le_pool);
	pool_fini(&ni->me_pool);
	pool_fini(&ni->md_pool);
	pool_fini(&ni->mr_pool);

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
	pthread_spin_destroy(&ni->recv_list_lock);
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
		err = iface_remove_ni(ni);
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

	if (unlikely(index >= PTL_SR_LAST)) {
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

	err = obj_get(0, handle, &obj);
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