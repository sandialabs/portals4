/*
 * ptl_init.c - initiator side processing
 */
#include "ptl_loc.h"

/*
 * send_message
 *	send a message to remote end
 */
int send_message_rdma(buf_t *buf, int signaled)
{
	int err;
	struct ibv_send_wr *bad_wr;

	buf->rdma.send_wr.opcode = IBV_WR_SEND;
	if (signaled) {
		buf->rdma.send_wr.send_flags = IBV_SEND_SIGNALED;

		/* Keep the buffer from being freed until we get the
		 * completion. */
		buf_get(buf);
	} else {
		buf->rdma.send_wr.send_flags = 0;
	}
	buf->rdma.sg_list[0].length = buf->length;
	buf->type = BUF_SEND;
#ifdef USE_XRC
	buf->send_wr.xrc_remote_srq_num = buf->dest.xrc_remote_srq_num;
#endif

	buf->comp = signaled;

	err = ibv_post_send(buf->dest.rdma.qp, &buf->rdma.send_wr, &bad_wr);
	if (err) {
		WARN();

		return PTL_FAIL;
	}

	return PTL_OK;
}
