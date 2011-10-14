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

static void make_send_event(xi_t *xi)
{
	/* note: mlength and rem offset may or may not contain valid
	 * values depending on whether we have seen an ack/reply or not */
	if (xi->ni_fail || !(xi->event_mask & XI_PUT_SUCCESS_DISABLE_EVENT)) {
		make_init_event(xi, xi->put_eq, PTL_EVENT_SEND, NULL);
	}

	xi->event_mask &= ~XI_SEND_EVENT;
}

static void make_ack_event(xi_t *xi)
{
	if (xi->ni_fail || !(xi->event_mask & XI_PUT_SUCCESS_DISABLE_EVENT)) {
		make_init_event(xi, xi->put_eq, PTL_EVENT_ACK, NULL);
	}

	xi->event_mask &= ~XI_ACK_EVENT;
}

static void make_reply_event(xi_t *xi)
{
	if (xi->ni_fail || !(xi->event_mask & XI_GET_SUCCESS_DISABLE_EVENT)) {
		make_init_event(xi, xi->get_eq, PTL_EVENT_REPLY, NULL);
	}

	xi->event_mask &= ~XI_REPLY_EVENT;
}

static inline void make_ct_send_event(xi_t *xi)
{
	make_ct_event(xi->put_ct, xi->ni_fail, xi->rlength, xi->event_mask & XI_PUT_CT_BYTES);
	xi->event_mask &= ~XI_CT_SEND_EVENT;
}

static inline void make_ct_ack_event(xi_t *xi)
{
	make_ct_event(xi->put_ct, xi->ni_fail, xi->mlength, xi->event_mask & XI_PUT_CT_BYTES);
	xi->event_mask &= ~XI_CT_ACK_EVENT;
}

static inline void make_ct_reply_event(xi_t *xi)
{
	make_ct_event(xi->get_ct, xi->ni_fail, xi->mlength, xi->event_mask & XI_GET_CT_BYTES);
	xi->event_mask &= ~XI_CT_REPLY_EVENT;
}

static void init_events(xi_t *xi)
{
	if (xi->put_md) {
		if (xi->put_md->options & PTL_MD_EVENT_SUCCESS_DISABLE)
			xi->event_mask |= XI_PUT_SUCCESS_DISABLE_EVENT;
		if (xi->put_md->options & PTL_MD_EVENT_CT_BYTES)
			xi->event_mask |= XI_PUT_CT_BYTES;
	}

	if (xi->get_md) {
		if (xi->get_md->options & PTL_MD_EVENT_SUCCESS_DISABLE)
			xi->event_mask |= XI_GET_SUCCESS_DISABLE_EVENT;
		if (xi->get_md->options & PTL_MD_EVENT_CT_BYTES)
			xi->event_mask |= XI_GET_CT_BYTES;
	}

	switch (xi->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (xi->put_md->eq)
			xi->event_mask |= XI_SEND_EVENT;

		if (xi->put_md->eq && (xi->ack_req == PTL_ACK_REQ))
			xi->event_mask |= XI_ACK_EVENT | XI_RECEIVE_EXPECTED;

		if (xi->put_md->ct &&
		    (xi->put_md->options & PTL_MD_EVENT_CT_SEND))
			xi->event_mask |= XI_CT_SEND_EVENT;

		if (xi->put_md->ct && 
			(xi->ack_req == PTL_CT_ACK_REQ || xi->ack_req == PTL_OC_ACK_REQ) &&
		    (xi->put_md->options & PTL_MD_EVENT_CT_ACK))
			xi->event_mask |= XI_CT_ACK_EVENT | XI_RECEIVE_EXPECTED;
		break;
	case OP_GET:
		xi->event_mask |= XI_RECEIVE_EXPECTED;
		if (xi->get_md->eq)
			xi->event_mask |= XI_REPLY_EVENT;

		if (xi->get_md->ct &&
		    (xi->get_md->options & PTL_MD_EVENT_CT_REPLY))
			xi->event_mask |= XI_CT_REPLY_EVENT;
		break;
	case OP_FETCH:
	case OP_SWAP:
		if (xi->put_md->eq)
			xi->event_mask |= XI_SEND_EVENT;

		xi->event_mask |= XI_REPLY_EVENT | XI_RECEIVE_EXPECTED;

		if (xi->put_md->ct &&
		    (xi->put_md->options & PTL_MD_EVENT_CT_SEND)) {
			xi->event_mask |= XI_CT_SEND_EVENT;
		}

		if (xi->get_md->ct &&
		    (xi->get_md->options & PTL_MD_EVENT_CT_REPLY)) {
			xi->event_mask |= XI_CT_REPLY_EVENT;
		}
		break;
	default:
		WARN();
		break;
	}
}

static int init_start(xi_t *xi)
{
	init_events(xi);

	return STATE_INIT_PREP_REQ;
}

/* Prepare a request. */
static int init_prep_req(xi_t *xi)
{
	int err;
	ni_t *ni = obj_to_ni(xi);
	buf_t *buf;
	req_hdr_t *hdr;
	data_t *put_data = NULL;
	ptl_size_t length = xi->rlength;
	conn_t *conn = xi->conn;

	/* get per conn info */
	if (!conn) {
		conn = xi->conn = get_conn(ni, &xi->target);
		if (unlikely(!conn)) {
			WARN();
			return STATE_INIT_ERROR;
		}
	}

	if (conn->transport.type == CONN_TYPE_RDMA)
		err = buf_alloc(ni, &buf);
	else
		err = sbuf_alloc(ni, &buf);
	if (err) {
		WARN();
		return STATE_INIT_ERROR;
	}
	hdr = (req_hdr_t *)buf->data;

	memset(hdr, 0, sizeof(req_hdr_t));

	xport_hdr_from_xi((hdr_t *)hdr, xi);
	base_hdr_from_xi((hdr_t *)hdr, xi);
	req_hdr_from_xi(hdr, xi);
	hdr->operation = xi->operation;
	buf->length = sizeof(req_hdr_t);
	buf->xi = xi;
	xi_get(xi);
	buf->dest = &xi->dest;

	switch (xi->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		put_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(xi->put_md, DATA_DIR_OUT, xi->put_offset,
							   length, buf, xi->conn->transport.type);
		if (err)
			goto error;
		break;

	case OP_GET:
		err = append_init_data(xi->get_md, DATA_DIR_IN, xi->get_offset,
							   length, buf, xi->conn->transport.type);
		if (err)
			goto error;
		break;

	case OP_FETCH:
	case OP_SWAP:
		err = append_init_data(xi->get_md, DATA_DIR_IN, xi->get_offset,
							   length, buf, xi->conn->transport.type);
		if (err)
			goto error;

		put_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(xi->put_md, DATA_DIR_OUT, xi->put_offset,
							   length, buf, xi->conn->transport.type);
		if (err)
			goto error;
		break;

	default:
		WARN();
		break;
	}

	/* Always ask for a response if the remote will do an RDMA
	 * operation for the Put. Until then the response is received, we
	 * cannot free the MR nor post the send events. */
	if ((put_data && put_data->data_fmt != DATA_FMT_IMMEDIATE &&
		 (xi->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))) ||
		buf->num_mr) {
		hdr->ack_req = PTL_ACK_REQ;
		xi->event_mask |= XI_RECEIVE_EXPECTED;
	}

	/* If we want an event, then do not request a completion for
	 * that message. It will be freed when we receive the ACK or
	 * reply. */
	buf->signaled = put_data && (put_data->data_fmt == DATA_FMT_IMMEDIATE) &&
	    (xi->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT));

	xi->send_buf = buf;

	return STATE_INIT_WAIT_CONN;

 error:
	buf_put(buf);
	return STATE_INIT_ERROR;
}

static int wait_conn(xi_t *xi)
{
	ni_t *ni = obj_to_ni(xi);
	conn_t *conn = xi->conn;

	/* note once connected we don't go back */
	if (conn->state >= CONN_STATE_CONNECTED)
		goto out;

	/* if not connected. Add the xt on the pending list. It will be
	 * retried once connected/disconnected. */
	pthread_mutex_lock(&conn->mutex);
	if (conn->state < CONN_STATE_CONNECTED) {
		pthread_spin_lock(&conn->wait_list_lock);
		list_add_tail(&xi->list, &conn->xi_list);
		pthread_spin_unlock(&conn->wait_list_lock);

		if (conn->state == CONN_STATE_DISCONNECTED) {
			if (init_connect(ni, conn)) {
				pthread_mutex_unlock(&conn->mutex);
				pthread_spin_lock(&conn->wait_list_lock);
				list_del(&xi->list);
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
		set_xi_dest(xi, conn->main_connect);
	else
#endif
		set_xi_dest(xi, conn);

	return STATE_INIT_SEND_REQ;
}

static int init_send_req(xi_t *xi)
{
	int err;
	int signaled = xi->send_buf->signaled;

	err = xi->conn->transport.send_message(xi->send_buf, signaled);
	if (err) {
		buf_put(xi->send_buf);
		xi->send_buf = NULL;
		return STATE_INIT_SEND_ERROR;
	}
	xi->send_buf = NULL;

	if (signaled) {
		if (xi->conn->transport.type == CONN_TYPE_RDMA)
			return STATE_INIT_WAIT_COMP;
		else
			return STATE_INIT_EARLY_SEND_EVENT;
	}
	else if (xi->event_mask & XI_RECEIVE_EXPECTED)
		return STATE_INIT_WAIT_RECV;
	else
		return STATE_INIT_CLEANUP;
}

static int init_send_error(xi_t *xi)
{
	xi->ni_fail = PTL_NI_UNDELIVERABLE;

	if (xi->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;
	else if (xi->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;
	else if (xi->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	else
		return STATE_INIT_CLEANUP;
}

/* Wait for an IB completion event. We can get here either with send
 * completion (most of the time) or with a receive completion related
 * to the ack/reply (rarely). */
static int init_wait_comp(xi_t *xi)
{
	if (xi->completed || xi->recv_buf) {
		return STATE_INIT_EARLY_SEND_EVENT;
	} else {
		return STATE_INIT_WAIT_COMP;
	}
}

static int early_send_event(xi_t *xi)
{
	/* Release the MD before posting the SEND event. */
	md_put(xi->put_md);
	xi->put_md = NULL;

	if (xi->event_mask & XI_SEND_EVENT)
		make_send_event(xi);

	if (xi->event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(xi);
	
	if ((xi->event_mask & XI_RECEIVE_EXPECTED) && !xi->ni_fail)
		return STATE_INIT_WAIT_RECV;
	else
		return STATE_INIT_CLEANUP;
}

static int wait_recv(xi_t *xi)
{
	buf_t *buf;
	hdr_t *hdr;

	/* We can come here on the application or the progress thread, but
	 * we need the receive buffer to make progress. */
	if (!xi->recv_buf)
		return STATE_INIT_WAIT_RECV;

	buf = xi->recv_buf;
	hdr = (hdr_t *)buf->data;

	/* get returned fields */
	xi->ni_fail = hdr->ni_fail;
	xi->mlength = be64_to_cpu(hdr->length);
	xi->moffset = be64_to_cpu(hdr->offset);

	if (debug) buf_dump(buf);

	/* Check for short immediate reply data. */
	if (xi->data_in && xi->get_md)
		return STATE_INIT_DATA_IN;

	if (xi->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;

	if (xi->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;

	if (xi->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	
	return STATE_INIT_CLEANUP;
}

/*
 * init_copy_in
 *	copy data from data segment to md
 */
static int init_copy_in(xi_t *xi, md_t *md, void *data)
{
	int err;

	/* the offset into the get_md is the original one */
	ptl_size_t offset = xi->get_offset;

	/* the length may have been truncated by target */
	ptl_size_t length = xi->mlength;

	assert(length <= xi->get_resid);

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
static int data_in(xi_t *xi)
{
	int err;
	md_t *md = xi->get_md;
	data_t *data = xi->data_in;

	switch (data->data_fmt) {
	case DATA_FMT_IMMEDIATE:
		err = init_copy_in(xi, md, data->immediate.data);
		if (err)
			return STATE_TGT_ERROR;
		break;
	default:
		printf("unexpected data in format\n");
		break;
	}

	if (xi->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;

	if (xi->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	
	return STATE_INIT_CLEANUP;
}

static int late_send_event(xi_t *xi)
{
	/* Release the MD before posting the SEND event. */
	md_put(xi->put_md);
	xi->put_md = NULL;

	if (xi->event_mask & XI_SEND_EVENT)
		make_send_event(xi);

	if (xi->event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(xi);
	
	if (xi->ni_fail == PTL_NI_UNDELIVERABLE)
		return STATE_INIT_CLEANUP;
	else if (xi->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;
	else if (xi->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	else
		return STATE_INIT_CLEANUP;
}

static int ack_event(xi_t *xi)
{
	buf_t *buf = xi->recv_buf;
	hdr_t *hdr = (hdr_t *)buf->data;

	/* Release the MD before posting the ACK event. */
	if (xi->put_md) {
		md_put(xi->put_md);
		xi->put_md = NULL;
	}

	if (hdr->operation != OP_NO_ACK) {
		if (xi->event_mask & XI_ACK_EVENT)
			make_ack_event(xi);

		if (xi->event_mask & XI_CT_ACK_EVENT)
			make_ct_ack_event(xi);
	}
	
	return STATE_INIT_CLEANUP;
}

static int reply_event(xi_t *xi)
{
	/* Release the MD before posting the REPLY event. */
	md_put(xi->get_md);
	xi->get_md = NULL;

	if ((xi->event_mask & XI_REPLY_EVENT) &&
		xi->get_eq)
		make_reply_event(xi);

	if (xi->event_mask & XI_CT_REPLY_EVENT)
		make_ct_reply_event(xi);
	
	return STATE_INIT_CLEANUP;
}

static void init_cleanup(xi_t *xi)
{
	if (xi->get_md) {
		md_put(xi->get_md);
		xi->get_md = NULL;
	}

	if (xi->put_md) {
		md_put(xi->put_md);
		xi->put_md = NULL;
	}

	if (xi->recv_buf) {
		buf_put(xi->recv_buf);
		xi->recv_buf = NULL;
	}

	if (xi->ack_buf) {
		buf_put(xi->ack_buf);
	}
}

/*
 * process_init
 *	this can run on the API or progress threads
 *	calling process_init will guarantee that the
 *	loop will run at least once either on the calling
 *	thread or the currently active thread
 */
int process_init(xi_t *xi)
{
	int err = PTL_OK;
	int state;

	do {
		pthread_mutex_lock(&xi->mutex);

		state = xi->state;

		while (1) {
			if (debug) printf("%p: init state = %s\n",
					  xi, init_state_name[state]);

			switch (state) {
			case STATE_INIT_START:
				state = init_start(xi);
				break;
			case STATE_INIT_PREP_REQ:
				state = init_prep_req(xi);	
				break;
			case STATE_INIT_WAIT_CONN:
				state = wait_conn(xi);
				if (state == STATE_INIT_WAIT_CONN)
					goto exit;
				break;
			case STATE_INIT_SEND_REQ:
				state = init_send_req(xi);
				if (state == STATE_INIT_WAIT_COMP) {
					/* Never finish that on application thread, 
					 * else a race with the receive thread will occur. */
					goto exit;
				}
				break;
			case STATE_INIT_WAIT_COMP:
				state = init_wait_comp(xi);
				if (state == STATE_INIT_WAIT_COMP) {
					goto exit;
				}
				break;
			case STATE_INIT_SEND_ERROR:
				state = init_send_error(xi);
				break;
			case STATE_INIT_EARLY_SEND_EVENT:
				state = early_send_event(xi);
				break;
			case STATE_INIT_WAIT_RECV:
				state = wait_recv(xi);
				if (state == STATE_INIT_WAIT_RECV)
					/* Nothing received yet. */
					goto exit;
				break;
			case STATE_INIT_DATA_IN:
				state = data_in(xi);
				break;
			case STATE_INIT_LATE_SEND_EVENT:
				state = late_send_event(xi);
				break;
			case STATE_INIT_ACK_EVENT:
				state = ack_event(xi);
				break;
			case STATE_INIT_REPLY_EVENT:
				state = reply_event(xi);
				break;

			case STATE_INIT_ERROR:
				err = PTL_FAIL;
				state = STATE_INIT_CLEANUP;
				break;

			case STATE_INIT_CLEANUP:
				init_cleanup(xi);
				xi->state = STATE_INIT_DONE;
				pthread_mutex_unlock(&xi->mutex);
				xi_put(xi);
				return err;
				break;

			case STATE_INIT_DONE:
				/* We reach that state only if the send completion is
				 * received after the recv completion. */
				assert(xi->obj.obj_ref.ref_cnt == 1);
				goto exit;
			default:
				abort();
			}
		}
exit:
		xi->state = state;

		pthread_mutex_unlock(&xi->mutex);
	} while(0);

	return err;
}
