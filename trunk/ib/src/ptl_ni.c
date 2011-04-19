/*
 * ptl_ni.c
 */

#include "ptl_loc.h"

#include <sys/ioctl.h>

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

	for (i = 0; i < map_size; i++) {
		struct rank_entry *entry = &ni->logical.rank_table[i];
		conn_t *conn = &entry->connect;

		entry->rank = i;
		entry->nid = actual_mapping[i].phys.nid;
		entry->pid = actual_mapping[i].phys.pid;

		conn_init(ni, conn);

		/* Get the IP address from the rank table for that NID. */
		conn->sin.sin_family = AF_INET;
		conn->sin.sin_addr.s_addr = nid_to_addr(entry->nid);
		conn->sin.sin_port = pid_to_port(entry->pid);
	}

	/* Sort the rank table by NID to find the unique nids. Be careful
	 * here because some pointers won't be valid anymore, until the
	 * table is sorted back by ranks. */
	qsort(ni->logical.rank_table, map_size, sizeof(struct rank_entry), compare_nid_pid);

	prev_nid = ni->logical.rank_table[0].nid + 1;
	main_rank = -1;

	for (i = 0; i < map_size; i++) {
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

/* TODO finish this */
static ptl_ni_limits_t default_ni_limits = {
	.max_entries		= 123,
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

static void ni_rcqp_stop(ni_t *ni)
{
	int i;
	const int map_size = ni->logical.map_size;

	if (ni->options & PTL_NI_LOGICAL) {
		for (i = 0; i < map_size; i++) {
			conn_t *connect = &ni->logical.rank_table[i].connect;
		
			pthread_mutex_lock(&connect->mutex);
			if (connect->state != CONN_STATE_DISCONNECTED &&
			    connect->state != CONN_STATE_XRC_CONNECTED) {
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

	ni->cq = ibv_create_cq(iface->ibv_context, MAX_QP_SEND_WR + MAX_RDMA_WR_OUT + MAX_QP_RECV_WR + 10,
						   ni, ni->ch, 0);
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
	struct iface *iface;
	int i;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	if (ifacenum == PTL_IFACE_DEFAULT) {
		ifacenum = get_default_iface(gbl);
		if (ifacenum == PTL_IFACE_DEFAULT) {
			WARN();
			err = PTL_ARG_INVALID;
			goto err1;
		}
	}
	iface = &gbl->iface[ifacenum];

	if (unlikely(options & ~PTL_NI_INIT_OPTIONS)) {
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

	ni = gbl_lookup_ni(gbl, ifacenum, ni_type);
	if (ni) {
		(void)__sync_add_and_fetch(&ni->ref_cnt, 1);
		goto done;
	}

	err = init_iface(iface, ifacenum);
	if (err) {
		WARN();
		goto err2;
	}

	err = ni_alloc(&gbl->ni_pool, &ni);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

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
	pthread_spin_init(&ni->physical.lock, PTHREAD_PROCESS_PRIVATE);
	pthread_mutex_init(&ni->logical.lock, NULL);

	ni->pt = calloc(ni->limits.max_pt_index, sizeof(*ni->pt));
	if (unlikely(!ni->pt)) {
		WARN();
		err = PTL_NO_SPACE;
		goto err3;
	}

	ni->mr_pool.free = mr_release;

	err = pool_init(&ni->mr_pool, "mr", sizeof(mr_t),
			POOL_MR, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	ni->md_pool.free = md_release;

	err = pool_init(&ni->md_pool, "md", sizeof(md_t),
			POOL_MD, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	ni->me_pool.free = me_release;

	err = pool_init(&ni->me_pool, "me", sizeof(me_t),
			POOL_ME, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	ni->le_pool.free = le_release;

	err = pool_init(&ni->le_pool, "le", sizeof(le_t),
			POOL_LE, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	ni->eq_pool.free = eq_release;

	err = pool_init(&ni->eq_pool, "eq", sizeof(eq_t),
			POOL_EQ, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	ni->ct_pool.free = ct_release;

	err = pool_init(&ni->ct_pool, "ct", sizeof(ct_t),
			POOL_CT, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	ni->xi_pool.init = xi_init;
	ni->xi_pool.fini = xi_fini;
	ni->xi_pool.alloc = xi_new;
	ni->xi_pool.max_count = 50;	// TODO make this a tunable parameter
	ni->xi_pool.min_count = 25;	// TODO make this a tunable parameter

	err = pool_init(&ni->xi_pool, "xi", sizeof(xi_t),
			POOL_XI, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	ni->xt_pool.init = xt_init;
	ni->xt_pool.fini = xt_fini;
	ni->xt_pool.alloc = xt_new;

	err = pool_init(&ni->xt_pool, "xt", sizeof(xt_t),
			POOL_XT, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	ni->buf_pool.init = buf_init;
	ni->buf_pool.fini = buf_release;
	ni->buf_pool.segment_size = 128*1024;

	err = pool_init(&ni->buf_pool, "buf", sizeof(buf_t),
			POOL_BUF, (obj_t *)ni);
	if (err) {
		WARN();
		goto err3;
	}

	err = init_ib(iface, ni);
	if (err) {
		WARN();
		goto err3;
	}

	/* Create the rank table. */
	if (options & PTL_NI_LOGICAL) {
		if (!iface->actual_mapping) {
			/* Don't have one yet. Allocate and fill-up now. */
			const int size = map_size * sizeof(ptl_process_t);

			iface->map_size = map_size;
			iface->actual_mapping = malloc(size);
			if (!iface->actual_mapping) {
				WARN();
				err = PTL_NO_SPACE;
				goto err3;
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
			ptl_warn("mapping does not contain NID/PID\n");
			WARN();
			err = PTL_ARG_INVALID;
			goto err3;
		}

		if (debug)
			printf("found rank = %d\n", ni->id.rank);

		/* return mapping to caller. */
		if (actual_mapping)
			memcpy(actual_mapping, iface->actual_mapping,
			       map_size*sizeof(ptl_process_t));

		err = create_tables(ni, iface->map_size, iface->actual_mapping);
		if (err) {
			WARN();
			goto err3;
		}

		/* Retrieve the XRC domain name. */
		err = get_xrc_domain(ni);
		if (err) {
			WARN();
			goto err3;
		}
	}

	/* Create own SRQ. */
	err = init_ib_srq(ni);
	if (err) {
		WARN();
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
			WARN();
			goto err1;
		}

		iface->listen = 1;
		
		if (debug) {
			printf("CM listening on %x:%d\n", iface->sin.sin_addr.s_addr, iface->sin.sin_port);
		}
	}

	err = gbl_add_ni(gbl, ni);
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
