/*
 * ptl_gbl.h
 */

#ifndef PTL_GBL_H
#define PTL_GBL_H

struct ni;
struct iface;

extern void gbl_release(ref_t *ref);

/* gbl is a structure to keep a client's information. There is a
 * unique instance for the fat library (per_proc_gbl), it doesn't
 * exist for the light library, and the PPE has an instance for each
 * client. We need to pass around the gbl because it contains the
 * index table from which we find an object given a portals
 * handle. However for the fat and light libraries, this isn't needed
 * because the gbl is global (hence known) or unused. So we play
 * preprocessor tricks to extend the portal API and add the
 * corresponding gbl to almost each portals API functions. */
#if IS_PPE
typedef struct gbl {
	int			num_iface;	/* size of interface table */
	struct iface			*iface;		/* interface table */
	int			ref_cnt;	/* count PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	int finalized;

	pthread_mutex_t		gbl_mutex;
	pool_t			ni_pool;

	atomic_t next_index;
	void **index_map;

	/* PPE specific. */

	/* Mapping of the whole process. */
	xpmem_apid_t apid;

	/* Number of the progress thread assigned to this client. */
	unsigned int prog_thread;
} gbl_t;

static inline int gbl_get(void) { return PTL_OK; }
static inline void gbl_put(void) { }
#define PPEGBL struct gbl *gbl,
#define MYGBL gbl
#define MYGBL_ gbl,
#define MYNIGBL_ ni->iface->gbl,
#elif IS_LIGHT_LIB

typedef struct gbl {
	pthread_mutex_t		gbl_mutex;

	int			ref_cnt;	/* count PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	int finalized;

	atomic_t next_index;
	void **index_map;
} gbl_t;

/* The light client doesn't have a GBL. */
#define PPEGBL

#define MYGBL ((gbl_t *)NULL)
#define MYGBL_

#else

typedef struct gbl {
	int			num_iface;	/* size of interface table */
	struct iface			*iface;		/* interface table */
	pthread_mutex_t		gbl_mutex;
	int			ref_cnt;	/* count PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	int finalized;
	int			fd;
	struct sockaddr_in	addr;
	pthread_t		event_thread;
	int			event_thread_run;
	struct pool			ni_pool;

	atomic_t next_index;
	void **index_map;
} gbl_t;

extern gbl_t per_proc_gbl;
#define PPEGBL
#define MYGBL (&per_proc_gbl)
#define MYGBL_
#define MYNIGBL_

/*
 * gbl_get()
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
static inline int gbl_get(void)
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
 *	obtained from a call to gbl_get()
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

#endif

struct ni *gbl_lookup_ni(gbl_t *gbl, ptl_interface_t iface, int ni_type);

int gbl_add_ni(gbl_t *gbl, struct ni *ni);

int gbl_remove_ni(gbl_t *gbl, struct ni *ni);

#endif /* PTL_GBL_H */
