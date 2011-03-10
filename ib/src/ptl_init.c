/*
 * ptl_init.c - initiator side processing
 */
#include "ptl_loc.h"

static char *init_state_name[] = {
	[STATE_INIT_START]			= "init_start",
	[STATE_INIT_SEND_ERROR]		= "init_send_error",
	[STATE_INIT_WAIT_COMP]		= "init_wait_comp",
	[STATE_INIT_HANDLE_COMP]	= "init_handle_comp",
	[STATE_INIT_EARLY_SEND_EVENT]	= "init_early_send_event",
	[STATE_INIT_GET_RECV]		= "init_get_recv",
	[STATE_INIT_WAIT_RECV]		= "init_wait_recv",
	[STATE_INIT_HANDLE_RECV]	= "init_handle_recv",
	[STATE_INIT_LATE_SEND_EVENT]	= "init_late_send_event",
	[STATE_INIT_ACK_EVENT]		= "init_ack_event",
	[STATE_INIT_REPLY_EVENT]	= "init_reply_event",
	[STATE_INIT_CLEANUP]		= "init_cleanup",
	[STATE_INIT_ERROR]		= "init_error",
	[STATE_INIT_DONE]		= "init_done",
};

static int make_send_event(xi_t *xi)
{
	int err = PTL_OK;
	md_t *md = xi->put_md;
	eq_t *eq = md->eq;

	/* note: mlength and rem offset may or may not contain valid
	 * values depending on whether we have seen an ack/reply or not */
	if (xi->ni_fail || !(md->options & PTL_MD_EVENT_SUCCESS_DISABLE)) {
		err = make_init_event(xi, eq, PTL_EVENT_SEND, NULL);
		if (err)
			WARN();
	}

	xi->event_mask &= ~XI_SEND_EVENT;

	return err;
}

static int make_ack_event(xi_t *xi)
{
	int err = PTL_OK;
	md_t *md = xi->put_md;
	eq_t *eq = md->eq;
	
	if (xi->ni_fail || !(md->options & PTL_MD_EVENT_SUCCESS_DISABLE)) {
		err = make_init_event(xi, eq, PTL_EVENT_ACK, NULL);
		if (err)
			WARN();
	}

	xi->event_mask &= ~XI_ACK_EVENT;

	return err;
}

static int make_reply_event(xi_t *xi)
{
	int err = PTL_OK;
	md_t *md = xi->get_md;
	eq_t *eq = md->eq;
	
	if (xi->ni_fail || !(md->options & PTL_MD_EVENT_SUCCESS_DISABLE)) {
		err = make_init_event(xi, eq, PTL_EVENT_REPLY, NULL);
		if (err)
			WARN();
	}

	xi->event_mask &= ~XI_REPLY_EVENT;

	return err;
}

static inline void make_ct_send_event(xi_t *xi)
{
	md_t *md = xi->put_md;
	ct_t *ct = md->ct;
	int bytes = md->options & PTL_MD_EVENT_CT_BYTES;

	make_ct_event(ct, xi->ni_fail, xi->mlength, bytes);
	xi->event_mask &= ~XI_CT_SEND_EVENT;
}

static inline void make_ct_ack_event(xi_t *xi)
{
	md_t *md = xi->put_md;
	ct_t *ct = md->ct;
	int bytes = md->options & PTL_MD_EVENT_CT_BYTES;

	make_ct_event(ct, xi->ni_fail, xi->mlength, bytes);
	xi->event_mask &= ~XI_CT_ACK_EVENT;
}

static inline void make_ct_reply_event(xi_t *xi)
{
	md_t *md = xi->get_md;
	ct_t *ct = md->ct;
	int bytes = md->options & PTL_MD_EVENT_CT_BYTES;

	make_ct_event(ct, xi->ni_fail, xi->mlength, bytes);
	xi->event_mask &= ~XI_CT_REPLY_EVENT;
}

static void init_events(xi_t *xi)
{
	switch (xi->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (xi->put_md->eq)
			xi->event_mask |= XI_SEND_EVENT;

		if (xi->put_md->eq && (xi->ack_req == PTL_ACK_REQ))
			xi->event_mask |= XI_ACK_EVENT;

		if (xi->put_md->ct &&
		    (xi->put_md->options & PTL_MD_EVENT_CT_SEND))
			xi->event_mask |= XI_CT_SEND_EVENT;

		if (xi->put_md->ct && (xi->ack_req == PTL_CT_ACK_REQ) &&
		    (xi->put_md->options & PTL_MD_EVENT_CT_ACK))
			xi->event_mask |= XI_CT_ACK_EVENT;
		break;
	case OP_GET:
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

		if (xi->get_md->eq)
			xi->event_mask |= XI_REPLY_EVENT;

		if (xi->put_md->ct &&
		    (xi->put_md->options & PTL_MD_EVENT_CT_SEND))
			xi->event_mask |= XI_CT_SEND_EVENT;

		if (xi->get_md->ct &&
		    (xi->get_md->options & PTL_MD_EVENT_CT_REPLY))
			xi->event_mask |= XI_CT_REPLY_EVENT;
		break;
	default:
		WARN();
		break;
	}
}

/*
 * init_start
 *	start request operation
 *	call assumes xi has been filled in from one the
 *	move or triggered move calls
 */
static int init_start(xi_t *xi)
{
	int err;
	ni_t *ni = to_ni(xi);
	buf_t *buf;
	req_hdr_t *hdr;
	data_t *get_data = NULL;
	data_t *put_data = NULL;
	ptl_size_t length = xi->rlength;
	gbl_t *gbl = ni->gbl;
	struct nid_connect *connect;

	if (unlikely(xi->target.rank >= gbl->nranks)) {
		ptl_warn("Invalid rank (%d >= %d\n",
				 xi->target.rank, gbl->nranks);
		return STATE_INIT_ERROR;
	}

	/* Ensure we are already connected. */
	connect = ni->rank_to_nid_table[xi->target.rank].connect;
	pthread_mutex_lock(&connect->mutex);
	if (unlikely(connect->state != GBLN_CONNECTED)) {
		/* Not connected. Add the xi on the pending list. It will be
		 * flushed once connected/disconnected. */
		int ret;

		if (connect->state == GBLN_DISCONNECTED) {
			/* Initiate connection. */
			list_add_tail(&xi->connect_pending_list, &connect->xi_list);

			if (init_connect(ni, connect)) {
				list_del(&xi->connect_pending_list);
				ret = STATE_INIT_ERROR;
			} else
				ret = STATE_INIT_START;
		} else {
			/* Connection in already in progress. */
			ret = STATE_INIT_START;
		}

		pthread_mutex_unlock(&connect->mutex);

		return ret;
	} else {
		set_xi_dest(xi, connect);
	}

	pthread_mutex_unlock(&connect->mutex);

	err = buf_alloc(ni, &buf);
	if (err) {
		WARN();
		return STATE_INIT_ERROR;
	}
	hdr = (req_hdr_t *)buf->data;

	xi->send_buf = buf;

	xport_hdr_from_xi((hdr_t *)hdr, xi);
	base_hdr_from_xi((hdr_t *)hdr, xi);
	req_hdr_from_xi(hdr, xi);
	hdr->operation = xi->operation;
	buf->length = sizeof(req_hdr_t);
	buf->xi = xi;
	buf->dest = &xi->dest;

	switch (xi->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		put_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(xi->put_md, DATA_DIR_OUT, xi->put_offset,
			     length, buf);
		if (err)
			return STATE_INIT_ERROR;
		break;

	case OP_GET:
		get_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(xi->get_md, DATA_DIR_IN, xi->get_offset,
			     length, buf);
		if (err)
			return STATE_INIT_ERROR;
		break;

	case OP_FETCH:
	case OP_SWAP:
		get_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(xi->get_md, DATA_DIR_IN, xi->get_offset,
			     length, buf);
		if (err)
			return STATE_INIT_ERROR;

		put_data = (data_t *)(buf->data + buf->length);
		err = append_init_data(xi->put_md, DATA_DIR_OUT, xi->put_offset,
			     length, buf);
		if (err)
			return STATE_INIT_ERROR;
		break;

	default:
		WARN();
		break;
	}

	buf->send_wr.opcode = IBV_WR_SEND;
	buf->sg_list[0].length = buf->length;
	buf->send_wr.send_flags = IBV_SEND_SIGNALED;

	init_events(xi);

	if (put_data && (put_data->data_fmt == DATA_FMT_IMMEDIATE) &&
	    (xi->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT)) &&
	    (xi->put_md->options & PTL_MD_REMOTE_FAILURE_DISABLE))
		xi->next_state = STATE_INIT_EARLY_SEND_EVENT;
	else if (xi->event_mask) {
		/* we need an ack and they are all the same */
		hdr->ack_req = PTL_ACK_REQ;
		xi->next_state = STATE_INIT_GET_RECV;
	} else
		xi->next_state = STATE_INIT_CLEANUP;

	err = send_message(buf);
	if (err)
		return STATE_INIT_SEND_ERROR;

	return STATE_INIT_WAIT_COMP;
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

static int wait_comp(xi_t *xi)
{
	pthread_spin_lock(&xi->send_lock);
	if (xi->send_buf) {
		pthread_spin_unlock(&xi->send_lock);
		return STATE_INIT_WAIT_COMP;
	} else {
		pthread_spin_unlock(&xi->send_lock);
		return STATE_INIT_HANDLE_COMP;
	}
}

static int handle_comp(xi_t *xi)
{
	return xi->next_state;
}

static int early_send_event(xi_t *xi)
{
	if (xi->event_mask & XI_SEND_EVENT)
		make_send_event(xi);

	if (xi->event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(xi);
	
	if (xi->event_mask)
		return STATE_INIT_GET_RECV;
	else
		return STATE_INIT_CLEANUP;
}

static int get_recv(xi_t *xi)
{
	if (xi->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		xi->next_state = STATE_INIT_LATE_SEND_EVENT;
	else if (xi->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		xi->next_state = STATE_INIT_ACK_EVENT;
	else if (xi->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		xi->next_state = STATE_INIT_REPLY_EVENT;
	else
		xi->next_state = STATE_INIT_CLEANUP;

	return STATE_INIT_WAIT_RECV;
}

static int wait_recv(xi_t *xi)
{
	ni_t *ni = to_ni(xi);
	int ret;

	pthread_spin_lock(&xi->recv_lock);
	if (list_empty(&xi->recv_list)) {
		pthread_spin_lock(&ni->xi_wait_list_lock);
		list_add(&xi->list, &ni->xi_wait_list);
		pthread_spin_unlock(&ni->xi_wait_list_lock);
		xi->state_waiting = 1;
		ret = STATE_INIT_WAIT_RECV;
	} else {
		ret = STATE_INIT_HANDLE_RECV;
	}
	pthread_spin_unlock(&xi->recv_lock);

	return ret;
}

static int handle_recv(xi_t *xi)
{
	buf_t *buf;
	hdr_t *hdr;

	/* we took another reference, drop it now */
	xi_put(xi);

	buf = xx_dequeue_recv_buf(xi);
	xi->recv_buf = buf;
	hdr = (hdr_t *)buf->data;

	/* get returned fields */
	xi->ni_fail = hdr->ni_fail;
	xi->mlength = be64_to_cpu(hdr->length);
	xi->moffset = be64_to_cpu(hdr->offset);

	if (debug) buf_dump(buf);

	return xi->next_state;
}

static int late_send_event(xi_t *xi)
{
	if (xi->event_mask & XI_SEND_EVENT)
		make_send_event(xi);

	if (xi->event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(xi);
	
	if (xi->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
		return STATE_INIT_ACK_EVENT;
	else if (xi->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
		return STATE_INIT_REPLY_EVENT;
	else
		return STATE_INIT_CLEANUP;
}

static int ack_event(xi_t *xi)
{
	if (xi->event_mask & XI_ACK_EVENT)
		make_ack_event(xi);

	if (xi->event_mask & XI_CT_ACK_EVENT)
		make_ct_ack_event(xi);
	
	return STATE_INIT_CLEANUP;
}

static int reply_event(xi_t *xi)
{
	if (xi->event_mask & XI_REPLY_EVENT)
		make_reply_event(xi);

	if (xi->event_mask & XI_CT_REPLY_EVENT)
		make_ct_reply_event(xi);
	
	return STATE_INIT_CLEANUP;
}

static int init_cleanup(xi_t *xi)
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

	pthread_spin_lock(&xi->recv_lock);
	while (!list_empty(&xi->recv_list)) {
		struct list_head *l;
		buf_t *buf;

		l = xi->recv_list.next;
		list_del(l);
		buf = list_entry(l, buf_t, list);
		buf_put(buf);
		// TODO count these as dropped packets??
	}
	pthread_spin_unlock(&xi->recv_lock);

	xi_put(xi);
	return STATE_INIT_DONE;
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
	ni_t *ni = to_ni(xi);

	xi->state_again = 1;

	do {
		err = pthread_spin_trylock(&xi->state_lock);
		if (err) {
			if (err == EBUSY) {
				/* we've set send_again so the current
				 * thread will take another crack at it */
				return PTL_OK;
			} else {
				WARN();
				return PTL_FAIL;
			}
		}

		xi->state_again = 0;

		/* we keep xi on a list in the NI in case we never
		 * get done so that cleanup is possible
		 * make sure we get off the list before running the
		 * loop. If we're still blocked we will get put
		 * back on before we leave. The send_lock will serialize
		 * changes to send_waiting */
		if (xi->state_waiting) {
			pthread_spin_lock(&ni->xi_wait_list_lock);
			list_del(&xi->list);
			pthread_spin_unlock(&ni->xi_wait_list_lock);
			xi->state_waiting = 0;
		}

		state = xi->state;

		while (1) {
			if (debug) printf("init state = %s\n",
					  init_state_name[state]);
			switch (state) {
			case STATE_INIT_START:
				state = init_start(xi);
				if (state == STATE_INIT_START)
					goto exit;
				break;
			case STATE_INIT_SEND_ERROR:
				state = init_send_error(xi);
				break;
			case STATE_INIT_WAIT_COMP:
				state = wait_comp(xi);
				if (state == STATE_INIT_WAIT_COMP)
					goto exit;
				break;
			case STATE_INIT_HANDLE_COMP:
				state = handle_comp(xi);
				break;
			case STATE_INIT_EARLY_SEND_EVENT:
				state = early_send_event(xi);
				break;
			case STATE_INIT_GET_RECV:
				state = get_recv(xi);
				break;
			case STATE_INIT_WAIT_RECV:
				state = wait_recv(xi);
				if (state == STATE_INIT_WAIT_RECV)
					goto exit;
				break;
			case STATE_INIT_HANDLE_RECV:
				state = handle_recv(xi);
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
			case STATE_INIT_CLEANUP:
				state = init_cleanup(xi);
				break;
			case STATE_INIT_ERROR:
				state = init_cleanup(xi);
				err = PTL_FAIL;
				break;
			case STATE_INIT_DONE:
				goto exit;
			}
		}
exit:
		xi->state = state;
		pthread_spin_unlock(&xi->state_lock);
	} while(xi->state_again);

	return err;
}
