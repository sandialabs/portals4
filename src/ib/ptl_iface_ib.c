/**
 * @file ptl_iface.c
 *
 * @brief
 */
#include "ptl_loc.h"

#define max(a,b)	(((a) > (b)) ? (a) : (b))
#define min(a,b)	(((a) > (b)) ? (b) : (a))

/**
 * @brief Query the RDMA interface and set some parameters.
 */
static int query_rdma_interface(iface_t *iface)
{
	int ret;

	ret = ibv_query_device(iface->ibv_context,
						   &iface->cap.device_attr);
	if (ret)
		return ret;

	/* We need SRQs. */
	if (iface->cap.device_attr.max_srq == 0)
		return -1;

	/* Preset QP parameters. */
	iface->cap.max_send_wr = min(get_param(PTL_MAX_QP_SEND_WR) + get_param(PTL_MAX_RDMA_WR_OUT),
									iface->cap.device_attr.max_qp_wr);

	iface->cap.max_send_sge = max(get_param(PTL_MAX_INLINE_SGE), get_param(PTL_MAX_QP_SEND_SGE));
	iface->cap.max_send_sge = min(iface->cap.max_send_sge, iface->cap.device_attr.max_sge);

	iface->cap.max_srq_wr = min(get_param(PTL_MAX_SRQ_RECV_WR), iface->cap.device_attr.max_srq_wr);

	return 0;
}

/**
 * @brief Get an IPv4 address from network device name (e.g. ib0).
 *
 * Returns INADDR_ANY on error or if address is not assigned.
 *
 * @param[in] ifname The network interface name to use
 *
 * @return IPV4 address as an in_addr_t in network byte order
 */
static in_addr_t get_ip_address(const char *ifname)
{
	int fd;
	struct ifreq devinfo;
	struct sockaddr_in *sin = (struct sockaddr_in*)&devinfo.ifr_addr;
	in_addr_t addr;

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (fd < 0)
		return INADDR_ANY;

	strncpy(devinfo.ifr_name, ifname, IFNAMSIZ);

	if (ioctl(fd, SIOCGIFADDR, &devinfo) == 0 &&
	    sin->sin_family == AF_INET)
		addr = sin->sin_addr.s_addr;
	else
		addr = INADDR_ANY;

	close(fd);

	return addr;
}


/**
 * @brief Initialize interface.
 *
 * @param[in] iface The iface to init
 *
 * @return status
 */
int init_iface_rdma(iface_t *iface)
{
	int err;
	in_addr_t addr;
	int flags;

	/* check to see if interface is already initialized. */
	if (iface->cm_watcher.data == iface)
		return PTL_OK;

	/* check to see if interface device is present */
	if (if_nametoindex(iface->ifname) == 0) {
		ptl_warn("interface %d doesn't exist\n",
			 iface->iface_id);
		err = PTL_FAIL;
		goto err1;
	}

	/* check to see if interface has a valid IPV4 address */
	addr = get_ip_address(iface->ifname);
	if (addr == INADDR_ANY) {
		ptl_warn("interface %d doesn't have an IPv4 address\n",
			 iface->iface_id);
		err = PTL_FAIL;
		goto err1;
	}

	iface->sin.sin_family = AF_INET;
	iface->sin.sin_addr.s_addr = addr;

	iface->cm_channel = rdma_create_event_channel();
	if (!iface->cm_channel) {
		ptl_warn("unable to create CM event channel for interface %d\n",
			 iface->iface_id);
		err = PTL_FAIL;
		goto err1;
	}

	flags = fcntl(iface->cm_channel->fd, F_GETFL);
	err = fcntl(iface->cm_channel->fd, F_SETFL, flags | O_NONBLOCK);
	if (err < 0) {
		ptl_warn("cannot set asynchronous fd to non blocking\n");
		goto err1;
	}

	/* add a watcher for CM events */
	iface->cm_watcher.data = iface;
	ev_io_init(&iface->cm_watcher, process_cm_event,
		   iface->cm_channel->fd, EV_READ);

	EVL_WATCH(ev_io_start(evl.loop, &iface->cm_watcher));

	return PTL_OK;

 err1:
	cleanup_iface(iface);
	return err;
}

/**
 * @brief Prepare interface for accepting connections.
 *
 * This routine creates CM ID and binds it to the local IPV4 address
 * and port number. rdma_cm assigns the rdma device and allocates
 * a protection domain.
 *
 * The rdma device has a file descriptor for returning asynchronous
 * events and this is set to non blocking so libev can poll for
 * events.
 *
 * @pre caller should hold gbl mutex.
 *
 * @param[in] iface interface to prepare
 * @param[in] port TCP/RDMA_CM port number in network byte order
 *
 * @return status
 */
static int iface_bind(iface_t *iface, unsigned int port)
{
	int ret;
	int flags;

	/* check to see if we are already configured */
	if (iface->listen_id) {
		/* Already bound. If we want to bind to the same port, or a
		 * random port then it's ok. */
		if (port == 0 || port == iface->sin.sin_port)
			return PTL_OK;

		ptl_warn("interface already bound\n");
		return PTL_FAIL;
	}

	iface->sin.sin_port = port;

	/* Create a RDMA CM ID and bind it to retrieve the context and
	 * PD. These will be valid for as long as librdmacm is not
	 * unloaded, ie. when the program exits. */
	ret = rdma_create_id(iface->cm_channel, &iface->listen_id,
			     NULL, RDMA_PS_TCP);
	if (ret) {
		ptl_warn("unable to create CM ID\n");
		goto err1;
	}

	ret = rdma_bind_addr(iface->listen_id, (struct sockaddr *)&iface->sin);
	if (ret) {
		ptl_warn("unable to bind to local address %x\n",
			 iface->sin.sin_addr.s_addr);
		goto err1;
	}

	/* in case we asked for any port get the actual source port */
	iface->sin.sin_port = rdma_get_src_port(iface->listen_id);

	iface->ibv_context = iface->listen_id->verbs;
	iface->pd = ibv_alloc_pd(iface->ibv_context);

	/* check to see we have rdma device and protection domain */
	if (iface->ibv_context == NULL || iface->pd == NULL) {
		ptl_warn("unable to get the CM ID context or PD\n");
		goto err1;
	}

	/* Query the RDMA interface. */
	ret = query_rdma_interface(iface);
	if (ret) {
		ptl_warn("unable to query the RDMA interface\n");
		goto err1;
	}

	/* change the blocking mode of the async event queue */
	flags = fcntl(iface->ibv_context->async_fd, F_GETFL);
	ret = fcntl(iface->ibv_context->async_fd, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0) {
		ptl_warn("cannot set asynchronous fd to non blocking\n");
		goto err1;
	}

	/* remember the physical pid. */
	iface->id.phys.pid = port_to_pid(rdma_get_src_port(iface->listen_id));

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

static void process_async(EV_P_ ev_io *w, int revents)
{
	ni_t *ni = w->data;
	struct ibv_async_event event;
	int err;

	return;

	err = gbl_get();
	if (unlikely(err)) {
		return;
	}

	/* Get the async event */
	if (ni->iface) {
		if (ibv_get_async_event(ni->iface->ibv_context, &event) == 0) {

			ptl_warn("Got an unexpected asynchronous event: %d\n", event.event_type);

			/* Ack the event */
			ibv_ack_async_event(&event);
		} else {
			ptl_warn("Failed to get the asynchronous event\n");
		}
	}

	gbl_put();
}

static int init_rdma_srq(ni_t *ni)
{
	struct ibv_srq_init_attr srq_init_attr;
	iface_t *iface = ni->iface;
	int err;
	int num_post;

	srq_init_attr.srq_context = ni;
	srq_init_attr.attr.max_wr = iface->cap.max_srq_wr;
	srq_init_attr.attr.max_sge = 1;
	srq_init_attr.attr.srq_limit = 0; /* should be ignored */

	/* Create regular SRQ. */
	ni->rdma.srq = ibv_create_srq(iface->pd, &srq_init_attr);

	if (!ni->rdma.srq) {
		WARN();
		return PTL_FAIL;
	}

	/* post receive buffers to the shared receive queue */
	while ((num_post = min(ni->iface->cap.max_srq_wr
						   - atomic_read(&ni->rdma.num_posted_recv),
				get_param(PTL_SRQ_REPOST_SIZE))) > 0) {
		err = ptl_post_recv(ni, num_post);
		if (err) {
			WARN();
			return PTL_FAIL;
		}
	}

	return PTL_OK;
}

static int ni_rcqp_cleanup(ni_t *ni)
{
	struct ibv_wc wc;
	int n;
	buf_t *buf;

	if (!ni->rdma.cq)
		return PTL_OK;

	while(1) {
		n = ibv_poll_cq(ni->rdma.cq, 1, &wc);
		if (n < 0)
			WARN();

		if (n != 1)
			break;

		buf = (buf_t *)(uintptr_t)wc.wr_id;

		if (!buf)
			continue;

		switch (buf->type) {
		case BUF_SEND:
			break;
		case BUF_RDMA:
			list_del(&buf->list);
			break;
		case BUF_RECV:
			list_del(&buf->list);
			break;
		default:
			abort();
		}

		/* Free pending references. They can come from send_message_rdma or buf_alloc. */
		while(buf_ref_cnt(buf))
			buf_put(buf);
	}

	return PTL_OK;
}

void cleanup_rdma(ni_t *ni)
{
	buf_t *buf;

	EVL_WATCH(ev_io_stop(evl.loop, &ni->rdma.async_watcher));

	if (ni->rdma.self_cm_id) {
		rdma_destroy_id(ni->rdma.self_cm_id);
	}

	if (ni->rdma.srq) {
		/* This lock should be removed later, testing for possible race condition */
		PTL_FASTLOCK_LOCK(&ni->rdma.srq_lock);
		ibv_destroy_srq(ni->rdma.srq);
		ni->rdma.srq = NULL;
		PTL_FASTLOCK_UNLOCK(&ni->rdma.srq_lock);
	}

	ni_rcqp_cleanup(ni);

	if (ni->rdma.cq) {
		ibv_destroy_cq(ni->rdma.cq);
		ni->rdma.cq = NULL;
	}

	if (ni->rdma.ch) {
		ibv_destroy_comp_channel(ni->rdma.ch);
		ni->rdma.ch = NULL;
	}

	/* Release the buffers still on the send_list and recv_list. */

	/* TODO: cleanup of the XT/XI and their buffers that might still
	 * be in flight. It's only usefull when something bad happens, so
	 * it's not critical. */
	while(!list_empty(&ni->rdma.recv_list)) {
		struct list_head *entry = ni->rdma.recv_list.next;
		list_del(entry);
		buf = list_entry(entry, buf_t, list);
		buf_put(buf);
	}

	PTL_FASTLOCK_DESTROY(&ni->rdma.recv_list_lock);
}

/* Must be locked by gbl_mutex. */
static int init_rdma(iface_t *iface, ni_t *ni)
{
	int err;
	int cqe;

	ni->id.phys.nid = addr_to_nid(&iface->sin);

	if (iface->id.phys.nid == PTL_NID_ANY) {
		iface->id.phys.nid = ni->id.phys.nid;
	} else if (iface->id.phys.nid != ni->id.phys.nid) {
		WARN();
		goto err1;
	}

	ptl_info("setting ni->id.phys.nid = %x\n", ni->id.phys.nid);

	err = iface_bind(iface, pid_to_port(ni->id.phys.pid));
	if (err) {
		ptl_warn("Binding failed\n");
		WARN();
		goto err1;
	}

	if ((ni->options & PTL_NI_PHYSICAL) &&
		(ni->id.phys.pid == PTL_PID_ANY)) {
		/* No well know PID was given. Retrieve the pid given by
		 * bind. */
		ni->id.phys.pid = iface->id.phys.pid;

		ptl_info("set iface pid(1) = %x\n", iface->id.phys.pid);
	}

	/* Create CC, CQ, SRQ. */
	ni->rdma.ch = ibv_create_comp_channel(iface->ibv_context);
	if (!ni->rdma.ch) {
		ptl_warn("unable to create comp channel\n");
		WARN();
		goto err1;
	}

	/* TODO: this is not enough, but we don't know the number of ranks yet. */
	cqe = ni->iface->cap.max_send_wr * 10 + ni->iface->cap.max_srq_wr + 10;
	if (cqe > ni->iface->cap.device_attr.max_cqe)
		cqe = ni->iface->cap.device_attr.max_cqe;

	ni->rdma.cq = ibv_create_cq(iface->ibv_context, cqe, ni, ni->rdma.ch, 0);
	if (!ni->rdma.cq) {
		WARN();
		ptl_warn("unable to create cq\n");
		WARN();
		goto err1;
	}

	err = ibv_req_notify_cq(ni->rdma.cq, 0);
	if (err) {
		ptl_warn("unable to req notify\n");
		WARN();
		goto err1;
	}

	return PTL_OK;

 err1:
	cleanup_rdma(ni);
	return PTL_FAIL;
}

int PtlNIInit_rdma(gbl_t *gbl, ni_t *ni)
{
	int err;
	iface_t *iface = ni->iface;

	INIT_LIST_HEAD(&ni->rdma.recv_list);
	atomic_set(&ni->rdma.num_conn, 0);
	PTL_FASTLOCK_INIT(&ni->rdma.recv_list_lock);
	PTL_FASTLOCK_INIT(&ni->rdma.srq_lock);

	err = init_rdma(iface, ni);
	if (unlikely(err))
		goto error;

	/* Create shared receive queue */
	err = init_rdma_srq(ni);
	if (unlikely(err))
		goto error;

	/* Add a watcher for asynchronous events. */
	ev_io_init(&ni->rdma.async_watcher, process_async, iface->ibv_context->async_fd, EV_READ);
	ni->rdma.async_watcher.data = ni;
	EVL_WATCH(ev_io_start(evl.loop, &ni->rdma.async_watcher));

	/* Ready to listen. */
	if (!iface->listen) {
		if (rdma_listen(iface->listen_id, 0)) {
			ptl_warn("Failed to listen\n");
			WARN();
			goto error;
		}

		iface->listen = 1;
	}

 error:
	return err;
}
