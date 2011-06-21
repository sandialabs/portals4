/*
 * ptl_xx.c - transaction descriptors
 */

#include "ptl_loc.h"

/*
 * xi_new
 *	called when new xi is allocated
 */
int xi_new(void *arg)
{
	xi_t *xi = arg;

	OBJ_NEW(xi);

	return PTL_OK;
}

/*
 * xt_new
 *	called when new xt is allocated
 */
int xt_new(void *arg)
{
	xt_t *xt = arg;

	OBJ_NEW(xt);

	INIT_LIST_HEAD(&xt->unexpected_list);

	return PTL_OK;
}
