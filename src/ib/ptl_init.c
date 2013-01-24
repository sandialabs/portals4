/**
 * @file ptl_init.c
 *
 * @brief Initiator side processing.
 */
#include "ptl_loc.h"

static char *init_state_name[] = {
	[STATE_INIT_START]		= "start",
	[STATE_INIT_PREP_REQ]		= "prepare_req",
	[STATE_INIT_WAIT_CONN]		= "wait_conn",
	[STATE_INIT_SEND_REQ]		= "send_req",
	[STATE_INIT_COPY_IN]		= "copy_in",
	[STATE_INIT_COPY_OUT]		= "copy_out",
	[STATE_INIT_COPY_DONE]		= "copy_done",
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

/**
 * @brief post a send event to caller.
 *
 * @param[in] buf the request buf.
 */
static inline void make_send_event(buf_t *buf)
{
	if (buf->ni_fail || !(buf->event_mask & XI_PUT_SUCCESS_DISABLE_EVENT))
		make_init_event(buf, buf->put_eq, PTL_EVENT_SEND);

	buf->event_mask &= ~XI_SEND_EVENT;
}

/**
 * @brief post an ack event to caller.
 *
 * @param[in] buf the request buf.
 */
static inline void make_ack_event(buf_t *buf)
{
	if (buf->ni_fail || !(buf->event_mask & XI_PUT_SUCCESS_DISABLE_EVENT))
		make_init_event(buf, buf->put_eq, PTL_EVENT_ACK);

	buf->event_mask &= ~XI_ACK_EVENT;
}

/**
 * @brief post a reply event to caller.
 *
 * @param[in] buf the request buf.
 */
static inline void make_reply_event(buf_t *buf)
{
	if (buf->ni_fail || !(buf->event_mask & XI_GET_SUCCESS_DISABLE_EVENT))
		make_init_event(buf, buf->get_eq, PTL_EVENT_REPLY);

	buf->event_mask &= ~XI_REPLY_EVENT;
}

/**
 * @brief post a ct send event to caller.
 *
 * @param[in] buf the request buf.
 */
static inline void make_ct_send_event(buf_t *buf)
{
	/* For send events we use the requested length instead of the
	 * modified length since we haven't had a chance to see it
	 * yet. This only matters if we are counting bytes. */
	int bytes = (buf->event_mask & XI_PUT_CT_BYTES) ?
			CT_RBYTES : CT_EVENTS;

	make_ct_event(buf->put_ct, buf, bytes);

	buf->event_mask &= ~XI_CT_SEND_EVENT;
}

/**
 * @brief post a ct ack event to caller.
 *
 * @param[in] buf the request buf.
 */
static inline void make_ct_ack_event(buf_t *buf)
{
	int bytes = (buf->event_mask & XI_PUT_CT_BYTES) ?
			CT_MBYTES : CT_EVENTS;

	make_ct_event(buf->put_ct, buf, bytes);

	buf->event_mask &= ~XI_CT_ACK_EVENT;
}

/**
 * @brief post a ct reply event to caller.
 *
 * @param[in] buf the request buf.
 */
static inline void make_ct_reply_event(buf_t *buf)
{
	int bytes = (buf->event_mask & XI_GET_CT_BYTES) ?
			CT_MBYTES : CT_EVENTS;

	make_ct_event(buf->get_ct, buf, bytes);

	buf->event_mask &= ~XI_CT_REPLY_EVENT;
}

/**
 * @brief initiator start state.
 *
 * This state analyzes the request
 * and determines the buf event mask.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int start(buf_t *buf)
{
	req_hdr_t *hdr = (req_hdr_t *)buf->data;

#if WITH_TRANSPORT_UDP
	((struct hdr_common *)buf->data)->version = PTL_HDR_VER_1;
	assert(((struct hdr_common *)buf->data)->version == PTL_HDR_VER_1);
#endif


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

	switch (hdr->h1.operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (buf->put_md->eq)
			buf->event_mask |= XI_SEND_EVENT;

		if (hdr->ack_req) {
			/* Some sort of ACK has been requested. */
			buf->event_mask |= XI_RECEIVE_EXPECTED;

			if (hdr->ack_req == PTL_ACK_REQ &&
				buf->put_md->eq)
				buf->event_mask |= XI_ACK_EVENT;

			/* All three forms of ACK can generate a counting
			 * event. */
			if (buf->put_md->ct &&
				(buf->put_md->options & PTL_MD_EVENT_CT_ACK))
				buf->event_mask |= XI_CT_ACK_EVENT;
		}

		if (buf->put_md->ct &&
		    (buf->put_md->options & PTL_MD_EVENT_CT_SEND))
			buf->event_mask |= XI_CT_SEND_EVENT;
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
	ptl_size_t length = buf->rlength;

	hdr->h1.version = PTL_HDR_VER_1;
	hdr->h1.ni_type = ni->ni_type;
	hdr->h1.pkt_fmt = PKT_FMT_REQ;
	hdr->h1.handle = cpu_to_le32(buf_to_handle(buf));
	hdr->h1.operand = 0;
	hdr->src_nid = cpu_to_le32(ni->id.phys.nid);
	hdr->src_pid = cpu_to_le32(ni->id.phys.pid);
#if WITH_TRANSPORT_UDP
	ptl_info("initiator nid: %i pid: %i NI: %p\n",hdr->src_nid,hdr->src_pid,ni);
	ptl_info("buffer handle: %i %i buf:%p\n",hdr->h1.handle,le32_to_cpu(hdr->h1.handle),&buf);
#endif
	hdr->rlength = cpu_to_le64(length);
	hdr->roffset = cpu_to_le64(buf->roffset);

#if IS_PPE
	hdr->h1.physical = !!(ni->options & PTL_NI_PHYSICAL);
	hdr->h1.dst_nid = cpu_to_le32(buf->target.phys.nid);
	hdr->h1.dst_pid = cpu_to_le32(buf->target.phys.pid);
	hdr->h1.hash = cpu_to_le32(ni->mem.hash);
#endif

	buf->length = sizeof(req_hdr_t);

	switch (hdr->h1.operation) {
	case OP_PUT:
	case OP_ATOMIC:
		hdr->h1.data_in = 0;
		hdr->h1.data_out = 1;

		buf->data_in = NULL;
		buf->data_out = (data_t *)(buf->data + buf->length);
		err = buf->conn->transport.init_prepare_transfer(buf->put_md, DATA_DIR_OUT,
														 buf->put_offset, length, buf);
		if (err)
			goto error;
		break;

	case OP_GET:
		hdr->h1.data_in = 1;
		hdr->h1.data_out = 0;

		buf->data_out = NULL;
		buf->data_in = (data_t *)(buf->data + buf->length);
		err = buf->conn->transport.init_prepare_transfer(buf->get_md, DATA_DIR_IN, 
				 buf->get_offset, length, buf);
		if (err)
			goto error;
		break;

	case OP_SWAP:
		/* If it is a CSWAP and variants, or MSWAP operation, then the
		 * operand has already been added after the header. This is
		 * not too pretty, and it might be better to set buf->length
		 * in all the functions in ptl_move.c. */
		if (hdr->atom_op != PTL_SWAP) {
			assert(hdr->atom_op >= PTL_CSWAP && hdr->atom_op <= PTL_MSWAP);
			hdr->h1.operand = 1;
			buf->length += sizeof(datatype_t);
		}

		/* fall through ... */

	case OP_FETCH:
		hdr->h1.data_in = 1;
		hdr->h1.data_out = 1;

		buf->data_in = (data_t *)(buf->data + buf->length);
		err = buf->conn->transport.init_prepare_transfer(buf->get_md, DATA_DIR_IN,
														 buf->get_offset, length, buf);
		if (err)
			goto error;

		buf->data_out = (data_t *)(buf->data + buf->length);
		err = buf->conn->transport.init_prepare_transfer(buf->put_md, DATA_DIR_OUT,
														 buf->put_offset, length, buf);
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
	if ((buf->data_out && (buf->data_out->data_fmt != DATA_FMT_IMMEDIATE) &&
		 (buf->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))) ||
	    buf->num_mr) {
		hdr->ack_req = PTL_ACK_REQ;
		buf->event_mask |= XI_RECEIVE_EXPECTED;
	}

	/* For immediate data we can cause an early send event provided
	 * we request a send completion event */
	if (buf->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT) &&
		(buf->data_out && buf->data_out->data_fmt == DATA_FMT_IMMEDIATE))
		buf->event_mask |= XI_EARLY_SEND;

	/* Inline the data if it fits. That may save waiting for a
	 * completion. */
	buf->conn->transport.set_send_flags(buf, 0);

	/* Protect the request packet until it is sent. */
	if (!(buf->event_mask & XX_INLINE) &&
		((buf->event_mask & XI_EARLY_SEND) ||
		 !(buf->event_mask & XI_RECEIVE_EXPECTED)))
		buf->event_mask |= XX_SIGNALED;

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
		if (conn->state == CONN_STATE_DISCONNECTED) {
			if (conn->transport.init_connect(ni, conn)) {
				pthread_mutex_unlock(&conn->mutex);

				return STATE_INIT_ERROR;
			}
		}

#if WITH_TRANSPORT_UDP
		ptl_info("SM: start waiting on %p\n",&conn->move_wait);
#endif


#if WITH_TRANSPORT_IB || WITH_TRANSPORT_UDP

//#if WITH_TRANSPORT_UDP
//		if (conn->udp.loop_to_self != 1)
//#endif
		pthread_cond_wait(&conn->move_wait, &conn->mutex);
#endif

	 	pthread_mutex_unlock(&conn->mutex);

		if (conn->state == CONN_STATE_CONNECTED)
			return STATE_INIT_SEND_REQ;

		WARN();
		return STATE_INIT_ERROR;
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
	conn_t *conn = buf->conn;
	int state;


#if WITH_TRANSPORT_UDP

//	if (conn->udp.loop_to_self == 1){
//		conn->udp.dest_addr = conn->sin;
//		buf->recv_buf = buf->internal_data;
//	}
	
	ptl_info("set destination to: %s:%i \n",inet_ntoa(conn->udp.dest_addr.sin_addr),ntohs(conn->udp.dest_addr.sin_port));
	
#endif

	set_buf_dest(buf, conn);

#if WITH_TRANSPORT_SHMEM && !USE_KNEM
	if ((buf->data_in && buf->data_in->data_fmt == DATA_FMT_NOKNEM) ||
		(buf->data_out && buf->data_out->data_fmt == DATA_FMT_NOKNEM)) {
		ni_t *ni = obj_to_ni(buf);

		PTL_FASTLOCK_LOCK(&ni->shmem.noknem_lock);
		list_add_tail(&buf->list, &ni->shmem.noknem_list);
		PTL_FASTLOCK_UNLOCK(&ni->shmem.noknem_lock);

		if (buf->data_in && buf->data_in->data_fmt == DATA_FMT_NOKNEM)
			state = STATE_INIT_COPY_IN;
		else
			state = STATE_INIT_COPY_OUT;
	}
	else
#endif
	if (buf->event_mask & XX_SIGNALED)
		state = STATE_INIT_WAIT_COMP;
	else if (buf->event_mask & XI_EARLY_SEND)
		state = STATE_INIT_EARLY_SEND_EVENT;
	else if (buf->event_mask & XI_RECEIVE_EXPECTED)
		state = STATE_INIT_WAIT_RECV;
	else
		state = STATE_INIT_CLEANUP;
      
	err = buf->conn->transport.send_message(buf, 1);
	if (err)
		return STATE_INIT_SEND_ERROR;

	return state;
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
	/* Release the put MD. */
	if (buf->put_md) {
		md_put(buf->put_md);
		buf->put_md = NULL;
	}

	buf->ni_fail = PTL_NI_UNDELIVERABLE;

	if (buf->event_mask & (XI_SEND_EVENT | XI_CT_SEND_EVENT))
		return STATE_INIT_LATE_SEND_EVENT;
	else
		return STATE_INIT_CLEANUP;
}

#if WITH_TRANSPORT_SHMEM && !USE_KNEM
static int init_copy_in(buf_t *buf)
{
	struct noknem *noknem = buf->transfer.noknem.noknem;
	ptl_size_t to_copy;
	int ret;

	/* Copy the data from the bounce buffer. */
	to_copy = noknem->length;

	/* Target should never send more than requested. */
	assert(to_copy <= buf->transfer.noknem.length_left);

	ret = iov_copy_in(buf->transfer.noknem.data, buf->transfer.noknem.iovecs,
					  NULL,
					  buf->transfer.noknem.num_iovecs,
					  buf->transfer.noknem.offset,
					  to_copy);
	if (ret == PTL_FAIL) {
		WARN();
		return STATE_INIT_ERROR;
	}

	if (noknem->target_done)
		return STATE_INIT_COPY_DONE;

	buf->transfer.noknem.length_left -= to_copy;
	buf->transfer.noknem.offset += to_copy;

	/* Tell the target the data is ready. */
	__sync_synchronize();
	noknem->state = 2;

	return STATE_INIT_COPY_IN;
}

static int init_copy_out(buf_t *buf)
{
	struct noknem *noknem = buf->transfer.noknem.noknem;
	ptl_size_t to_copy;
	int ret;

	if (noknem->target_done)
		return STATE_INIT_COPY_DONE;

	noknem->state = 1;

	/* Copy the data to the bounce buffer. */
	to_copy = buf->transfer.noknem.data_length;
	if (to_copy > buf->transfer.noknem.length_left)
		to_copy = buf->transfer.noknem.length_left;

	ret = iov_copy_out(buf->transfer.noknem.data, buf->transfer.noknem.iovecs,
					   NULL,
					   buf->transfer.noknem.num_iovecs,
					   buf->transfer.noknem.offset,
					   to_copy);
	if (ret == PTL_FAIL) {
		WARN();
		return STATE_INIT_ERROR;
	}

	buf->transfer.noknem.length_left -= to_copy;
	buf->transfer.noknem.offset += to_copy;

	noknem->length = to_copy;

	/* Tell the target the data is ready. */
	__sync_synchronize();
	noknem->state = 2;

	return STATE_INIT_COPY_OUT;
}

static int init_copy_done(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	struct noknem *noknem = buf->transfer.noknem.noknem;

	/* Ack. */
	noknem->init_done = 1;
	__sync_synchronize();
	noknem->state = 2;

	/* Free the bounce buffer allocated in init_append_data. */
	ll_enqueue_obj_alien(&ni->shmem.bounce_buf.head->free_list,
						   buf->transfer.noknem.data,
						   ni->shmem.bounce_buf.head,
						   ni->shmem.bounce_buf.head->head_index0);

	/* Only called from the progress thread, so ni->shmem.noknem_lock is
	 * already locked. */
	list_del(&buf->list);

	if (buf->event_mask & XI_EARLY_SEND)
		return STATE_INIT_EARLY_SEND_EVENT;
	else if (buf->event_mask & XI_RECEIVE_EXPECTED)
		return STATE_INIT_WAIT_RECV;
	else
		return STATE_INIT_CLEANUP;
}

#elif WITH_TRANSPORT_UDP
static int init_copy_in(buf_t *buf)
{
	struct udp *udp = buf->transfer.udp.udp;
	ptl_size_t to_copy;
	int ret;

	udp->state = 1;

	/* Copy the data from the bounce buffer. */
	to_copy = udp->length;

	/* Target should never send more than requested. */
	assert(to_copy <= buf->transfer.udp.length_left);

	ret = iov_copy_in(buf->transfer.udp.data, buf->transfer.udp.iovecs,
					  NULL,
					  buf->transfer.udp.num_iovecs,
					  buf->transfer.udp.offset,
					  to_copy);
	if (ret == PTL_FAIL) {
		WARN();
		return STATE_INIT_ERROR;
	}

	if (udp->target_done)
		return STATE_INIT_COPY_DONE;

	buf->transfer.udp.length_left -= to_copy;
	buf->transfer.udp.offset += to_copy;

	/* Tell the target the data is ready. */
	__sync_synchronize();
	udp->state = 2;

	return STATE_INIT_COPY_IN;
}

static int init_copy_out(buf_t *buf)
{
	struct udp *udp = buf->transfer.udp.udp;
	ptl_size_t to_copy;
	int ret;

	if (udp->target_done)
		return STATE_INIT_COPY_DONE;

	udp->state = 1;

	/* Copy the data to the bounce buffer. */
	to_copy = buf->transfer.udp.data_length;
	if (to_copy > buf->transfer.udp.length_left)
		to_copy = buf->transfer.udp.length_left;

	ret = iov_copy_out(buf->transfer.udp.data, buf->transfer.udp.iovecs,
					   NULL,
					   buf->transfer.udp.num_iovecs,
					   buf->transfer.udp.offset,
					   to_copy);
	if (ret == PTL_FAIL) {
		WARN();
		return STATE_INIT_ERROR;
	}

	buf->transfer.udp.length_left -= to_copy;
	buf->transfer.udp.offset += to_copy;

	udp->length = to_copy;

	/* Tell the target the data is ready. */
	__sync_synchronize();
	udp->state = 2;

	return STATE_INIT_COPY_OUT;
}

static int init_copy_done(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	struct udp *udp = buf->transfer.udp.udp;

	/* Ack. */
	udp->init_done = 1;
	__sync_synchronize();
	udp->state = 2;

	/* Free the bounce buffer allocated in init_append_data. */
	ll_enqueue_obj_alien(&ni->udp.udp_buf.head->free_list,
						   buf->transfer.udp.data,
						   ni->udp.udp_buf.head,
						   ni->udp.udp_buf.head->head_index0);

	/* Only called from the progress thread, so ni->udp_lock is
	 * already locked. */
	list_del(&buf->list);

	if (buf->event_mask & XI_EARLY_SEND)
		return STATE_INIT_EARLY_SEND_EVENT;
	else if (buf->event_mask & XI_RECEIVE_EXPECTED)
		return STATE_INIT_WAIT_RECV;
	else
		return STATE_INIT_CLEANUP;
}

#else
static int init_copy_in(buf_t *buf) { abort(); }
static int init_copy_out(buf_t *buf) { abort(); }
static int init_copy_done(buf_t *buf) { abort(); }
#endif

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

	if ((buf->event_mask & XI_RECEIVE_EXPECTED) &&
			(buf->ni_fail != PTL_NI_UNDELIVERABLE))
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
	ack_hdr_t *hdr;

	assert(buf->event_mask & XI_RECEIVE_EXPECTED);

	if (buf->ni_fail == PTL_NI_UNDELIVERABLE) {
		/* The send completion failed. */
		return STATE_INIT_SEND_ERROR;
	}

	if (!buf->recv_buf)
		return STATE_INIT_WAIT_RECV;

	/* Release the put MD. */
	if (buf->put_md) {
		md_put(buf->put_md);
		buf->put_md = NULL;
	}

	/* get returned fields */
	hdr = (ack_hdr_t *)buf->recv_buf->data;
	buf->ni_fail = hdr->h1.ni_fail;

	/* moffset and mlength are only valid for certain reply/ack. The
	 * target has not set their values if it wasn't necessary. */
	if (hdr->h1.operation <= OP_ACK) {
		/* ACK and REPLY. */
		buf->moffset = le64_to_cpu(hdr->moffset);
		buf->matching_list = hdr->h1.matching_list;
	} else {
		/* Set a random invalid value. */
		buf->moffset = 0x77777777;
		buf->matching_list = 0x88;
	}

	if (hdr->h1.operation <= OP_CT_ACK) {
		/* ACK, CT_ACK and REPLY. */
		buf->mlength = le64_to_cpu(hdr->mlength);
	} else {
		/* Set a random invalid value. */
		buf->moffset = 0x66666666;
	}

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
	void *data = buf->data_in->immediate.data;
	ptl_size_t offset = buf->get_offset;
	ptl_size_t length = buf->mlength;

	assert(buf->data_in->data_fmt == DATA_FMT_IMMEDIATE);

	if (md->num_iov) {
		err = iov_copy_in(data, (ptl_iovec_t *)md->start, md->mr_list,
				      md->num_iov, offset, length);
		if (err)
			return STATE_INIT_ERROR;
	} else {
		void *start = md->start + offset;
#if IS_PPE
		mr_t *mr;

		if (md->ppe.mr_start)
			mr = md->ppe.mr_start;
		else {
			err = mr_lookup_app(obj_to_ni(md), start, length, &mr);
			if (err)
				return STATE_INIT_ERROR;
		}

		memcpy(addr_to_ppe(start, mr), data, length);

		if (!md->ppe.mr_start)
			mr_put(mr);
#else
		memcpy(start, data, length);
#endif
	}

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
	if (buf->event_mask & XI_SEND_EVENT)
		make_send_event(buf);

	if (buf->event_mask & XI_CT_SEND_EVENT)
		make_ct_send_event(buf);

	if (buf->ni_fail != PTL_NI_UNDELIVERABLE) {
		if (buf->event_mask & (XI_ACK_EVENT | XI_CT_ACK_EVENT))
			return STATE_INIT_ACK_EVENT;

		else if (buf->event_mask & (XI_REPLY_EVENT | XI_CT_REPLY_EVENT))
			return STATE_INIT_REPLY_EVENT;
	}

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
	ack_hdr_t *ack_hdr = (ack_hdr_t *)buf->recv_buf->data;

	/* Release the put MD before posting the ACK event. */
	if (buf->put_md) {
		md_put(buf->put_md);
		buf->put_md = NULL;
	}

	if (ack_hdr->h1.operation != OP_NO_ACK) {
		if (buf->event_mask & XI_ACK_EVENT)
			make_ack_event(buf);

		if (buf->event_mask & XI_CT_ACK_EVENT)
			make_ct_ack_event(buf);
	} else {
		buf->event_mask &= ~(XI_ACK_EVENT | XI_CT_ACK_EVENT);
	}

	return STATE_INIT_CLEANUP;
}

/**
 * @brief initiator reply event state.
 *
 * This state is reached if we can deliver
 * a reply or ct_reply event.
 *
 * @param[in] buf the request buf.
 * @return next state.
 */
static int reply_event(buf_t *buf)
{

#if WITH_TRANSPORT_UDP
	//immediate copies have already been performed, don't do anything more
	//otherwise we need to copy the reply into the md
	if (!buf->data_in){
		void *start = buf->get_md->start + buf->get_offset;
		memcpy(start, buf->recv_buf->transfer.udp.my_iovec.iov_base, buf->mlength);;
 	}
#endif	

	/* Release the get MD before posting the REPLY event. */
	md_put(buf->get_md);
	buf->get_md = NULL;

	if (buf->event_mask & XI_REPLY_EVENT)
		make_reply_event(buf);

	if (buf->event_mask & XI_CT_REPLY_EVENT)
		make_ct_reply_event(buf);

	return STATE_INIT_CLEANUP;
}

/**
 * @brief initiator error state.
 *
 * This state is reached when an error has
 * occured during the processing of the
 * request.
 *
 * @param[in] buf the request buf.
 */
static void error(buf_t *buf)
{
	/* TODO log the error */
}

/**
 * @brief initiator cleanup state.
 *
 * This state is reached when we are finished
 * processing a portals message.
 *
 * @param[in] buf the request buf.
 */
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
#if WITH_TRANSPORT_UDP
	if (atomic_read(&buf->recv_buf->obj.obj_ref.ref_cnt) < 1)
#endif
		buf_put(buf->recv_buf);
		buf->recv_buf = NULL;
	}

	conn_put(buf->conn);
}

/*
 * @brief initiator state machine.
 *
 * This state machine can be reentered one or more times
 * for each portals message. It is initially called from
 * one of the move APIs (e.g. Put/Get/...) with a buf
 * in the start state. It may exit the state machine for
 * one of the wait states (wait_conn, wait_comp, wait_recv)
 * and be reentered when the event occurs. The state
 * machine is protected by buf->mutex so only one thread at
 * a time can work on a given message. It can be executed
 * on an application thread, the IB connection thread or
 * a progress thread. The state machine drops the reference
 * to the buf corresponding to the original allocation
 * before leaving for the final time. If the caller into the
 * state machine needs to access the buf after the return
 * it should take an additional reference before calling
 * process_init and drop it after finishing accessing the buf.
 *
 * @param[in] buf the request buf.
 * @return status
 */
int process_init(buf_t *buf)
{
	int err = PTL_OK;
	enum init_state state;

	pthread_mutex_lock(&buf->mutex);

	state = buf->init_state;

	while (1) {
		ptl_info("[%d]%p: init state = %s\n",
				 getpid(), buf, init_state_name[state]);

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
#if WITH_TRANSPORT_SHMEM && !USE_KNEM
			if (state == STATE_INIT_COPY_IN ||
				state == STATE_INIT_COPY_OUT)
				goto exit;
#endif
			break;
		case STATE_INIT_COPY_IN:
			state = init_copy_in(buf);
			if (state == STATE_INIT_COPY_IN)
				goto exit;
			break;
		case STATE_INIT_COPY_OUT:
			state = init_copy_out(buf);
			if (state == STATE_INIT_COPY_OUT)
				goto exit;
			break;
		case STATE_INIT_COPY_DONE:
			state = init_copy_done(buf);
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
			error(buf);
			err = PTL_FAIL;
			state = STATE_INIT_CLEANUP;
			break;
		case STATE_INIT_CLEANUP:
			cleanup(buf);
			buf->init_state = STATE_INIT_DONE;
			pthread_mutex_unlock(&buf->mutex);
			buf_put(buf);
			return err;
		case STATE_INIT_DONE:
			/* This state handles the unusual case
			 * where the IB send completion occurs
			 * after the response from the target
			 * since we have already completed processing
			 * the request we do nothing here.
			 * The send completion handler will drop
			 * the final reference to the buf
			 * after we return. */
			goto exit;
		default:
			abort();
		}
	}

exit:
	/* we reach this point if we are leaving the state machine
	 * to wait for an external event such as an IB send completion. */
	buf->init_state = state;
	pthread_mutex_unlock(&buf->mutex);

	return PTL_OK;
}
