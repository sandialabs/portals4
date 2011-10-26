/*
 * ptl_xx.c - transaction descriptors
 */

#include "ptl_loc.h"


/*
 * xt_setup
 *	called when new xt is allocated
 */
int xt_setup(void *arg)
{
	xt_t *xt = arg;

	OBJ_NEW(xt);

	INIT_LIST_HEAD(&xt->unexpected_list);
	INIT_LIST_HEAD(&xt->rdma_list);
	pthread_spin_init(&xt->rdma_list_lock, PTHREAD_PROCESS_PRIVATE);

	return PTL_OK;
}

void xt_cleanup(void *arg)
{
	xt_t *xt = arg;

	pthread_spin_destroy(&xt->rdma_list_lock);
}
