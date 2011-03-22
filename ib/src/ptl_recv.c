/*
 * ptl_recv.c - initial receive processing
 */
#include "ptl_loc.h"

/*
 * recv_state_name
 *	for debugging output
 */
static char *recv_state_name[] = {
	[STATE_RECV_COMP_WAIT]		= "comp_wait",
	[STATE_RECV_SEND_COMP]		= "send_comp",
	[STATE_RECV_RDMA_COMP]		= "rdma_comp",
	[STATE_RECV_PACKET]		= "recv_packet",
	[STATE_RECV_DROP_BUF]		= "recv_drop_buf",
	[STATE_RECV_REQ]		= "recv_req",
	[STATE_RECV_INIT]		= "recv_init",
	[STATE_RECV_ERROR]		= "recv_error",
	[STATE_RECV_DONE]		= "recv_done",
};

/*
 * comp_wait
 *	wait for a send or recv to complete
 */
static int comp_wait(ni_t *ni, buf_t **buf_p)
{
	int err;
	struct ibv_cq *cq;
	void *unused;
	struct ibv_wc wc;
	int n;
	buf_t *buf;

	err = ibv_get_cq_event(ni->ch, &cq, &unused);
	if (err) {
		WARN();
		return STATE_RECV_ERROR;
	}

	/* todo: coalesce acks. ibv_ack_cq_events is costly. see man page. */
	ibv_ack_cq_events(ni->cq, 1);

	if (cq != ni->cq) {
		WARN();
		return STATE_RECV_ERROR;
	}

	if (ibv_req_notify_cq(ni->cq, 0)) {
		ptl_warn("unable to req notify\n");
		WARN();
		return STATE_RECV_ERROR;
	}

	n = ibv_poll_cq(ni->cq, 1, &wc);
	if (n <= 0) {
		WARN();
		return STATE_RECV_ERROR;
	}

	assert(n == 1);

	if (wc.wr_id == 0) {
		/* No buffer with intermediate error completion */
		WARN();
		return STATE_RECV_ERROR;
	}

	buf = (buf_t *)(uintptr_t)wc.wr_id;
	*buf_p = buf;

	if (debug)
		printf("rank %d: comp_wait - wc.status(%d), wc.length(%d)\n",
			   ni->gbl->rank, (int) wc.status, (int) wc.byte_len);


	if (wc.status == IBV_WC_WR_FLUSH_ERR)
		return STATE_RECV_DROP_BUF;

	if (wc.status != IBV_WC_SUCCESS) {
		if (debug) printf("error completion\n");
		return STATE_RECV_ERROR;
	}

	buf->length = wc.byte_len;

	if (buf->type == BUF_SEND) {
		if (debug) printf("received a send wc\n");
		return STATE_RECV_SEND_COMP;
	} else if (buf->type == BUF_RECV) {
		if (debug) printf("received a recv wc\n");
		post_recv(ni);
		return STATE_RECV_PACKET;
	} else if (buf->type == BUF_RDMA) {
		if (debug) printf("received a send RDMA wc\n");
		return STATE_RECV_RDMA_COMP;
	} else {
		if (debug) printf("received a bogus wc\n");
		return STATE_RECV_ERROR;
	}
}

static int send_comp(buf_t *send_buf)
{
	int err;
	ni_t *ni = to_ni(send_buf);

	pthread_spin_lock(&ni->send_list_lock);
	list_del(&send_buf->list);
	pthread_spin_unlock(&ni->send_list_lock);

	if (send_buf->xi) {
		send_buf->xi->send_buf = NULL;
		err = process_init(send_buf->xi);
		if (err) {
			WARN();
			return STATE_RECV_ERROR;
		}
	}
	if (send_buf->xt) {
		send_buf->xt->send_buf = NULL;
		err = process_tgt(send_buf->xt);
		if (err) {
			WARN();
			return STATE_RECV_ERROR;
		}
	}

	buf_put(send_buf);
	return STATE_RECV_DONE;
}

/*
 * rdma_comp
 *	received completion on target initiated RDMA
 */
static int rdma_comp(buf_t *rdma_buf)
{
	int err;
	ni_t *ni = to_ni(rdma_buf);

	pthread_spin_lock(&ni->send_list_lock);
	list_del(&rdma_buf->list);
	pthread_spin_unlock(&ni->send_list_lock);

	if (rdma_buf->xt) {
		rdma_buf->xt->rdma_comp--;
		err = process_tgt(rdma_buf->xt);
		if (err) {
			WARN();
			return STATE_RECV_ERROR;
		}
	}
	return STATE_RECV_DONE;
}

/*
 * recv_packet
 *	process a new packet
 */
static int recv_packet(buf_t *buf)
{
	ni_t *ni = to_ni(buf);
	hdr_t *hdr = (hdr_t *)buf->data;

	pthread_spin_lock(&ni->recv_list_lock);
	list_del(&buf->list);
	pthread_spin_unlock(&ni->recv_list_lock);

#if 0
int i;
for (i = 0; i < buf->length; i++) {
	if (i % 8 == 0)
	printf("%04x: ", i);
	printf(" %02x", buf->data[i]);
	if (i % 8 == 7)
	printf("\n");
}
printf("\n");
#endif

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
	ni_t *ni = to_ni(buf);
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

	/* req packet data dir is wrt init, xt is wrt tgt */
	if (hdr->data_in)
		xt->data_out = (data_t *)(buf->data + sizeof(*hdr));

	if (hdr->data_out)
		xt->data_in = (data_t *)(buf->data + sizeof(*hdr) +
					  data_size(xt->data_out));

	xt->recv_buf = buf;
	xt->state = STATE_TGT_START;
	err = process_tgt(xt);
	if (err)
		WARN();

	return STATE_RECV_DONE;
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

	err = xi_get(be64_to_cpu(hdr->handle), &xi);
	if (err) {
		WARN();
		return STATE_RECV_DROP_BUF;
	}

	xi->recv_buf = buf;

	err = process_init(xi);
	if (err)
		WARN();

	return STATE_RECV_DONE;
}

/*
 * process_recv
 *	process an incoming packet
 */
void process_recv(EV_P_ ev_io *w, int revents)
{
	ni_t *ni = w->data;
	int state = STATE_RECV_COMP_WAIT;
	buf_t *buf = NULL;

	while(1) {
		if (debug) printf("recv state = %s\n", recv_state_name[state]);
		switch (state) {
		case STATE_RECV_COMP_WAIT:
			state = comp_wait(ni, &buf);
			break;
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
		case STATE_RECV_DROP_BUF:
			buf_put(buf);
			ni->num_recv_drops++;
			return PTL_OK;
		case STATE_RECV_ERROR:
			buf_put(buf);
			ni->num_recv_errs++;
			return PTL_FAIL;
		case STATE_RECV_DONE:
			return PTL_OK;
		}
	}
}
