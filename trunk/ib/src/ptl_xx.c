/*
 * ptl_xx.c - transaction descriptors
 */

#include "ptl_loc.h"

/*
 * xi_fini
 *	called when xi is destroyed
 */
void xi_fini(void *arg)
{
	xi_t *xi = arg;

	pthread_spin_destroy(&xi->send_lock);
	pthread_spin_destroy(&xi->recv_lock);
	pthread_spin_destroy(&xi->state_lock);
}

/*
 * xi_init
 *	called when xi is created
 */
int xi_init(void *arg, void *parm)
{
	xi_t *xi = arg;

	xi->ack_req = PTL_NO_ACK_REQ;

	pthread_spin_init(&xi->send_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xi->recv_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xi->state_lock, PTHREAD_PROCESS_PRIVATE);

	return 0;
}

/*
 * xi_new
 *	called when new xi is alocated
 */
int xi_new(void *arg)
{
	xi_t *xi = arg;

	xi->data_in = NULL;
	xi->data_out = NULL;
	xi->conn = NULL;

	return PTL_OK;
}

/*
 * xt_fini
 *	called when xt is destroyed
 */
void xt_fini(void *arg)
{
	xt_t *xt = arg;

	pthread_spin_destroy(&xt->send_lock);
	pthread_spin_destroy(&xt->recv_lock);
	pthread_spin_destroy(&xt->state_lock);
}

/*
 * xt_init
 *	called when xt is created
 */
int xt_init(void *arg, void *parm)
{
	xt_t *xt = arg;

	pthread_spin_init(&xt->send_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xt->recv_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xt->state_lock, PTHREAD_PROCESS_PRIVATE);

	xt->event_mask = 0;
	xt->put_resid = 0;
	xt->get_resid = 0;

	return 0;
}

/*
 * xt_new
 *	called when new xt is alocated
 */
int xt_new(void *arg)
{
	xt_t *xt = arg;

	xt->data_in = NULL;
	xt->data_out = NULL;
	xt->conn = NULL;

	return PTL_OK;
}
