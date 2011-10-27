/*
 * ptl_init.c - initiator side processing
 */
#include "ptl_loc.h"

static char *init_state_name[] = {
	[STATE_INIT_START]		= "init_start",
	[STATE_INIT_PREP_REQ]		= "init_prep_req",
	[STATE_INIT_WAIT_CONN]		= "init_wait_conn",
	[STATE_INIT_SEND_REQ]		= "init_send_req",
	[STATE_INIT_WAIT_COMP]		= "init_wait_comp",
	[STATE_INIT_SEND_ERROR]		= "init_send_error",
	[STATE_INIT_EARLY_SEND_EVENT]	= "init_early_send_event",
	[STATE_INIT_WAIT_RECV]		= "init_wait_recv",
	[STATE_INIT_DATA_IN]		= "init_data_in",
	[STATE_INIT_LATE_SEND_EVENT]	= "init_late_send_event",
	[STATE_INIT_ACK_EVENT]		= "init_ack_event",
	[STATE_INIT_REPLY_EVENT]	= "init_reply_event",
	[STATE_INIT_CLEANUP]		= "init_cleanup",
	[STATE_INIT_ERROR]		= "init_error",
	[STATE_INIT_DONE]		= "init_done",
};

static void make_send_event(buf_t *buf)
{
	/* note: mlength and rem offset may or may not contain valid
	 * values depending on whether we have seen an ack/reply or not */
	if (buf->xi.ni_fail || !(buf->xi.event_mask & XI_PUT_SUCCESS_DISABLE_EVENT)) {
		make_init_event(buf, buf->xi.put_eq, PTL_EVENT_SEND, NULL);
	}

	buf->xi.event_mask &= ~XI_SEND_EVENT;
}

static void make_ack_event(buf_t *buf)
{
	if (buf->xi.ni_fail || !(buf->xi.event_mask & XI_PUT_SUCCESS_DISABLE_EVENT)) {
		make_init_event(buf, buf->xi.put_eq, PTL_EVENT_ACK, NULL);
	}

	buf->xi.event_mask &= ~XI_ACK_EVENT;
}

static void make_reply_event(buf_t *buf)
{
	if (buf->xi.ni_fail || !(buf->xi.event_mask & XI_GET_SUCCESS_DISABLE_EVENT)) {
		make_init_event(buf, buf->xi.get_eq, PTL_EVENT_REPLY, NULL);
	}

	buf->xi.event_mask &= ~XI_REPLY_EVENT;
}

static inline void make_ct_send_event(buf_t *buf)
{
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;

	make_ct_event(buf->xi.put_ct, buf->xi.ni_fail, le64_to_cpu(hdr->length), buf->xi.event_mask & XI_PUT_CT_BYTES);
	buf->xi.event_mask &= ~XI_CT_SEND_EVENT;
}

static inline void make_ct_ack_event(buf_t *buf)
{
	make_ct_event(buf->xi.put_ct, buf->xi.ni_fail, buf->xi.mlength, buf->xi.event_mask & XI_PUT_CT_BYTES);
	buf->xi.event_mask &= ~XI_CT_ACK_EVENT;
}

static inline void make_ct_reply_event(buf_t *buf)
{
	make_ct_event(buf->xi.get_ct, buf->xi.ni_fail, buf->xi.mlength, buf->xi.event_mask & XI_GET_CT_BYTES);
	buf->xi.event_mask &= ~XI_CT_REPLY_EVENT;
}

static void init_events(buf_t *buf)
{
	req_hdr_t *hdr = (req_hdr_t *)buf->data;

	if (buf->xi.put_md) {
		if (buf->xi.put_md->options & PTL_MD_EVENT_SUCCESS_DISABLE)
			buf->xi.event_mask |= XI_PUT_SUCCESS_DISABLE_EVENT;
		if (buf->xi.put_md->options & PTL_MD_EVENT_CT_BYTES)
			buf->xi.event_mask |= XI_PUT_CT_BYTES;
	}

	if (buf->xi.get_md) {
		if (buf->xi.get_md->options & PTL_MD_EVENT_SUCCESS_DISABLE)
			buf->xi.event_mask |= XI_GET_SUCCESS_DISABLE_EVENT;
		if (buf->xi.get_md->options & PTL_MD_EVENT_CT_BYTES)
			buf->xi.event_mask |= XI_GET_CT_BYTES;
	}

	switch (hdr->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (buf->xi.put_md->eq)
			buf->xi.event_mask |= XI_SEND_EVENT;

		if (buf->xi.put_md->eq && (hdr->ack_req == PTL_ACK_REQ))
			buf->xi.event_mask |= XI_ACK_EVENT | XI_RECEIVE_EXPECTED;

		if (buf->xi.put_md->ct &&
		    (buf->xi.put_md->options & PTL_MD_EVENT_CT_SEND))
			buf->xi.event_mask |= XI_CT_SEND_EVENT;

		if (buf->xi.put_md->ct && 
			(hdr->ack_req == PTL_CT_ACK_REQ || hdr->ack_req == PTL_OC_ACK_REQ) &&
		    (buf->xi.put_md->options & PTL_MD_EVENT_CT_ACK))
			buf->xi.event_mask |= XI_CT_ACK_EVENT | XI_RECEIVE_EXPECTED;
		break;
	case OP_GET:
		buf->xi.event_mask |= XI_RECEIVE_EXPECTED;
		if (buf->xi.get_md->eq)
			buf->xi.event_mask |= XI_REPLY_EVENT;

		if (buf->xi.get_md->ct &&
		    (buf->xi.get_md->options & PTL_MD_EVENT_CT_REPLY))
			buf->xi.event_mask |= XI_CT_REPLY_EVENT;
		break;
	case OP_FETCH:
	case OP_SWAP:
		if (buf->xi.put_md->eq)
			buf->xi.event_mask |= XI_SEND_EVENT;

		buf->xi.event_mask |= XI_REPLY_EVENT | XI_RECEIVE_EXPECTED;

		if (buf->xi.put_md->ct &&
		    (buf->xi.put_md->options & PTL_MD_EVENT_CT_SEND)) {
			buf->xi.event_mask |= XI_CT_SEND_EVENT;
		}

		if (buf->xi.get_md->ct &&
		    (buf->xi.get_md->options & PTL_MD_EVENT_CT_REPLY)) {
			buf->xi.event_mask |= XI_CT_REPLY_EVENT;
		}
		break;
	default:
		WARN();
		break;
	}
}

static int init_start(buf_t *buf)
{
	init_events(buf);

	return STATE_INIT_PREP_REQ;
}

/* Prepare a request. */
static int init_prep_req(buf_t *buf)
{
	int err;
	ni_t *ni = obj_to_ni(buf);
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	data_t *put_data = NULL;
	ptl_size_t length = le64_to_cpu(hdr->length);

	assert(buf->xi.conn);

	hdr->version = PTL_HDR_VER_1;
	hdr->ni_type = ni->ni_type;
	hdr->pkt_fmt = PKT_FMT_REQ;
	hdr->dst_nid = cpu_to_le32(buf->xi.target.phys.nid);
	hdr->dst_pid = cpu_to_le32(buf->xi.target.phys.pid);
	hdr->src_nid = cpu_to_le32(ni->id.phys.nid);
	hdr->src_pid = cpu_to_le32(ni->id.phys.pid);
	hdr->hdr_size = sizeof(req_hdr_t);
	hdr->handle		= cpu_to_le64(buf_to_handle(buf));

	buf->length = sizeof(req_hdr_t);
	buf->dest = &buf->xi.dest;

	switch (hdr->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		hdr->data_in = 0;
		hdr->data_out = 1;

		put_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(buf->xi.put_md, DATA_DIR_OUT, buf->xi.put_offset,
							   length, buf, buf->xi.conn->transport.type);
		if (err)
			goto error;
		break;

	case OP_GET:
		hdr->data_in = 1;
		hdr->data_out = 0;

		err = append_init_data(buf->xi.get_md, DATA_DIR_IN, buf->xi.get_offset,
							   length, buf, buf->xi.conn->transport.type);
		if (err)
			goto error;
		break;

	case OP_FETCH:
	case OP_SWAP:
		hdr->data_in = 1;
		hdr->data_out = 1;

		err = append_init_data(buf->xi.get_md, DATA_DIR_IN, buf->xi.get_offset,
							   length, buf, buf->xi.conn->transport.type);
		if (err)
			goto error;

		put_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(buf->xi.put_md, DATA_DIR_OUT, buf->xi.put_offset,
							   length, buf, buf->xi.conn->transport.type);
		if (err)
			goto error;
		break;

	default:
		/* is never supposed to happen */
		abort();
		break;
	}

	/* Always ask for a response if the remote will do an RDMA
	 * operation for the Put. Until then the response is received, we
	 * cannot free the MR nor post the send events. */
	if ((put_data && put_data->data_fmt != DATA_FMT_IMMEDIATE &&
		 (buf->xi.event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))) ||
		buf->num_mr) {
		hdr->ack_req = PTL_ACK_REQ;
		buf->xi.event_mask |= XI_RECEIVE_EXPECTED;
	}

	/* If we want an event, then do not request a completion for
	 * that message. It will be freed when we receive the ACK or
	 * reply. */
	buf->signaled = put_data && (put_data->data_fmt == DATA_FMT_IMMEDIATE) &&
	    (buf->xi.event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT));

	return STATE_INIT_WAIT_CONN;

 error:
	buf_put(buf);
	return STATE_INIT_ERROR;
}

static int wait_conn(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	conn_t *conn = buf->xi.conn;

	/* note once connected we don't go back */
	if (conn->state >= CONN_STATE_CONNECTED)
		goto out;

	/* if not connected. Add the xt on the pending list. It will be
	 * retried once connected/disconnected. */
	pthread_mutex_lock(&conn->mutex);
	if (conn->state < CONN_STATE_CONNECTED) {
		pthread_spin_lock(&conn->wait_list_lock);
		list_add_tail(&buf->list, &conn->buf_list);
		pthread_spin_unlock(&conn->wait_list_lock);

		if (conn->state == CONN_STATE_DISCONNECTED) {
			if (init_connect(ni, conn)) {
				pthread_mutex_unlock(&conn->mutex);
				pthread_spin_lock(&conn->wait_list_lock);
				list_del(&buf->list);
				pthread_spin_unlock(&conn->wait_list_lock);
				return STATE_INIT_ERROR;
			}
		}

		pthread_mutex_unlock(&conn->mutex);
		return STATE_INIT_WAIT_CONN;
	}
	pthread_mutex_unlock(&conn->mutex);

out:
#ifdef USE_XRC
	if (conn->state == CONN_STATE_XRC_CONNECTED)
		set_buf_dest(buf, conn->main_connect);
	else
#endif
		set_buf_dest(buf, conn);

	return STATE_INIT_SEND_REQ;
}

static int init_send_req(buf_t *buf)
{
	int err;
	int signaled = buf->signaled;

	err = buf->xi.conn->transport.send_message(buf, signaled);
	if (err)
		return STATE_INIT_SEND_ERROR;

	if (signaled) {
		if (buf->xi.conn->transport.type == CONN_TYPE_RDMA)
			return STATE_INIT_WAIT_COMP;
		else
			return STATE_INIT_EARLY_SEND_EVENT;
	}
	else if (buf->xi.event_mask & XI_RECEIVE_EXPECTED)
		return STATE_INIT_WAIT_RECV;
	else
		return STATE_INIT_CLEANUP;
}

static int init_send_error(buf_t *buf)
{
	buf->xi.ni_fail = PTL_NI_UNDELIVERABLE;

	if (buf->xi.event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;
	else if (buf->xi.event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;
	else if (buf->xi.event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	else
		return STATE_INIT_CLEANUP;
}

/* Wait for an IB completion event. We can get here either with send
 * completion (most of the time) or with a receive completion related
 * to the ack/reply (rarely). */
static int init_wait_comp(buf_t *buf)
{
	if (buf->xi.completed || buf->xi.recv_buf) {
		return STATE_INIT_EARLY_SEND_EVENT;
	} else {
		return STATE_INIT_WAIT_COMP;
	}
}

static int early_send_event(buf_t *buf)
{
	/* Release the MD before posting the SEND event. */
	md_put(buf->xi.put_md);
	buf->xi.put_md = NULL;

	if (buf->xi.event_mask & XI_SEND_EVENT)
		make_send_event(buf);

	if (buf->xi.event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(buf);
	
	if ((buf->xi.event_mask & XI_RECEIVE_EXPECTED) && !buf->xi.ni_fail)
		return STATE_INIT_WAIT_RECV;
	else
		return STATE_INIT_CLEANUP;
}

static int wait_recv(buf_t *buf)
{
	hdr_t *hdr;

	/* We can come here on the application or the progress thread, but
	 * we need the receive buffer to make progress. */
	if (!buf->xi.recv_buf)
		return STATE_INIT_WAIT_RECV;

	hdr = (hdr_t *)buf->xi.recv_buf->data;

	/* get returned fields */
	buf->xi.ni_fail = hdr->ni_fail;
	buf->xi.mlength = le64_to_cpu(hdr->length);
	buf->xi.moffset = le64_to_cpu(hdr->offset);

	if (debug) buf_dump(buf->xi.recv_buf);

	/* Check for short immediate reply data. */
	if (buf->xi.data_in && buf->xi.get_md)
		return STATE_INIT_DATA_IN;

	if (buf->xi.event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;

	if (buf->xi.event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;

	if (buf->xi.event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	
	return STATE_INIT_CLEANUP;
}

/*
 * init_copy_in
 *	copy data from data segment to md
 */
static int init_copy_in(buf_t *buf, md_t *md, void *data)
{
	int err;

	/* the offset into the get_md is the original one */
	ptl_size_t offset = buf->xi.get_offset;

	/* the length may have been truncated by target */
	ptl_size_t length = buf->xi.mlength;

	assert(length <= buf->xi.get_resid);

	if (md->num_iov) {
		void *start;
		err = iov_copy_in(data, (ptl_iovec_t *)md->start,
				  md->num_iov, offset, length, &start);
		if (err)
			return PTL_FAIL;
	} else {
		memcpy(md->start + offset, data, length);
	}

	return PTL_OK;
}

/* we are here because the response packet contains data */
static int data_in(buf_t *buf)
{
	int err;
	md_t *md = buf->xi.get_md;
	data_t *data = buf->xi.data_in;

	switch (data->data_fmt) {
	case DATA_FMT_IMMEDIATE:
		err = init_copy_in(buf, md, data->immediate.data);
		if (err)
			return STATE_TGT_ERROR;
		break;
	default:
		printf("unexpected data in format\n");
		break;
	}

	if (buf->xi.event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;

	if (buf->xi.event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	
	return STATE_INIT_CLEANUP;
}

static int late_send_event(buf_t *buf)
{
	/* Release the MD before posting the SEND event. */
	md_put(buf->xi.put_md);
	buf->xi.put_md = NULL;

	if (buf->xi.event_mask & XI_SEND_EVENT)
		make_send_event(buf);

	if (buf->xi.event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(buf);
	
	if (buf->xi.ni_fail == PTL_NI_UNDELIVERABLE)
		return STATE_INIT_CLEANUP;
	else if (buf->xi.event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;
	else if (buf->xi.event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	else
		return STATE_INIT_CLEANUP;
}

static int ack_event(buf_t *buf)
{
	hdr_t *hdr = (hdr_t *)buf->xi.recv_buf->data;

	/* Release the MD before posting the ACK event. */
	if (buf->xi.put_md) {
		md_put(buf->xi.put_md);
		buf->xi.put_md = NULL;
	}

	if (hdr->operation != OP_NO_ACK) {
		if (buf->xi.event_mask & XI_ACK_EVENT)
			make_ack_event(buf);

		if (buf->xi.event_mask & XI_CT_ACK_EVENT)
			make_ct_ack_event(buf);
	}
	
	return STATE_INIT_CLEANUP;
}

static int reply_event(buf_t *buf)
{
	/* Release the MD before posting the REPLY event. */
	md_put(buf->xi.get_md);
	buf->xi.get_md = NULL;

	if ((buf->xi.event_mask & XI_REPLY_EVENT) &&
		buf->xi.get_eq)
		make_reply_event(buf);

	if (buf->xi.event_mask & XI_CT_REPLY_EVENT)
		make_ct_reply_event(buf);
	
	return STATE_INIT_CLEANUP;
}

static void init_cleanup(buf_t *buf)
{
	if (buf->xi.get_md) {
		md_put(buf->xi.get_md);
		buf->xi.get_md = NULL;
	}

	if (buf->xi.put_md) {
		md_put(buf->xi.put_md);
		buf->xi.put_md = NULL;
	}

	if (buf->xi.recv_buf) {
		buf_put(buf->xi.recv_buf);
		buf->xi.recv_buf = NULL;
	}
}

/*
 * process_init
 *	this can run on the API or progress threads
 *	calling process_init will guarantee that the
 *	loop will run at least once either on the calling
 *	thread or the currently active thread
 */
int process_init(buf_t *buf)
{
	int err = PTL_OK;
	enum init_state state;

	do {
		pthread_mutex_lock(&buf->xi.mutex);

		state = buf->xi.state;

		while (1) {
			if (debug) printf("%p: init state = %s\n",
					  buf, init_state_name[state]);

			switch (state) {
			case STATE_INIT_START:
				state = init_start(buf);
				break;
			case STATE_INIT_PREP_REQ:
				state = init_prep_req(buf);	
				break;
			case STATE_INIT_WAIT_CONN:
				state = wait_conn(buf);
				if (state == STATE_INIT_WAIT_CONN)
					goto exit;
				break;
			case STATE_INIT_SEND_REQ:
				state = init_send_req(buf);
				if (state == STATE_INIT_WAIT_COMP) {
					/* Never finish that on application thread, 
					 * else a race with the receive thread will occur. */
					goto exit;
				}
				break;
			case STATE_INIT_WAIT_COMP:
				state = init_wait_comp(buf);
				if (state == STATE_INIT_WAIT_COMP) {
					goto exit;
				}
				break;
			case STATE_INIT_SEND_ERROR:
				state = init_send_error(buf);
				break;
			case STATE_INIT_EARLY_SEND_EVENT:
				state = early_send_event(buf);
				break;
			case STATE_INIT_WAIT_RECV:
				state = wait_recv(buf);
				if (state == STATE_INIT_WAIT_RECV)
					/* Nothing received yet. */
					goto exit;
				break;
			case STATE_INIT_DATA_IN:
				state = data_in(buf);
				break;
			case STATE_INIT_LATE_SEND_EVENT:
				state = late_send_event(buf);
				break;
			case STATE_INIT_ACK_EVENT:
				state = ack_event(buf);
				break;
			case STATE_INIT_REPLY_EVENT:
				state = reply_event(buf);
				break;

			case STATE_INIT_ERROR:
				err = PTL_FAIL;
				state = STATE_INIT_CLEANUP;
				break;

			case STATE_INIT_CLEANUP:
				init_cleanup(buf);
				buf->xi.state = STATE_INIT_DONE;
				pthread_mutex_unlock(&buf->xi.mutex);
				buf_put(buf);
				return err;
				break;

			case STATE_INIT_DONE:
				/* We reach that state only if the send completion is
				 * received after the recv completion. */
				goto exit;
			default:
				abort();
			}
		}
exit:
		buf->xi.state = state;

		pthread_mutex_unlock(&buf->xi.mutex);
	} while(0);

	return err;
}
