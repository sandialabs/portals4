/*
 * ptl_xx.c - transaction descriptors
 */

#include "ptl_loc.h"

/*
 * xi_setup
 *	called when new xi is allocated
 */
int xi_setup(void *arg)
{
	xi_t *xi = arg;

	OBJ_NEW(xi);
	INIT_LIST_HEAD(&xi->send_list);
	INIT_LIST_HEAD(&xi->rdma_list);
	pthread_spin_init(&xi->send_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xi->rdma_list_lock, PTHREAD_PROCESS_PRIVATE);

	return PTL_OK;
}

void xi_cleanup(void *arg)
{
	xi_t *xi = arg;

	pthread_spin_destroy(&xi->send_list_lock);
	pthread_spin_destroy(&xi->rdma_list_lock);
}

/*
 * xt_setup
 *	called when new xt is allocated
 */
int xt_setup(void *arg)
{
	xt_t *xt = arg;

	OBJ_NEW(xt);

	INIT_LIST_HEAD(&xt->unexpected_list);
	INIT_LIST_HEAD(&xt->send_list);
	INIT_LIST_HEAD(&xt->rdma_list);
	pthread_spin_init(&xt->send_list_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xt->rdma_list_lock, PTHREAD_PROCESS_PRIVATE);

	return PTL_OK;
}

void xt_cleanup(void *arg)
{
	xt_t *xt = arg;

	pthread_spin_destroy(&xt->send_list_lock);
	pthread_spin_destroy(&xt->rdma_list_lock);
}
