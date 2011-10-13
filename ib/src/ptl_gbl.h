/*
 * ptl_gbl.h
 */

#ifndef PTL_GBL_H
#define PTL_GBL_H

struct ni;

typedef struct gbl {
	int			num_iface;	/* size of interface table */
	iface_t			*iface;		/* interface table */
	pthread_mutex_t		gbl_mutex;
	int			init_once;
	int			ref_cnt;	/* count PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	int			fd;
	struct sockaddr_in	addr;
	pthread_t		event_thread;
	int			event_thread_run;
	pool_t			ni_pool;
} gbl_t;

extern gbl_t per_proc_gbl;
extern void gbl_release(ref_t *ref);

/*
 * get_gbl()
 *	get a reference to per process global state
 *	must be matched with a call to gbl_put()
 *
 * Outputs
 *	gbl_p
 *
 * Return Value
 *	PTL_OK			success, gbl contains address of per_proc_gbl
 *	PTL_NO_INIT		failure, per_proc_gbl is not in init state
 */
static inline int get_gbl(void)
{
#ifndef NO_ARG_VALIDATION
	if (unlikely(per_proc_gbl.ref_cnt == 0))
		return PTL_NO_INIT;

	ref_get(&per_proc_gbl.ref);
#endif
	return PTL_OK;
}

/*
 * gbl_put()
 *	release a reference to per process global state
 *	obtained from a call to get_gbl()
 *
 * Inputs
 *	gbl
 *
 * Return Value
 *	none
 */
static inline void gbl_put(void)
{
#ifndef NO_ARG_VALIDATION
	ref_put(&per_proc_gbl.ref, gbl_release);
#endif
}

struct ni *gbl_lookup_ni(gbl_t *gbl, ptl_interface_t iface, int ni_type);

int gbl_add_ni(gbl_t *gbl, struct ni *ni);

int gbl_remove_ni(gbl_t *gbl, struct ni *ni);

#endif /* PTL_GBL_H */
