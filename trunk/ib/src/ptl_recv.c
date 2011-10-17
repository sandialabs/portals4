/*
 * ptl_recv.c - initial receive processing
 */
#include "ptl_loc.h"

/*
 * recv_state_name
 *	for debugging output
 */
static char *recv_state_name[] = {
	[STATE_RECV_SEND_COMP]		= "send_comp",
	[STATE_RECV_RDMA_COMP]		= "rdma_comp",
	[STATE_RECV_PACKET]		= "recv_packet",
	[STATE_RECV_DROP_BUF]		= "recv_drop_buf",
	[STATE_RECV_REQ]		= "recv_req",
	[STATE_RECV_INIT]		= "recv_init",
	[STATE_RECV_REPOST]		= "recv_repost",
	[STATE_RECV_ERROR]		= "recv_error",
	[STATE_RECV_DONE]		= "recv_done",
};

/*
 * comp_poll
 *	poll for completion event
 *	returns buffer that completed
 *	note we can optimize this by
 *	polling for multiple events and then
 *	queuing the resulting buffers
 */
static int comp_poll(ni_t *ni, int num_wc,
		     struct ibv_wc wc_list[], buf_t *buf_list[])
{
	int ret;
	int i, j;
	struct ibv_wc *wc;
	buf_t *buf;

	ret = ibv_poll_cq(ni->rdma.cq, num_wc, wc_list);
	if (ret <= 0)
		return ret;

	for (i = 0, j = 0; i < ret; i++) {
		wc = &wc_list[i];

		buf = (buf_t *)(uintptr_t)wc->wr_id;
		buf->length = wc_list->byte_len;

		if (wc->status) {
			if (buf->type == BUF_SEND) {
				buf->xi->ni_fail = PTL_NI_UNDELIVERABLE;
				buf->state = STATE_RECV_SEND_COMP;
			} 
			else if (buf->type == BUF_RDMA) {
				buf->state = STATE_RECV_ERROR;
			} else {
				buf->state = STATE_RECV_DROP_BUF;
			}
		} else {
			if (buf->type == BUF_SEND) {
				buf->state = STATE_RECV_SEND_COMP;
			} else if (buf->type == BUF_RDMA) {
				buf->state = STATE_RECV_RDMA_COMP;
			} else if (buf->type == BUF_RECV) {
				buf->state = STATE_RECV_PACKET;
			} else {
				buf->state = STATE_RECV_ERROR;
			}
		}

		buf_list[j++] = buf;
	}

	return j;
}

/*
 * send_comp
 *	process a send completion event
 */
static int send_comp(buf_t *buf)
{
	xi_t *xi = buf->xi;			/* can be an XT or an XI */

	assert(buf->comp);
	if (!buf->comp)
		return STATE_RECV_DONE;

	if (xi->obj.obj_pool->type == POOL_XI) {
		/* Fox XI only, restart the initiator state machine. */
		int err;
		xi->completed = 1;
		err = process_init(xi);
		if (err)
			WARN();
	}

	buf_put(buf);

	return STATE_RECV_DONE;
}

/*
 * rdma_comp
 *	process a send rdma completion event
 */
static int rdma_comp(buf_t *buf)
{
	struct list_head temp_list;
	int err;
	xt_t *xt = buf->xt;

	if (!buf->comp)
		return STATE_RECV_DONE;

	/* Take a ref on the XT since freeing all its buffers will also
	 * free it. */
	assert(xt);
	assert(atomic_read(&xt->rdma.rdma_comp) < 5000);
	xt_get(xt);

	pthread_spin_lock(&xt->rdma_list_lock);
	list_cut_position(&temp_list, &xt->rdma_list, &buf->list);
	pthread_spin_unlock(&xt->rdma_list_lock);

	atomic_dec(&xt->rdma.rdma_comp);

	while(!list_empty(&temp_list)) {
		buf = list_first_entry(&temp_list, buf_t, list);
		list_del(&buf->list);
		buf_put(buf);
	}

	err = process_tgt(xt);
	if (err) {
		WARN();
		xt_put(xt);
		return STATE_RECV_ERROR;
	}

	xt_put(xt);
	return STATE_RECV_DONE;
}

/*
 * recv_packet
 *	process a receive completion event
 */
static int recv_packet(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	hdr_t *hdr = (hdr_t *)buf->data;

	(void)__sync_sub_and_fetch(&ni->rdma.num_posted_recv, 1);

	/* Dequeue buf is on the recv list. Todo: create another
	 * intermediate state for IB stuff or dequeue it before. */
	if (!list_empty(&buf->list)) {
		pthread_spin_lock(&ni->rdma.recv_list_lock);
		list_del(&buf->list);
		pthread_spin_unlock(&ni->rdma.recv_list_lock);
	}

	if (buf->length < sizeof(hdr_t)) {
		WARN();
		return STATE_RECV_DROP_BUF;
	}

	if (hdr->version != PTL_HDR_VER_1) {
		WARN();
		return STATE_RECV_DROP_BUF;
	}

	if (hdr->ni_type != ni->ni_type) {
		WARN();
		return STATE_RECV_DROP_BUF;
	}

	switch (hdr->operation) {
	case OP_PUT:
	case OP_GET:
	case OP_ATOMIC:
	case OP_FETCH:
	case OP_SWAP:
		if (buf->length < sizeof(req_hdr_t)) {
			WARN();
			return STATE_RECV_DROP_BUF;
		}
		return STATE_RECV_REQ;

	case OP_DATA:
	case OP_REPLY:
	case OP_ACK:
	case OP_CT_ACK:
	case OP_OC_ACK:
	case OP_NO_ACK:
		return STATE_RECV_INIT;

	default:
		WARN();
		return STATE_RECV_DROP_BUF;
	}
}

/*
 * recv_req
 *	process an initial packet to target
 */
static int recv_req(buf_t *buf)
{
	int err;
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	ni_t *ni = obj_to_ni(buf);
	xt_t *xt;

	err = xt_alloc(ni, &xt);
	if (err) {
		WARN();
		return STATE_RECV_ERROR;
	}

	xport_hdr_to_xt((hdr_t *)hdr, xt);
	base_hdr_to_xt((hdr_t *)hdr, xt);
	req_hdr_to_xt(hdr, xt);
	xt->operation = hdr->operation;

	/* req packet data direction is wrt init, xt direction is wrt tgt */
	if (hdr->data_in)
		xt->data_out = (data_t *)(buf->data + sizeof(*hdr));

	if (hdr->data_out)
		xt->data_in = (data_t *)(buf->data + sizeof(*hdr) +
					  data_size(xt->data_out));

	xt->recv_buf = buf;
	xt->state = STATE_TGT_START;

	/* note process_tgt must drop recv_buf */
	err = process_tgt(xt);
	if (err)
		WARN();

	return STATE_RECV_REPOST;
}

/*
 * recv_init
 *	process an incoming packet to init
 */
static int recv_init(buf_t *buf)
{
	int err;
	xi_t *xi;
	hdr_t *hdr = (hdr_t *)buf->data;

	err = to_xi(be64_to_cpu(hdr->handle), &xi);
	if (err) {
		WARN();
		return STATE_RECV_DROP_BUF;
	}

	if (hdr->data_in)
		xi->data_out = (data_t *)(buf->data + hdr->hdr_size);

	if (hdr->data_out)
		xi->data_in = (data_t *)(buf->data + hdr->hdr_size +
					  data_size(xi->data_out));

	xi->recv_buf = buf;

	/* note process_init must drop recv_buf */
	err = process_init(xi);
	if (err)
		WARN();

	return STATE_RECV_REPOST;
}

/*
 * recv_repost
 *	repost receive buffers to srq
 */
static int recv_repost(buf_t *buf)
{
	int err;
	ni_t *ni = obj_to_ni(buf);
	int num_bufs;

	if ((get_param(PTL_MAX_SRQ_RECV_WR) - ni->rdma.num_posted_recv)
				> get_param(PTL_SRQ_REPOST_SIZE))
		ptl_post_recv(ni, get_param(PTL_SRQ_REPOST_SIZE));

	return STATE_RECV_DONE;
}

/*
 * recv_drop_buf
 *	drop buffer
 */
static int recv_drop_buf(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);

	buf_put(buf);
	ni->num_recv_drops++;

	return STATE_RECV_DONE;
}

/*
 * process_recv
 *	handle ni completion queue
 */
void *process_recv_rdma_thread(void *arg)
{
	ni_t *ni = arg;
	int num_wc = get_param(PTL_WC_COUNT);
	int num_buf;
	int i;
	struct ibv_wc wc_list[num_wc];
	int state;
	buf_t *buf_list[num_wc];
	buf_t *buf;

	while(!ni->rdma.catcher_stop) {
		num_buf = comp_poll(ni, num_wc, wc_list, buf_list);

		for (i = 0; i < num_buf; i++) {
			buf = buf_list[i];
			state = buf->state;

			while(1) {
				if (debug > 1)
					printf("tid:%x buf:%p: state = %s\n",
						pthread_self(), buf,
						recv_state_name[state]);
				switch (state) {
				case STATE_RECV_SEND_COMP:
					state = send_comp(buf);
					break;
				case STATE_RECV_RDMA_COMP:
					state = rdma_comp(buf);
					break;
				case STATE_RECV_PACKET:
					state = recv_packet(buf);
					break;
				case STATE_RECV_REQ:
					state = recv_req(buf);
					break;
				case STATE_RECV_INIT:
					state = recv_init(buf);
					break;
				case STATE_RECV_REPOST:
					state = recv_repost(buf);
					break;
				case STATE_RECV_DROP_BUF:
					state = recv_drop_buf(buf);
					break;
				case STATE_RECV_ERROR:
					if (buf) {
						buf_put(buf);
						ni->num_recv_errs++;
					}
					goto next_buf;
				case STATE_RECV_DONE:
					goto next_buf;
				}
			}
next_buf:
			continue;
		}
	}

	return NULL;
}

/*
 * process_recv
 *	handle ni completion queue for a buffer coming from shared memory.
 */
void process_recv_shmem(ni_t *ni, buf_t *buf)
{
	int state = STATE_RECV_PACKET;

	while(1) {
		if (debug) printf("%p: recv state local = %s\n", buf, recv_state_name[state]);
		switch (state) {
		case STATE_RECV_PACKET:
			state = recv_packet(buf);
			break;
		case STATE_RECV_REQ:
			state = recv_req(buf);
			break;
		case STATE_RECV_INIT:
			state = recv_init(buf);
			break;
		case STATE_RECV_DROP_BUF:
			state = recv_drop_buf(buf);
			break;
		case STATE_RECV_ERROR:
			if (buf) {
				buf_put(buf);
				ni->num_recv_errs++;
			}
			goto fail;

		case STATE_RECV_COMP_POLL:		/* COMP_POLL is an ending state for SHMEM. */
		case STATE_RECV_DONE:
			goto done;
		default:
			abort();
		}
	}

done:
	return;

fail:
	return;
}
