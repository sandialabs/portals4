/*
 * ptl_ni.c
 */

#include "ptl_loc.h"

unsigned short ptl_ni_port(ni_t *ni)
{
	return PTL_NI_PORT + ni->ni_type;
}

static int get_default_iface(gbl_t *gbl)
{
	int iface;
	char file_name[64];
	FILE *fd;
	char if_name[16];
	int n;

	pthread_mutex_lock(&gbl->gbl_mutex);

	iface = if_nametoindex("ib0");
	if (iface) {
		goto done;
	}

	snprintf(file_name, sizeof(file_name), "/sys/class/infiniband/rxe0/parent");
	fd = fopen(file_name, "r");
	if (!fd) {
		pthread_mutex_unlock(&gbl->gbl_mutex);
		return 0;
	}

	n = fread(if_name, 1, 16, fd);
	fclose(fd);
	if (n <= 0) {
		pthread_mutex_unlock(&gbl->gbl_mutex);
		return 0;
	}

	if_name[n-1] = 0;
	iface = if_nametoindex(if_name);

done:
	pthread_mutex_unlock(&gbl->gbl_mutex);
	return iface;
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

static int ni_rcqp_stop(ni_t *ni)
{
	int err;
	struct ibv_qp_attr attr;
	int mask;

	attr.qp_state			= IBV_QPS_ERR;
	mask				= IBV_QP_STATE;

	err = ibv_modify_qp(ni->qp, &attr, mask);
	if (err) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

static int ni_rcqp_cleanup(ni_t *ni)
{
	int err;
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

	err = ibv_destroy_qp(ni->qp);
	if (err) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

int ni_rcqp_init(ni_t *ni)
{
	int err;
	struct ibv_qp_init_attr init;
	struct ibv_qp_attr attr;
	int mask;
	int i;

	init.qp_context			= ni;
	init.send_cq			= ni->cq;
	init.recv_cq			= ni->cq;
	init.srq			= NULL;
	/* Temporary values for wr entries, will use SRQ eventually */
	init.cap.max_send_wr		= MAX_QP_SEND_WR * MAX_RDMA_WR_OUT;
	init.cap.max_recv_wr		= MAX_QP_RECV_WR;
	init.cap.max_send_sge		= MAX_INLINE_SGE;
	init.cap.max_recv_sge		= 10;
	init.cap.max_inline_data	= 0;
	init.qp_type			= IBV_QPT_RC;
	init.sq_sig_all			= 0;
	init.xrc_domain			= NULL;

	ni->qp = ibv_create_qp(ni->pd, &init);
	if (!ni->qp) {
		WARN();
		err = PTL_FAIL;
		goto err1;
	}

	attr.qp_state			= IBV_QPS_INIT;
	attr.qp_access_flags		= IBV_ACCESS_REMOTE_WRITE
					| IBV_ACCESS_REMOTE_READ
					| IBV_ACCESS_REMOTE_ATOMIC
					;
	attr.pkey_index			= 0;
	attr.port_num			= 1;

	mask				= IBV_QP_STATE
					| IBV_QP_ACCESS_FLAGS
					| IBV_QP_PKEY_INDEX
					| IBV_QP_PORT
					;

	err = ibv_modify_qp(ni->qp, &attr, mask);
	if (err) {
		WARN();
		err = PTL_FAIL;
		goto err1;
	}

	for (i = 0; i < 10; i++) {
		err = post_recv(ni);
		if (err) {
			WARN();
			err = PTL_FAIL;
			goto err1;
		}
	}

	attr.qp_state			= IBV_QPS_RTR;
	attr.path_mtu			= IBV_MTU_2048;
	attr.rq_psn			= 1;
	attr.dest_qp_num		= ni->qp->qp_num;

	/* loop back to ourselves */
	ibv_query_gid(ni->ibv_context, 1, 0, &attr.ah_attr.grh.dgid);

	attr.ah_attr.grh.flow_label	= 0;
	attr.ah_attr.grh.sgid_index	= 0;
	attr.ah_attr.grh.hop_limit	= 1;
	attr.ah_attr.grh.traffic_class	= 0;
	attr.ah_attr.dlid		= 0;
	attr.ah_attr.sl			= 0;
	attr.ah_attr.src_path_bits	= 0;
	attr.ah_attr.static_rate	= IBV_RATE_10_GBPS;
	attr.ah_attr.is_global		= 1;
	attr.ah_attr.port_num		= 1;
	attr.max_dest_rd_atomic		= 4;
	attr.min_rnr_timer		= 3;

	mask				= IBV_QP_STATE
					| IBV_QP_AV
					| IBV_QP_PATH_MTU
					| IBV_QP_DEST_QPN
					| IBV_QP_RQ_PSN
					| IBV_QP_MAX_DEST_RD_ATOMIC
					| IBV_QP_MIN_RNR_TIMER
					;

	err = ibv_modify_qp(ni->qp, &attr, mask);
	if (err) {
		WARN();
		err = PTL_FAIL;
		goto err1;
	}

	attr.qp_state			= IBV_QPS_RTS;
	attr.sq_psn			= 1;
	attr.max_rd_atomic		= 4;
	attr.timeout			= 12;	/* 4usec * 2^n */
	attr.retry_cnt			= 3;
	attr.rnr_retry			= 3;

	mask				= IBV_QP_STATE
					| IBV_QP_SQ_PSN
					| IBV_QP_MAX_QP_RD_ATOMIC
					| IBV_QP_RETRY_CNT
					| IBV_QP_RNR_RETRY
					| IBV_QP_TIMEOUT
					;

	err = ibv_modify_qp(ni->qp, &attr, mask);
	if (err) {
		WARN();
		err = PTL_FAIL;
		goto err1;
	}

	ni->recv_run = 1;
	err = pthread_create(&ni->recv_thread, NULL, recv_thread, ni);
	if (err) {
		WARN();
		err = PTL_FAIL;
		goto err1;
	}
	ni->has_recv_thread = 1;

	return PTL_OK;

err1:
	return err;
}

static int cleanup_ib(ni_t *ni)
{
	if (ni->xrc_srq) {
		ibv_destroy_srq(ni->xrc_srq);
		ni->xrc_srq = NULL;
	}

	if (ni->xrc_domain_fd) {
		close(ni->xrc_domain_fd);
		ni->xrc_domain_fd = 0;
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

	if (ni->pd) {
		ibv_dealloc_pd(ni->pd);
		ni->pd = NULL;
	}

	if (ni->ibv_context) {
		ibv_close_device(ni->ibv_context);
		ni->ibv_context = NULL;
	}

	return PTL_OK;
}

static int init_ib(ni_t *ni)
{
	char ifname[IF_NAMESIZE];
	char *str;
	struct ibv_device **dev_list;
	struct ibv_device **dev;
	struct ibv_srq_init_attr srq_init_attr;
	gbl_t *gbl = ni->gbl;
	struct rpc_msg rpc_msg;
	int err;

	/* Retrieve the interface name */
	str = if_indextoname(ni->iface, ifname);
	if (!str) {
		ptl_warn("unable to get ifname from ifindex\n");
		return PTL_FAIL;
	}

	/* Retrieve the IB interface and the XRC domain name. */
	memset(&rpc_msg, 0, sizeof(rpc_msg));
	rpc_msg.type = QUERY_XRC_DOMAIN;
	strcpy(rpc_msg.query_xrc_domain.net_name, str);
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

	/* Find the interface name in the IB device list and open it. */
	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		ptl_warn("unable to get rdma device list\n");
		return PTL_FAIL;
	}

	dev = dev_list;
	while(*dev) {
		if (!strcmp(ibv_get_device_name(*dev), rpc_msg.reply_xrc_domain.ib_name))
			break;
		dev ++;
	}

	if (!*dev) {
		ptl_warn("unable to find rdma device for rxe domain\n");
		ibv_free_device_list(dev_list);
		goto err1;
	}

	ni->ibv_context = ibv_open_device(*dev);

	ibv_free_device_list(dev_list);

	if (!ni->ibv_context) {
		ptl_warn("unable to open rdma device\n");
		goto err1;
	}

	/* Create PD, CC, CQ, SRQ. */
	ni->pd = ibv_alloc_pd(ni->ibv_context);
	if (!ni->pd) {
		ptl_warn("unable to alloc pd\n");
		goto err1;
	}

	ni->ch = ibv_create_comp_channel(ni->ibv_context);
	if (!ni->ch) {
		ptl_warn("unable to create comp channel\n");
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

	/* Create XRC QP. */

	/* Open XRC domain. */
	ni->xrc_domain = ibv_open_xrc_domain(ni->ibv_context,
		ni->xrc_domain_fd, O_CREAT);
	if (!ni->xrc_domain) {
		ptl_warn("unable to open xrc domain\n");
		goto err1;
	}

	srq_init_attr.srq_context = ni;
	srq_init_attr.attr.max_wr = 1;
	srq_init_attr.attr.max_sge = 1;
	srq_init_attr.attr.srq_limit = 30; /* should be ignored */
	
	ni->xrc_srq = ibv_create_xrc_srq(ni->pd, ni->xrc_domain,
					 ni->cq, &srq_init_attr);
	if (!ni->xrc_srq) {
		ptl_fatal("unable to create xrc srq\n");
		goto err1;
	}

	return PTL_OK;

 err1:
	cleanup_ib(ni);
	return PTL_FAIL;
}

/* convert ni option flags to a 2 bit type */
static inline int ni_options_to_type(unsigned int options)
{
	return (((options & PTL_NI_MATCHING) != 0) << 1) |
	       ((options & PTL_NI_LOGICAL) != 0);
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
		if (!iface) {
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

	/* For now, only accept PTL_PID_ANY. */
	if (pid != PTL_PID_ANY) {
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
		ni->ref_cnt++;
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
	pthread_spin_init(&ni->md_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->ct_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->xi_wait_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->xt_wait_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->mr_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&ni->send_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_mutex_init(&ni->pt_mutex, NULL);
	pthread_mutex_init(&ni->eq_wait_mutex, NULL);
	pthread_cond_init(&ni->eq_wait_cond, NULL);

	ni->pt = calloc(ni->limits.max_pt_index, sizeof(*ni->pt));
	if (unlikely(!ni->pt)) {
		err = PTL_NO_SPACE;
		goto err3;
	}

	if (options & PTL_NI_LOGICAL) {
		err = ni_map(gbl, ni, map_size, desired_mapping);
		if (unlikely(err)) {
			goto err3;
		}
	}

	err = init_ib(ni);
	if (err) {
		goto err3;
	}

	err = ni_rcqp_init(ni);
	if (err) {
		goto err3;
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
	list_for_each(l, &ni->ct_list) {
		ct = list_entry(l, ct_t, list);
		if (ct->waiting) {
			ct->interrupt = 1;
			pthread_mutex_lock(&ct->mutex);
			pthread_cond_broadcast(&ct->cond);
			pthread_mutex_unlock(&ct->mutex);
		}
	}
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

void ni_cleanup(ni_t *ni)
{
	void *notused;

	interrupt_cts(ni);
	cleanup_mr_list(ni);

	ni->recv_run = 0;

	ni_rcqp_stop(ni);

	if (ni->has_recv_thread) {
		pthread_join(ni->recv_thread, &notused);
		ni->has_recv_thread = 0;
	}

	ni_rcqp_cleanup(ni);

	cleanup_ib(ni);

	if (ni->pt) {
		free(ni->pt);
		ni->pt = NULL;
	}

	if (ni->map) {
		free(ni->map);
		ni->map = NULL;
	}

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
	if (--ni->ref_cnt <= 0) {
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
