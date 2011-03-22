/*
 * ptl_gbl.h
 */

#ifndef PTL_GBL_H
#define PTL_GBL_H

#define MAX_IFACE		(32)

struct ni;
struct rpc;

typedef struct iface {
	struct ni		*ni[4];
	char			if_name[IF_NAMESIZE];
} iface_t;

typedef struct gbl {

	iface_t			iface[MAX_IFACE];
	pthread_mutex_t		gbl_mutex;

	int			init_once;
	int			ref_cnt;		/* PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	
	int			fd;
	struct sockaddr_in	addr;

	pthread_t		event_thread;
	int			event_thread_run;

	struct rpc		*rpc;

	ptl_jid_t		jid;
	ptl_nid_t		nid;

	ptl_nid_t		main_ctl_nid; /* NID of the main control process */

	ptl_rank_t rank;
	unsigned int nranks;		/* total number of rank for job */

	ptl_rank_t local_rank;
	unsigned int local_nranks;	/* number of ranks on that node */

	unsigned int num_nids;		/* number of nodes */

} gbl_t;

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
int get_gbl(gbl_t **gbl_p);

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
void gbl_put(gbl_t *gbl);

struct ni *gbl_lookup_ni(gbl_t *gbl, ptl_interface_t iface, int ni_type);

int gbl_add_ni(gbl_t *gbl, struct ni *ni);

int gbl_remove_ni(gbl_t *gbl, struct ni *ni);

#endif /* PTL_GBL_H */
