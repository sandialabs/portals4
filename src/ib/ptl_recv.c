/**
 * @file ptl_recv.c
 *
 * Completion queue processing.
 */
#include "ptl_loc.h"

/**
 * Receive state name for debug output.
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

/**
 * Poll the rdma completion queue.
 *
 * @param ni the ni that owns the cq.
 * @param num_wc the number of entries in wc_list and buf_list.
 * @param wc_list an array of work completion structs.
 * @param buf_list an array of buf pointers.
 *
 * @return the number of work completions found if no error.
 * @return a negative number if an error occured.
 */
static int comp_poll(ni_t *ni, int num_wc,
		     struct ibv_wc wc_list[], buf_t *buf_list[])
{
	int ret;
	int i;
	buf_t *buf;

	ret = ibv_poll_cq(ni->rdma.cq, num_wc, wc_list);
	if (ret <= 0)
		return 0;

	/* convert from wc to buf and set initial state */
	for (i = 0; i < ret; i++) {
		const struct ibv_wc *wc = &wc_list[i];

		buf = (buf_t *)(uintptr_t)wc->wr_id;
		buf->length = wc->byte_len;

		if (unlikely(wc->status)) {
			if (buf->type == BUF_SEND) {
				buf->xi.ni_fail = PTL_NI_UNDELIVERABLE;
				buf->state = STATE_RECV_SEND_COMP;
			} else if (buf->type == BUF_RDMA)
				buf->state = STATE_RECV_ERROR;
			else
				buf->state = STATE_RECV_DROP_BUF;
		} else {
			if (buf->type == BUF_SEND)
				buf->state = STATE_RECV_SEND_COMP;
			else if (buf->type == BUF_RDMA)
				buf->state = STATE_RECV_RDMA_COMP;
			else if (buf->type == BUF_RECV)
				buf->state = STATE_RECV_PACKET;
			else
				buf->state = STATE_RECV_ERROR;
		}

		buf_list[i] = buf;
	}

	return ret;
}

/**
 * Process a send completion.
 *
 * @param buf the buffer that finished.
 *
 * @return next state
 */
static int send_comp(buf_t *buf)
{
	/* this should only happen if we requested a completion */
	assert(buf->comp || buf->xi.ni_fail == PTL_NI_UNDELIVERABLE);

	/* Fox XI only, restart the initiator state machine. */
	if (!buf->xt) {
		buf->xi.completed = 1;
		process_init(buf);
	} else {
		/* free the buffer */
		buf_put(buf);
	}

	if (buf->comp)
		buf_put(buf);

	return STATE_RECV_DONE;
}

/**
 * Process a read/write completion.
 *
 * @param buf the buffer that finished.
 *
 * @return the next state.
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

/**
 * Process a received buffer.
 *
 * @param buf the receive buffer that finished.
 *
 * @return the next state.
 */
static int recv_packet(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	hdr_t *hdr = (hdr_t *)buf->data;

	/* keep track of the number of buffers posted to the srq */
	(void)__sync_sub_and_fetch(&ni->rdma.num_posted_recv, 1);

	/* remove buf from pending receive list */
	assert(!list_empty(&buf->list));

	pthread_spin_lock(&ni->rdma.recv_list_lock);
	list_del(&buf->list);
	pthread_spin_unlock(&ni->rdma.recv_list_lock);

	/* sanity check received buffer */
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

	/* compute next state */
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

/**
 * Process a new request to target.
 *
 * @param buf the received buffer.
 *
 * @return the next state.
 */
static int recv_req(buf_t *buf)
{
	int err;
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	ni_t *ni = obj_to_ni(buf);
	xt_t *xt;

	/* allocate a new xt to track the message */
	err = xt_alloc(ni, &xt);
	if (err) {
		WARN();
		return STATE_RECV_ERROR;
	}

	/* unpack message header */
	xport_hdr_to_xt((hdr_t *)hdr, xt);
	base_hdr_to_xt((hdr_t *)hdr, xt);
	req_hdr_to_xt(hdr, xt);
	xt->operation = hdr->operation;

	/* compute the data segments in the message
	 * note req packet data direction is wrt init,
	 * xt direction is wrt tgt */
	if (hdr->data_in)
		xt->data_out = (data_t *)(buf->data + sizeof(*hdr));

	if (hdr->data_out)
		xt->data_in = (data_t *)(buf->data + sizeof(*hdr) +
					  data_size(xt->data_out));

	xt->recv_buf = buf;
	xt->state = STATE_TGT_START;

	/* Send message to target state machine. process_tgt must drop the
	 * buffer, so buf will not be valid after the call. */
	err = process_tgt(xt);
	if (err)
		WARN();

	return STATE_RECV_REPOST;
}

/**
 * Process a response message to initiator.
 *
 * @param buf the message received.
 *
 * @return the next state.
 */
static int recv_init(buf_t *buf)
{
	int err;
	buf_t *init_buf;
	hdr_t *hdr = (hdr_t *)buf->data;

	/* lookup the xi handle to get original xi */
	err = to_buf(le64_to_cpu(hdr->handle), &init_buf);
	if (err) {
		WARN();
		return STATE_RECV_DROP_BUF;
	}

	/* compute data segments in response message */
	if (hdr->data_in)
		init_buf->xi.data_out = (data_t *)(buf->data + hdr->hdr_size);

	if (hdr->data_out)
		init_buf->xi.data_in = (data_t *)(buf->data + hdr->hdr_size +
					  data_size(init_buf->xi.data_out));

	init_buf->xi.recv_buf = buf;

	/* Note: process_init must drop recv_buf, so buf will not be valid
	 * after the call. */
	err = process_init(init_buf);
	if (err)
		WARN();

	buf_put(init_buf);					/* from to_xi() */

	return STATE_RECV_REPOST;
}

/**
 * Repost receive buffers to srq.
 *
 * @param buf the received buffer.
 *
 * @return the next state.
 */
static int recv_repost(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	int num_bufs;

	/* compute the available room in the srq */
	num_bufs = (get_param(PTL_MAX_SRQ_RECV_WR) - ni->rdma.num_posted_recv);

	/* if rooms exceeds threshold repost that many buffers
	 * this should reduce the number of receive queue doorbells
	 * which should improve performance */
	if (num_bufs > get_param(PTL_SRQ_REPOST_SIZE))
		ptl_post_recv(ni, get_param(PTL_SRQ_REPOST_SIZE));

	return STATE_RECV_DONE;
}

/**
 * Drop the received buffer.
 *
 * @param buf the completed buffer.
 *
 * @return the next state.
 */
static int recv_drop_buf(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);

	buf_put(buf);
	ni->num_recv_drops++;

	return STATE_RECV_REPOST;
}

/**
 * Completion queue polling thread.
 *
 * @param arg opaque pointer to ni.
 */
void *process_recv_rdma_thread(void *arg)
{
	ni_t *ni = arg;
	const int num_wc = get_param(PTL_WC_COUNT);
	struct ibv_wc wc_list[num_wc];
	buf_t *buf_list[num_wc];

	while(!ni->rdma.catcher_stop) {
		const int num_buf = comp_poll(ni, num_wc, wc_list, buf_list);
		int i;
		for (i = 0; i < num_buf; i++) {
			buf_t *buf = buf_list[i];	
			enum recv_state state = buf->state;

			while(1) {
				if (debug > 1)
					printf("tid:%lx buf:%p: state = %s\n",
						   pthread_self(), buf, recv_state_name[state]);
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

/**
 * Process a received message in shared memory.
 *
 * @param ni the ni to poll.
 * @param buf the received buffer.
 */
void process_recv_shmem(ni_t *ni, buf_t *buf)
{
	enum recv_state state = STATE_RECV_PACKET;

	while(1) {
		if (debug)
			printf("tid:%lx buf:%p: recv state local = %s\n",
				   pthread_self(), buf,
				   recv_state_name[state]);
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
			goto exit;
		case STATE_RECV_REPOST:
		case STATE_RECV_DONE:
			goto exit;

		case STATE_RECV_SEND_COMP:
		case STATE_RECV_RDMA_COMP:
			/* Not reachable. */
			abort();
		}
	}
exit:
	return;
}
