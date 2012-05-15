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

#if WITH_TRANSPORT_SHMEM || IS_PPE
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

	for (i = 0; i < num_iface; i++) {
		gbl->iface[i].iface_id = i;
		gbl->iface[i].id.phys.nid = PTL_NID_ANY;
		gbl->iface[i].id.phys.pid = PTL_PID_ANY;

#if WITH_TRANSPORT_IB
		/* the interface name is "ib" followed by the interface id
		 * in the future we may support other RDMA devices */
		sprintf(gbl->iface[i].ifname, "ib%d", i);
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

/**
 * @brief Add ni to iface table.
 *
 * @pre caller should hold global mutex
 *
 * @param[in] iface the interface table entry to use
 * @param[in] ni the ni to add to the entry
 */
void iface_add_ni(iface_t *iface, ni_t *ni)
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
void iface_remove_ni(ni_t *ni)
{
	iface_t *iface = ni->iface;
	int type = ni->ni_type;

	/* sanity check */
	assert(ni == iface->ni[type]);

	iface->ni[type] = NULL;
}
