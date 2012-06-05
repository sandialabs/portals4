/*
 * ptl_gbl.h
 */

#ifndef PTL_GBL_H
#define PTL_GBL_H

// todo: can we merge client and gbl ?
// todo: have common parameters for the various gbl_t in a structure or define.
typedef struct gbl {
	int			num_iface;	/* size of interface table */
	iface_t			*iface;		/* interface table */
	int			ref_cnt;	/* count PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	int finalized;

	pthread_mutex_t		gbl_mutex;
	pool_t			ni_pool;

	/* PPE specific. */

	/* Mapping of the whole process. */
	xpmem_apid_t apid;
} gbl_t;

static inline int gbl_get(void)
{
	return PTL_OK;
}

static inline void gbl_put(void)
{
}

extern void gbl_release(ref_t *ref);

/* Represents a client connected to this PPE. */
struct client
{
	struct list_head list;

	/* Keep the PID of the client. It's the key to the list. */
	pid_t pid;
	
	gbl_t gbl;
};

extern struct transport transport_mem;

#endif /* PTL_GBL_H */
