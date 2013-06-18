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
#if WITH_TRANSPORT_IB
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
#endif

#if WITH_TRANSPORT_UDP
	//close the socket
	close(iface->udp.connect_s);
#endif

	iface->ifname[0] = 0;
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
	int i;

	for (i=0; i<gbl->num_iface; i++) {
		iface_t *iface = &gbl->iface[i];
		cleanup_iface(iface);
	}

	free(gbl->iface);
	gbl->iface = NULL;
	gbl->num_iface = 0;

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

#if WITH_TRANSPORT_SHMEM || IS_PPE || WITH_TRANSPORT_UDP
	/* Clip the number of interfaces for shmem.
	 * Note: Is that really necessary? */
	num_iface = 1;
#endif

	if (!gbl->iface) {
		gbl->num_iface = num_iface;
		gbl->iface = calloc(num_iface, sizeof(iface_t));
		if (!gbl->iface)
			return PTL_NO_SPACE;
	}

	int current_if_num = 0;

	for (i = 0; i < num_iface; i++) {
		gbl->iface[i].iface_id = i;
		gbl->iface[i].id.phys.nid = PTL_NID_ANY;
		gbl->iface[i].id.phys.pid = PTL_PID_ANY;
		gbl->iface[i].gbl = gbl;

#if WITH_TRANSPORT_IB
		/* the interface name is "ib" followed by the interface id
		 * in the future we may support other RDMA devices */
		sprintf(gbl->iface[i].ifname, "ib%d", current_if_num);
		current_if_num++;
#endif

#if WITH_TRANSPORT_UDP
		/* the interface name is "eth" followed by the interface id */
		sprintf(gbl->iface[i].ifname, "eth%d", current_if_num);

#ifdef __APPLE__
                //sprintf(gbl->iface[i].ifname, "lo%d",i);
		sprintf(gbl->iface[i].ifname, "en%d", current_if_num);
#endif
		//sprintf(gbl->iface[i].ifname, "lo");
		in_addr_t addr;
		addr = check_ip_address(gbl->iface[i].ifname);
	        if (addr == INADDR_ANY) {
                	ptl_warn("interface %d of %d doesn't have an IPv4 address, searching for valid interface\n",
                        gbl->iface[i].iface_id,num_iface);
                	while (addr == INADDR_ANY && current_if_num < 100) {
				current_if_num++;
#ifdef __APPLE__
				sprintf(gbl->iface[i].ifname, "en%d", current_if_num);
#else
				sprintf(gbl->iface[i].ifname, "eth%d", current_if_num);
#endif
				addr = check_ip_address(gbl->iface[i].ifname);
			};
			if (current_if_num == 100 && addr == INADDR_ANY){
				ptl_warn("no valid network device found \n");
				return PTL_NO_INIT;
			}
			else{
				ptl_warn("valid interface IPv4 address found: %s %i\n",gbl->iface[i].ifname,current_if_num);
			}	
                }


		gbl->iface[i].udp.connect_s = -1;
#endif

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
#if WITH_TRANSPORT_IB
		/* check to make sure that interface is present */
		if (if_nametoindex(gbl->iface[0].ifname) > 0) {
			return &gbl->iface[0];
		} else {
			ptl_warn("unable to get default iface\n");
			return NULL;
		}
#else
		return &gbl->iface[0];
#endif
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
ni_t *iface_get_ni(iface_t *iface, int ni_type)
{
	ni_t *ni;

	/* sanity check */
	assert(ni_type < MAX_NI_TYPES);

	ni = iface->ni[ni_type];
	if (ni)
		atomic_inc(&ni->ref_cnt);

	return ni;
}

/* get an IPv4 address from network device name (e.g. ib0).
 *
 * Returns INADDR_ANY on error or if address is not assigned.
 *
 * @param[in] ifname The network interface name to use
 *
 * @return IPV4 address as an in_addr_t in network byte order
 */
in_addr_t check_ip_address(const char *ifname)
{
        int fd;
        struct ifreq devinfo;
        struct sockaddr_in *sin = (struct sockaddr_in*)&devinfo.ifr_addr;
        in_addr_t addr;

        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (fd < 0){
		ptl_warn("error initializing socket \n");		
                return INADDR_ANY;
	}

        strncpy(devinfo.ifr_name, ifname, IFNAMSIZ);

        if (ioctl(fd, SIOCGIFADDR, &devinfo) == 0){
		ptl_warn("got a valid address for: %s \n",devinfo.ifr_name);	
                addr = sin->sin_addr.s_addr;
	}
        else{
                addr = INADDR_ANY;
	}
        close(fd);

        return addr;
}

