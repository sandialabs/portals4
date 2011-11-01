/*
 * ptl_init.c - initiator side processing
 */
#include "ptl_loc.h"

static char *init_state_name[] = {
	[STATE_INIT_START]		= "start",
	[STATE_INIT_PREP_REQ]		= "prepare_req",
	[STATE_INIT_WAIT_CONN]		= "wait_conn",
	[STATE_INIT_SEND_REQ]		= "send_req",
	[STATE_INIT_WAIT_COMP]		= "wait_comp",
	[STATE_INIT_SEND_ERROR]		= "send_error",
	[STATE_INIT_EARLY_SEND_EVENT]	= "early_send_event",
	[STATE_INIT_WAIT_RECV]		= "wait_recv",
	[STATE_INIT_DATA_IN]		= "data_in",
	[STATE_INIT_LATE_SEND_EVENT]	= "late_send_event",
	[STATE_INIT_ACK_EVENT]		= "ack_event",
	[STATE_INIT_REPLY_EVENT]	= "reply_event",
	[STATE_INIT_CLEANUP]		= "cleanup",
	[STATE_INIT_ERROR]		= "error",
	[STATE_INIT_DONE]		= "done",
};

static void make_send_event(buf_t *buf)
{
	/* note: mlength and rem offset may or may not contain valid
	 * values depending on whether we have seen an ack/reply or not */
	if (buf->ni_fail || !(buf->event_mask & XI_PUT_SUCCESS_DISABLE_EVENT))
		make_init_event(buf, buf->put_eq, PTL_EVENT_SEND, NULL);

	buf->event_mask &= ~XI_SEND_EVENT;
}

static void make_ack_event(buf_t *buf)
{
	if (buf->ni_fail || !(buf->event_mask & XI_PUT_SUCCESS_DISABLE_EVENT))
		make_init_event(buf, buf->put_eq, PTL_EVENT_ACK, NULL);

	buf->event_mask &= ~XI_ACK_EVENT;
}

static void make_reply_event(buf_t *buf)
{
	if (buf->ni_fail || !(buf->event_mask & XI_GET_SUCCESS_DISABLE_EVENT))
		make_init_event(buf, buf->get_eq, PTL_EVENT_REPLY, NULL);

	buf->event_mask &= ~XI_REPLY_EVENT;
}

static inline void make_ct_send_event(buf_t *buf)
{
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;

	make_ct_event(buf->put_ct, buf->ni_fail, le64_to_cpu(hdr->length),
		      buf->event_mask & XI_PUT_CT_BYTES);

	buf->event_mask &= ~XI_CT_SEND_EVENT;
}

static inline void make_ct_ack_event(buf_t *buf)
{
	make_ct_event(buf->put_ct, buf->ni_fail, buf->mlength,
		      buf->event_mask & XI_PUT_CT_BYTES);

	buf->event_mask &= ~XI_CT_ACK_EVENT;
}

static inline void make_ct_reply_event(buf_t *buf)
{
	make_ct_event(buf->get_ct, buf->ni_fail, buf->mlength,
		      buf->event_mask & XI_GET_CT_BYTES);

	buf->event_mask &= ~XI_CT_REPLY_EVENT;
}

/**
 * @brief initiator start state.
 *
 * This state analyzes the request and the options
 * for the md and determines the buf event mask.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int start(buf_t *buf)
{
	req_hdr_t *hdr = (req_hdr_t *)buf->data;

	buf->event_mask = 0;

	if (buf->put_md) {
		if (buf->put_md->options & PTL_MD_EVENT_SUCCESS_DISABLE)
			buf->event_mask |= XI_PUT_SUCCESS_DISABLE_EVENT;

		if (buf->put_md->options & PTL_MD_EVENT_CT_BYTES)
			buf->event_mask |= XI_PUT_CT_BYTES;
	}

	if (buf->get_md) {
		if (buf->get_md->options & PTL_MD_EVENT_SUCCESS_DISABLE)
			buf->event_mask |= XI_GET_SUCCESS_DISABLE_EVENT;

		if (buf->get_md->options & PTL_MD_EVENT_CT_BYTES)
			buf->event_mask |= XI_GET_CT_BYTES;
	}

	switch (hdr->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (buf->put_md->eq)
			buf->event_mask |= XI_SEND_EVENT;

		if (hdr->ack_req == PTL_ACK_REQ) {
			buf->event_mask |= XI_RECEIVE_EXPECTED;
			if (buf->put_md->eq)
				buf->event_mask |= XI_ACK_EVENT;
		}

		if (buf->put_md->ct &&
		    (buf->put_md->options & PTL_MD_EVENT_CT_SEND))
			buf->event_mask |= XI_CT_SEND_EVENT;

		if ((hdr->ack_req == PTL_CT_ACK_REQ ||
		     hdr->ack_req == PTL_OC_ACK_REQ))
			buf->event_mask |= XI_RECEIVE_EXPECTED; {
			if (buf->put_md->ct && (buf->put_md->options & PTL_MD_EVENT_CT_ACK))
				buf->event_mask |= XI_CT_ACK_EVENT;
		}
		break;
	case OP_GET:
		buf->event_mask |= XI_RECEIVE_EXPECTED;

		if (buf->get_md->eq)
			buf->event_mask |= XI_REPLY_EVENT;

		if (buf->get_md->ct &&
		    (buf->get_md->options & PTL_MD_EVENT_CT_REPLY))
			buf->event_mask |= XI_CT_REPLY_EVENT;
		break;
	case OP_FETCH:
	case OP_SWAP:
		buf->event_mask |= XI_RECEIVE_EXPECTED;

		if (buf->put_md->eq)
			buf->event_mask |= XI_SEND_EVENT;

		if (buf->get_md->eq)
			buf->event_mask |= XI_REPLY_EVENT;

		if (buf->put_md->ct &&
		    (buf->put_md->options & PTL_MD_EVENT_CT_SEND))
			buf->event_mask |= XI_CT_SEND_EVENT;

		if (buf->get_md->ct &&
		    (buf->get_md->options & PTL_MD_EVENT_CT_REPLY))
			buf->event_mask |= XI_CT_REPLY_EVENT;
		break;
	default:
		WARN();
		abort();
		break;
	}

	return STATE_INIT_PREP_REQ;
}

/**
 * @brief initiator prepare request state.
 *
 * This state builds the request message
 * header and optional data descriptors.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int prepare_req(buf_t *buf)
{
	int err;
	ni_t *ni = obj_to_ni(buf);
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	data_t *put_data = NULL;
	ptl_size_t length = le64_to_cpu(hdr->length);

	assert(buf->conn);

	hdr->version = PTL_HDR_VER_1;
	hdr->ni_type = ni->ni_type;
	hdr->pkt_fmt = PKT_FMT_REQ;
	hdr->dst_nid = cpu_to_le32(buf->target.phys.nid);
	hdr->dst_pid = cpu_to_le32(buf->target.phys.pid);
	hdr->src_nid = cpu_to_le32(ni->id.phys.nid);
	hdr->src_pid = cpu_to_le32(ni->id.phys.pid);
	hdr->hdr_size = sizeof(req_hdr_t);
	hdr->handle = cpu_to_le32(buf_to_handle(buf));

	buf->length = sizeof(req_hdr_t);

	switch (hdr->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		hdr->data_in = 0;
		hdr->data_out = 1;

		put_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(buf->put_md, DATA_DIR_OUT,
				       buf->put_offset, length, buf,
				       buf->conn->transport.type);
		if (err)
			goto error;
		break;

	case OP_GET:
		hdr->data_in = 1;
		hdr->data_out = 0;

		err = append_init_data(buf->get_md, DATA_DIR_IN,
				       buf->get_offset, length, buf,
				       buf->conn->transport.type);
		if (err)
			goto error;
		break;

	case OP_FETCH:
	case OP_SWAP:
		hdr->data_in = 1;
		hdr->data_out = 1;

		err = append_init_data(buf->get_md, DATA_DIR_IN,
				       buf->get_offset, length, buf,
				       buf->conn->transport.type);
		if (err)
			goto error;

		put_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(buf->put_md, DATA_DIR_OUT,
				       buf->put_offset, length, buf,
				       buf->conn->transport.type);
		if (err)
			goto error;
		break;

	default:
		/* is never supposed to happen */
		abort();
		break;
	}

	/* Always ask for a response if the remote will do an RDMA
	 * operation for the Put. Until the response is received, we
	 * cannot free the MR nor post the send events. Note we
	 * have already set event_mask. */
	if ((put_data && put_data->data_fmt != DATA_FMT_IMMEDIATE &&
		 (buf->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))) ||
		buf->num_mr) {
		hdr->ack_req = PTL_ACK_REQ;
		buf->event_mask |= XI_RECEIVE_EXPECTED;
	}

	/* For immediate data we can cause an early send event provided
	 * we request a send completion event */
	buf->signaled = put_data && (put_data->data_fmt ==
				DATA_FMT_IMMEDIATE) && (buf->event_mask &
				(XI_SEND_EVENT | XI_CT_SEND_EVENT));

	/* if we are not already 'connected' to destination
	 * wait until we are */
	if (likely(buf->conn->state >= CONN_STATE_CONNECTED))
		return STATE_INIT_SEND_REQ;
	else
		return STATE_INIT_WAIT_CONN;

error:
	return STATE_INIT_ERROR;
}

/**
 * @brief initiator wait for connection state.
 *
 * This state is reached if the source and
 * destination are not 'connected'. For the
 * InfiniBand case an actual connection is
 * required. While waiting for a connection to
 * be established the buf is held on the
 * conn->buf_list and the buf which is running
 * on the application thread leaves the state
 * machine. The connection event is received on
 * the rdma_cm event thread and reenters the
 * state machine still in the same state.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int wait_conn(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	conn_t *conn = buf->conn;

	/* we return here if a connection completes
	 * so check again */
	if (buf->conn->state >= CONN_STATE_CONNECTED)
		return STATE_INIT_SEND_REQ;

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

	return STATE_INIT_SEND_REQ;
}

/**
 * @brief initiator send request state.
 *
 * This state sends the request to the
 * destination. Signaled is set if an early
 * send event is possible. For the InfiniBand
 * case a send completion event must be received.
 * For the shmem case when the send_message call
 * returns we can go directly to the send event.
 * Otherwise we must wait for a response message
 * (ack or reply) from the target or if no
 * events are going to happen we are done and can
 * cleanup.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int send_req(buf_t *buf)
{
	int err;
	int signaled = buf->signaled;

#ifdef USE_XRC
	if (buf->conn->state == CONN_STATE_XRC_CONNECTED)
		set_buf_dest(buf, buf->conn->main_connect);
	else
#endif
		set_buf_dest(buf, buf->conn);

	err = buf->conn->transport.send_message(buf, signaled);
	if (err)
		return STATE_INIT_SEND_ERROR;

	if (signaled) {
		if (buf->conn->transport.type == CONN_TYPE_RDMA)
			return STATE_INIT_WAIT_COMP;
		else
			return STATE_INIT_EARLY_SEND_EVENT;
	}
	else if (buf->event_mask & XI_RECEIVE_EXPECTED)
		return STATE_INIT_WAIT_RECV;
	else
		return STATE_INIT_CLEANUP;
}

/**
 * @brief initiator send error state.
 *
 * This state is reached if an error has
 * occured while trying to send the request.
 * If the caller expects events we must
 * generate them even though we have not
 * received a send or recv completion.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int send_error(buf_t *buf)
{
	buf->ni_fail = PTL_NI_UNDELIVERABLE;

	if (buf->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;
	else if (buf->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;
	else if (buf->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	else
		return STATE_INIT_CLEANUP;
}

/**
 * @brief initiator wait for send completion state.
 *
 * This state is reached if we are waiting for an
 * InfiniBand send completion. We can get here either with send
 * completion (most of the time) or with a receive completion related
 * to the ack/reply (rarely). In the latter case we will
 * go ahead and process the response event. The send completion
 * event will likely occur later while the buf is in the done state.
 * After the delayed send completion event the buf will be freed.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int wait_comp(buf_t *buf)
{
	if (buf->completed || buf->recv_buf)
		return STATE_INIT_EARLY_SEND_EVENT;
	else
		return STATE_INIT_WAIT_COMP;
}

/**
 * @brief initiator early send event state.
 *
 * This state is reached if we can deliver a send
 * event or counting event before receiving a
 * response from the target. This can only happen
 * if the message was sent as immediate data.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int early_send_event(buf_t *buf)
{
	/* Release the put MD before posting the SEND event. */
	md_put(buf->put_md);
	buf->put_md = NULL;

	if (buf->event_mask & XI_SEND_EVENT)
		make_send_event(buf);

	if (buf->event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(buf);
	
	/* an ni failure has to be 'undeliverable' so
	 * we are not going to get a response. In
	 * this case we do not generate ACK or CT ACK events. */
	if ((buf->event_mask & XI_RECEIVE_EXPECTED) && !buf->ni_fail)
		return STATE_INIT_WAIT_RECV;
	else
		return STATE_INIT_CLEANUP;
}

/**
 * @brief initiator wait for receive state.
 *
 * This state is reached if we are waiting
 * to receive a response (ack or reply).
 * If we have received one buf->recv_buf
 * will point to the receive buf.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int wait_recv(buf_t *buf)
{
	hdr_t *hdr;

	if (!buf->recv_buf)
		return STATE_INIT_WAIT_RECV;

	hdr = (hdr_t *)buf->recv_buf->data;

	/* get returned fields */
	buf->ni_fail = hdr->ni_fail;
	buf->mlength = le64_to_cpu(hdr->length);
	buf->moffset = le64_to_cpu(hdr->offset);

	if (buf->data_in && buf->get_md)
		return STATE_INIT_DATA_IN;

	if (buf->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;

	if (buf->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;

	if (buf->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
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
	ptl_size_t offset = buf->get_offset;

	/* the length may have been truncated by target */
	ptl_size_t length = buf->mlength;

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

/**
 * @brief initiator immediate data in state.
 *
 * This state is reached if we are receiving a
 * reply with immediate data. We do not receive
 * dma or indirect dma data at the initiator.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int data_in(buf_t *buf)
{
	int err;
	md_t *md = buf->get_md;
	data_t *data = buf->data_in;

	if (unlikely(data->data_fmt != DATA_FMT_IMMEDIATE)) {
		abort();
		return STATE_INIT_ERROR;
	}

	err = init_copy_in(buf, md, data->immediate.data);
	assert(!err);
	if (err)
		return STATE_INIT_ERROR;

	if (buf->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;

	if (buf->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	
	return STATE_INIT_CLEANUP;
}

/**
 * @brief initiator late send event state.
 *
 * This state is reached if we can deliver
 * a send or ct send event after receiving
 * a response.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int late_send_event(buf_t *buf)
{
	/* Release the put MD before posting the SEND event. */
	md_put(buf->put_md);
	buf->put_md = NULL;

	if (buf->event_mask & XI_SEND_EVENT)
		make_send_event(buf);

	if (buf->event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(buf);
	
	if (buf->ni_fail == PTL_NI_UNDELIVERABLE)
		return STATE_INIT_CLEANUP;

	else if (buf->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;

	else if (buf->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;

	return STATE_INIT_CLEANUP;
}

/**
 * @brief initiator ack event state.
 *
 * This state is reached if we can deliver
 * an ack or ct ack event
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int ack_event(buf_t *buf)
{
	/* Release the put MD before posting the ACK event. */
	if (buf->put_md) {
		md_put(buf->put_md);
		buf->put_md = NULL;
	}

	if (buf->event_mask & XI_ACK_EVENT)
		make_ack_event(buf);

	if (buf->event_mask & XI_CT_ACK_EVENT)
		make_ct_ack_event(buf);
	
	return STATE_INIT_CLEANUP;
}

static int reply_event(buf_t *buf)
{
	/* Release the get MD before posting the REPLY event. */
	md_put(buf->get_md);
	buf->get_md = NULL;

	if (buf->event_mask & XI_REPLY_EVENT)
		make_reply_event(buf);

	if (buf->event_mask & XI_CT_REPLY_EVENT)
		make_ct_reply_event(buf);
	
	return STATE_INIT_CLEANUP;
}

static void cleanup(buf_t *buf)
{
	if (buf->get_md) {
		md_put(buf->get_md);
		buf->get_md = NULL;
	}

	if (buf->put_md) {
		md_put(buf->put_md);
		buf->put_md = NULL;
	}

	if (buf->recv_buf) {
		buf_put(buf->recv_buf);	/* from buf_alloc in ptl_post_recv */
		buf->recv_buf = NULL;
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

	pthread_mutex_lock(&buf->mutex);

	state = buf->init_state;

	while (1) {
		if (debug) printf("%p: init state = %s\n",
				  buf, init_state_name[state]);

		switch (state) {
		case STATE_INIT_START:
			state = start(buf);
			break;
		case STATE_INIT_PREP_REQ:
			state = prepare_req(buf);	
			break;
		case STATE_INIT_WAIT_CONN:
			state = wait_conn(buf);
			if (state == STATE_INIT_WAIT_CONN)
				goto exit;
			break;
		case STATE_INIT_SEND_REQ:
			state = send_req(buf);
			break;
		case STATE_INIT_WAIT_COMP:
			state = wait_comp(buf);
			if (state == STATE_INIT_WAIT_COMP)
				goto exit;
			break;
		case STATE_INIT_SEND_ERROR:
			state = send_error(buf);
			break;
		case STATE_INIT_EARLY_SEND_EVENT:
			state = early_send_event(buf);
			break;
		case STATE_INIT_WAIT_RECV:
			state = wait_recv(buf);
			if (state == STATE_INIT_WAIT_RECV)
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
			cleanup(buf);
			buf->init_state = STATE_INIT_DONE;
			pthread_mutex_unlock(&buf->mutex);
			buf_put(buf);		/* from (s)buf_alloc in get_transport_buf */
			return err;
		case STATE_INIT_DONE:
			goto exit;
		default:
			abort();
		}
	}

exit:
	buf->init_state = state;
	pthread_mutex_unlock(&buf->mutex);

	return err;
}
