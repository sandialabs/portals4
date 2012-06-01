/*
 * ptl_gbl.h
 */

#ifndef PTL_GBL_H
#define PTL_GBL_H

typedef struct gbl {
	int			num_iface;	/* size of interface table */
	iface_t			*iface;		/* interface table */
	int			ref_cnt;	/* count PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	int finalized;

	pthread_mutex_t		gbl_mutex;
	pool_t			ni_pool;
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
	
	/* Mapping of the whole process. */
	xpmem_apid_t apid;

	gbl_t gbl;
};

extern struct transport transport_mem;

/* Extends the ptl_le_t structure to store mappings. */
struct ptl_le_ppe {
	ptl_le_t le_init;
	void *client_start;
};

/* Extends the ptl_le_t structure to store mappings. */
struct ptl_me_ppe {
	ptl_me_t me_init;
	void *client_start;
};

/* Extends the ptl_md_t structure to store mappings. */
struct ptl_md_ppe {
	ptl_md_t md_init;
	void *client_start;
};

#endif /* PTL_GBL_H */
