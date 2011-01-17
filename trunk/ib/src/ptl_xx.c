/*
 * ptl_xx.c - transaction descriptors
 */

#include "ptl_loc.h"

void xi_release(void *arg)
{
	xi_t *xi = arg;

	pthread_spin_destroy(&xi->send_lock);
	pthread_spin_destroy(&xi->recv_lock);
	pthread_spin_destroy(&xi->state_lock);
}

void xi_init(void *arg)
{
	xi_t *xi = arg;

	xi->ack_req = PTL_NO_ACK_REQ;

	INIT_LIST_HEAD(&xi->recv_list);
	pthread_spin_init(&xi->send_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xi->recv_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xi->state_lock, PTHREAD_PROCESS_PRIVATE);
}

void xt_release(void *arg)
{
	xt_t *xt = arg;

	pthread_spin_destroy(&xt->send_lock);
	pthread_spin_destroy(&xt->recv_lock);
	pthread_spin_destroy(&xt->state_lock);
}

void xt_init(void *arg)
{
	xt_t *xt = arg;

	INIT_LIST_HEAD(&xt->recv_list);
	pthread_spin_init(&xt->send_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xt->recv_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&xt->state_lock, PTHREAD_PROCESS_PRIVATE);
}
