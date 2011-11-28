/*
 * ptl_init.c - initiator side processing
 */
#include "ptl_loc.h"

/*
 * send_message
 *	send a message to remote end
 */
int send_message_rdma(buf_t *buf)
{
	int err;
	struct ibv_send_wr *bad_wr;
	struct ibv_send_wr wr;
	struct ibv_sge sg_list;

	wr.wr_id = (uintptr_t)buf;
	wr.next = NULL;
	wr.sg_list = &sg_list;
	wr.num_sge = 1;
	wr.opcode = IBV_WR_SEND;
	if ((buf->event_mask & XX_SIGNALED) || 
		atomic_inc(&buf->conn->rdma.completion_threshold) == get_param(PTL_MAX_SEND_COMP_THRESHOLD)) {
		wr.send_flags = IBV_SEND_SIGNALED;
		atomic_set(&buf->conn->rdma.completion_threshold, 0);

		/* Keep the buffer from being freed until we get the
		 * completion. */
		buf_get(buf);
	} else {
		wr.send_flags = 0;
	}

	if (buf->event_mask & XX_INLINE) {
		wr.send_flags |= IBV_SEND_INLINE;

		if (wr.send_flags == IBV_SEND_INLINE) {
			/* Inline and no completion required: fire and forget. If
			   there is an error, we will get a completion anyway, so
			   we must ignore it. */
			wr.wr_id = 0;
		}
	}

	sg_list.addr = (uintptr_t)buf->internal_data;
	sg_list.lkey = buf->rdma.lkey;
	sg_list.length = buf->length;

#ifdef USE_XRC
	wr.xrc_remote_srq_num = buf->dest.xrc_remote_srq_num;
#endif
	
	buf->type = BUF_SEND;

	err = ibv_post_send(buf->dest.rdma.qp, &wr, &bad_wr);
	if (err) {
		WARN();

		return PTL_FAIL;
	}

	return PTL_OK;
}
