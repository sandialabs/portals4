/**
 * @file ptl_iface.c
 *
 * @brief Interface table management.
 */

#include "ptl_loc.h"

/**
 * @brief Cleanup iface resources.
 *
 * @param[in] iface The interface to cleanup
 */
void cleanup_iface(iface_t *iface)
{
	/* stop CM event watcher */
	EVL_WATCH(ev_io_stop(evl.loop, &iface->cm_watcher));

	/* destroy CM ID */
	if (iface->listen_id) {
		rdma_destroy_id(iface->listen_id);
		iface->listen_id = NULL;
		iface->listen = 0;
	}

	/* destroy CM event channel */
	if (iface->cm_channel) {
		rdma_destroy_event_channel(iface->cm_channel);
		iface->cm_channel = NULL;
	}

	/* Release PD. */
	if (iface->pd) {
		ibv_dealloc_pd(iface->pd);
		iface->pd = NULL;
	}

	/* mark as uninitialized */
	iface->sin.sin_addr.s_addr = INADDR_ANY;
	iface->ifname[0] = 0;
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
int init_iface(iface_t *iface)
{
	int err;
	in_addr_t addr;
	int flags;

	/* check to see if interface is already initialized. */
	if (iface->cm_watcher.data == iface)
		return PTL_OK;

	/* check to see if interface device is pesent */
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
 * @brief Cleanup interface table.
 *
 * Called from gbl_release from last PtlFini call.
 *
 * @param[in] gbl global state
 */
void iface_fini(gbl_t *gbl)
{
	gbl->num_iface = 0;
	free(gbl->iface);
	gbl->iface = NULL;
}

/**
 * @brief Initialize interface table.
 *
 * Called from first PtlInit call.
 *
 * @param[in] gbl global state
 *
 * @return status
 */
int init_iface_table(gbl_t *gbl)
{
	int i;
	int num_iface = get_param(PTL_MAX_IFACE);

	if (!gbl->iface) {
		gbl->num_iface = num_iface;
		gbl->iface = calloc(num_iface, sizeof(iface_t));
		if (!gbl->iface)
			return PTL_NO_SPACE;
	}

	for (i = 0; i < num_iface; i++) {
		gbl->iface[i].iface_id = i;
		gbl->iface[i].id.phys.nid = PTL_NID_ANY;
		gbl->iface[i].id.phys.pid = PTL_PID_ANY;

		/* the interface name is "ib" followed by the interface id
		 * in the future we may support other RDMA devices */
		sprintf(gbl->iface[i].ifname, "ib%d", i);
	}

	return PTL_OK;
}

/**
 * @brief Return interface from iface_id.
 *
 * Default iface is the first table entry.
 *
 * @param[in] gbl global state
 * @param[in] iface_id the iface_id to use
 *
 * @return the iface table entry with index iface_id
 */
iface_t *get_iface(gbl_t *gbl, ptl_interface_t iface_id)
{
	/* check to see that we have a non empty interface table */
	if (!gbl->num_iface || !gbl->iface) {
		ptl_warn("no iface available\n");
		return NULL;
	}

	if (iface_id == PTL_IFACE_DEFAULT) {
		/* check to make sure that interface is present */
		if (if_nametoindex(gbl->iface[0].ifname) > 0) {
			return &gbl->iface[0];
		} else {
			ptl_warn("unable to get default iface\n");
			return NULL;
		}
	} else if (iface_id >= gbl->num_iface) {
		ptl_warn("requested iface %d out of range\n", iface_id);
		return NULL;
	} else {
		return &gbl->iface[iface_id];
	}
}

/**
 * @brief Lookup ni in iface table from ni type.
 *
 * Takes a reference on the ni which should
 * be dropped by caller when the ni is no
 * longer needed.
 *
 * @pre caller should hold global mutex
 *
 * @param[in] iface the interface to use
 * @param[in] ni_type the ni type to get
 *
 * @return ni or NULL
 */
ni_t *__iface_get_ni(iface_t *iface, int ni_type)
{
	ni_t *ni;

	/* sanity check */
	assert(ni_type < MAX_NI_TYPES);

	ni = iface->ni[ni_type];
	if (ni)
		atomic_inc(&ni->ref_cnt);

	return ni;
}

/**
 * @brief Add ni to iface table.
 *
 * @pre caller should hold global mutex
 *
 * @param[in] iface the interface table entry to use
 * @param[in] ni the ni to add to the entry
 */
void __iface_add_ni(iface_t *iface, ni_t *ni)
{
	int type = ni->ni_type;

	/* sanity check */
	assert(iface->ni[type] == NULL);

	iface->ni[type] = ni;
	ni->iface = iface;
}

/**
 * @brief Remove ni from iface table.
 *
 * @pre caller should hold global mutex
 *
 * @param[in] ni The ni to remove from interface
 */
void __iface_remove_ni(ni_t *ni)
{
	iface_t *iface = ni->iface;
	int type = ni->ni_type;

	/* sanity check */
	assert(ni == iface->ni[type]);

	iface->ni[type] = NULL;
	ni->iface = NULL;
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
int __iface_bind(iface_t *iface, unsigned int port)
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

#ifdef USE_XRC
	rdma_query_id(iface->listen_id, &iface->ibv_context, &iface->pd);
#else
	iface->ibv_context = iface->listen_id->verbs;
	iface->pd = ibv_alloc_pd(iface->ibv_context);
#endif

	/* check to see we have rdma device and protection domain */
	if (iface->ibv_context == NULL || iface->pd == NULL) {
		ptl_warn("unable to get the CM ID context or PD\n");
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
