/*
 * ptl_gbl.h
 */

#ifndef PTL_LIGHT_LIB_H
#define PTL_LIGHT_LIB_H

typedef struct gbl {
	pthread_mutex_t		gbl_mutex;

	int			ref_cnt;	/* count PtlInit/PtlFini */
	ref_t			ref;		/* sub objects references */
	int finalized;
} gbl_t;

extern void gbl_release(ref_t *ref);

#endif	/* PTL_LIGHT_LIB_H */
