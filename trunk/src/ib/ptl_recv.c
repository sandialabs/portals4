/**
 * file ptl_recv.c
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
	[STATE_RECV_PACKET_RDMA]	= "recv_packet_rdma",
	[STATE_RECV_PACKET]		= "recv_packet",
	[STATE_RECV_DROP_BUF]		= "recv_drop_buf",
	[STATE_RECV_REQ]		= "recv_req",
	[STATE_RECV_INIT]		= "recv_init",
	[STATE_RECV_REPOST]		= "recv_repost",
	[STATE_RECV_ERROR]		= "recv_error",
	[STATE_RECV_DONE]		= "recv_done",
};

#if WITH_TRANSPORT_IB
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

		buf_list[i] = buf = (buf_t *)(uintptr_t)wc->wr_id;

		if (unlikely(wc->status != IBV_WC_SUCCESS &&
					 wc->status != IBV_WC_WR_FLUSH_ERR))
			WARN();

		/* The work request id might be NULL. That can happen when an
		 * inline send completed in error and no completion was
		 * requested. */
		if (!buf)
			continue;

		buf->length = wc->byte_len;

		if (unlikely(wc->status)) {
			if (buf->type == BUF_SEND) {
				buf->ni_fail = PTL_NI_UNDELIVERABLE;
				buf->recv_state = STATE_RECV_SEND_COMP;
			} else if (buf->type == BUF_RDMA)
				buf->recv_state = STATE_RECV_ERROR;
			else
				buf->recv_state = STATE_RECV_DROP_BUF;
		} else {
			if (buf->type == BUF_SEND)
				buf->recv_state = STATE_RECV_SEND_COMP;
			else if (buf->type == BUF_RDMA)
				buf->recv_state = STATE_RECV_RDMA_COMP;
			else if (buf->type == BUF_RECV)
				buf->recv_state = STATE_RECV_PACKET_RDMA;
			else
				buf->recv_state = STATE_RECV_ERROR;
		}
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
	/* If it's a completion that was not requested, then it's either
	 * coming from the send completion threshold mechanism (see
	 * conn->rdma.completion_threshold), or it was completed in
	 * error. We ignore the first type and let the second one pass
	 * through the state machine. */
	if (buf->event_mask & XX_SIGNALED ||
		buf->ni_fail == PTL_NI_UNDELIVERABLE) {
		/* Fox XI only, restart the initiator state machine. */
		struct hdr_common *hdr = (struct hdr_common *)buf->data;

		if (hdr->operation <= OP_SWAP) {
			buf->completed = 1;
			process_init(buf);
		}
		else if (hdr->operation == OP_RDMA_DISC) {
			conn_t * conn = buf->conn;

			pthread_mutex_lock(&conn->mutex);

			assert(conn->rdma.local_disc == 1);
			conn->rdma.local_disc = 2;

			/* If the remote side has already informed us of its
			 * intention to disconnect, then we can destroy that
			 * connection. */
			if (conn->rdma.remote_disc)
				disconnect_conn_locked(conn);

			pthread_mutex_unlock(&conn->mutex);
		}
	}

	buf_put(buf);

	return STATE_RECV_DONE;
}

/**
 * Process an rdma completion.
 *
 * @param rdma_buf the buffer that finished.
 *
 * @return the next state.
 */
static int rdma_comp(buf_t *rdma_buf)
{
	struct list_head temp_list;
	int err;
	buf_t *buf = rdma_buf->transfer.rdma.xxbuf;

	/* If it's a completion that was not requested, then it's coming
	 * from the send completion threshold mechanism (see
	 * conn->rdma.completion_threshold), and we ignore it. */
	if (!(rdma_buf->event_mask & XX_SIGNALED))
		return STATE_RECV_DONE;

	/* Take a ref on the XT since freeing all its rdma_buffers will also
	 * free it. */
	buf_get(buf);

	/* do not do this for indirect rdma sge lists */
	if (rdma_buf != buf) {
		atomic_dec(&buf->rdma.rdma_comp);

		PTL_FASTLOCK_LOCK(&buf->rdma.rdma_list_lock);
		list_cut_position(&temp_list, &buf->transfer.rdma.rdma_list, &rdma_buf->list);
		PTL_FASTLOCK_UNLOCK(&buf->rdma.rdma_list_lock);

		/* free the chain of rdma bufs */
		while(!list_empty(&temp_list)) {
			rdma_buf = list_first_entry(&temp_list, buf_t, list);
			list_del(&rdma_buf->list);
			buf_put(rdma_buf);
		}
	} else {
		buf->rdma_desc_ok = 1;
	}

	err = process_tgt(buf);
	buf_put(buf);

	if (err) {
		WARN();
		return STATE_RECV_ERROR;
	}

	return STATE_RECV_DONE;
}

/**
 * Process a received buffer. RDMA only.
 *
 * @param buf the receive buffer that finished.
 *
 * @return the next state.
 */
static int recv_packet_rdma(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);

	/* keep track of the number of buffers posted to the srq */
	atomic_dec(&ni->rdma.num_posted_recv);

	/* remove buf from pending receive list */
	assert(!list_empty(&buf->list));

	PTL_FASTLOCK_LOCK(&ni->rdma.recv_list_lock);
	list_del(&buf->list);
	PTL_FASTLOCK_UNLOCK(&ni->rdma.recv_list_lock);

	return STATE_RECV_PACKET;
}
#endif	/* WITH_TRANSPORT_IB */

/**
 * Process a received buffer. Common for RDMA and SHMEM.
 *
 * @param buf the receive buffer that finished.
 *
 * @return the next state.
 */
static int recv_packet(buf_t *buf)
{
#if WITH_TRANSPORT_UDP
	//with UDP we must reset this pointer to the data location on the local machine
	buf->data = &buf->internal_data;
#endif
	struct hdr_common *hdr = (struct hdr_common *)buf->data;

	/* sanity check received buffer */
	if (hdr->version != PTL_HDR_VER_1) {
		WARN();
		return STATE_RECV_DROP_BUF;
	}

	/* compute next state */
	if (hdr->operation <= OP_SWAP) {
		if (buf->length < sizeof(req_hdr_t))
			return STATE_RECV_DROP_BUF;
		else
			return STATE_RECV_REQ;
	}
	else if (hdr->operation >= OP_REPLY) {
		return STATE_RECV_INIT;
	}
	else {
#if WITH_TRANSPORT_IB
		/* Disconnect. */
		conn_t *conn;
		const req_hdr_t *hdr = (req_hdr_t *)buf->data;
		ptl_process_t initiator;

		/* get per conn info */
		initiator.phys.nid = le32_to_cpu(hdr->src_nid);
		initiator.phys.pid = le32_to_cpu(hdr->src_pid);

		conn = get_conn(buf->obj.obj_ni, initiator);

		pthread_mutex_lock(&conn->mutex);

		conn->rdma.remote_disc = 1;

		/* Remote side ready to disconnect, and if we are too, then
		 * disconnect. */
		if (conn->rdma.local_disc == 2)
			disconnect_conn_locked(conn);

		pthread_mutex_unlock(&conn->mutex);
#endif

		return STATE_RECV_DROP_BUF;
	}
	
 //REG: TODO case for UDP
#if WITH_TRANSPORT_UDP
		ptl_info("State machine, handling received UDP message\n");

#endif
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
	void *start;

	/* compute the data segments in the message
	 * note req packet data direction is wrt init */
	start = buf->data + sizeof(*hdr);
	if (hdr->h1.operand)
		start += sizeof(datatype_t);

	if (hdr->h1.data_in)
		buf->data_out = start;
	else
		buf->data_out = NULL;

	if (hdr->h1.data_out)
		buf->data_in = start + data_size(buf->data_out);
	else
		buf->data_in = NULL;

	buf->tgt_state = STATE_TGT_START;
	buf->type = BUF_TGT;

	/* Send message to target state machine. process_tgt must drop the
	 * buffer, so buf will not be valid after the call. */
#if WITH_TRANSPORT_UDP
	ptl_info("process received UDP request as target\n");
#endif
	err = process_tgt(buf);
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
static int recv_init(PPEGBL buf_t *buf)
{
	int err;
	buf_t *init_buf;
	ack_hdr_t *hdr = (ack_hdr_t *)buf->data;

	/* lookup the buf handle to get original buf */
	err = to_buf(MYGBL_ le32_to_cpu(hdr->h1.handle), &init_buf);
	if (err) {
		WARN();
		ptl_info("cannot find buffer via buffer handle: %i %p\n",hdr->h1.handle,init_buf);
		return STATE_RECV_DROP_BUF;
	}

	/* compute data segments in response message */
	if (hdr->h1.data_in)
		init_buf->data_out = (data_t *)(buf->data + sizeof(ack_hdr_t));
	else
		init_buf->data_out = NULL;

	if (hdr->h1.data_out)
		init_buf->data_in = (data_t *)(buf->data  + sizeof(ack_hdr_t) +
					  data_size(init_buf->data_out));
	else
		init_buf->data_in = NULL;

	init_buf->recv_buf = buf;

#if WITH_TRANSPORT_UDP	
	ni_t * ni = obj_to_ni(buf);

	if (atomic_read(&ni->udp.self_recv) > 0){
		if (hdr->h1.operation == OP_CT_ACK || OP_ACK || OP_OC_ACK){
			init_buf->init_state = STATE_INIT_ACK_EVENT;
			err = process_init(init_buf);
		}
	}
	else{
#endif
		/* Note: process_init must drop recv_buf, so buf will not be valid
		 * after the call. */
		err = process_init(init_buf);
#if WITH_TRANSPORT_UDP
	}
#endif

	if (err)
		WARN();

#if WITH_TRANSPORT_UDP
	if (atomic_read(&init_buf->obj.obj_ref.ref_cnt) > 1)
#endif
		buf_put(init_buf);	/* from to_buf() */

	return STATE_RECV_REPOST;
}

#if WITH_TRANSPORT_IB
/**
 * Repost receive buffers to srq.
 *
 * @param buf the received buffer.
 *
 * @return the next state.
 */
static int recv_repost(ni_t *ni)
{
	int num_bufs;

	/* compute the available room in the srq */
	num_bufs = ni->iface->cap.max_srq_wr - atomic_read(&ni->rdma.num_posted_recv);

	/* if rooms exceeds threshold repost that many buffers
	 * this should reduce the number of receive queue doorbells
	 * which should improve performance */
	if (num_bufs > get_param(PTL_SRQ_REPOST_SIZE))
		ptl_post_recv(ni, get_param(PTL_SRQ_REPOST_SIZE));

	return STATE_RECV_DONE;
}
#endif

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

#if WITH_TRANSPORT_IB
/**
 * Completion queue polling thread.
 *
 * @param arg opaque pointer to ni.
 */
static void process_recv_rdma(ni_t *ni, buf_t *buf)
{
	enum recv_state state = buf->recv_state;

	while(1) {
		ptl_info("tid:%lx buf:%p: state = %s\n",
				 pthread_self(), buf, recv_state_name[state]);
		switch (state) {
		case STATE_RECV_SEND_COMP:
			state = send_comp(buf);
			break;
		case STATE_RECV_RDMA_COMP:
			state = rdma_comp(buf);
			break;
		case STATE_RECV_PACKET_RDMA:
			state = recv_packet_rdma(buf);
			break;
		case STATE_RECV_PACKET:
			state = recv_packet(buf);
			break;
		case STATE_RECV_REQ:
			state = recv_req(buf);
			break;
		case STATE_RECV_INIT:
			state = recv_init(MYNIGBL_ buf);
			break;
		case STATE_RECV_REPOST:
			state = recv_repost(ni);
			break;
		case STATE_RECV_DROP_BUF:
			state = recv_drop_buf(buf);
			break;
		case STATE_RECV_ERROR:
			if (buf) {
				buf_put(buf);
				ni->num_recv_errs++;
			}
			goto done;
			break;
		case STATE_RECV_DONE:
			goto done;
			break;
		default:
			abort();
		}
	}
 done:
	return;
}

void progress_thread_rdma(ni_t *ni)
{
	const int num_wc = get_param(PTL_WC_COUNT);
	buf_t *buf_list[num_wc];
	int i;
	int num_buf;
	struct ibv_wc wc_list[num_wc];

	num_buf = comp_poll(ni, num_wc, wc_list, buf_list);

	for (i = 0; i < num_buf; i++) {
		if (buf_list[i])
			process_recv_rdma(ni, buf_list[i]);
	}
}
#endif

#if 0
#if WITH_TRANSPORT_UDP
//TODO: Fill-in this function showing how udp does its
//      recvfrom to retrieve the sent buffer; remember to
//      increment relevant atomics
static void progress_thread_udp(ni_t *ni)
{
	ssize_t nbytes;
	char data[BUF_DATA_SIZE];

	nbytes = recvfrom(ni->udp.s, data, BUF_DATA_SIZE, 0, 
					  NULL, NULL);

	if (nbytes != -1) {
		if (nbytes > 0) {
			buf_t *buf;
			int err;

			/* Allocate a buffer to copy the data in. This is bad; we
			 * should have a buf readily available for the recvfrom
			 * call to save a copy. TODO. */
			err = buf_alloc(ni, &buf);
			if (err) {
				WARN();
			} else {
				memcpy(buf->data, data, nbytes);
				buf->length = nbytes;
				process_recv_mem(ni, buf);
			}
		} else {
			/* Can we receive 0 bytes ? */
			abort();
		}
	}
}
#endif
#endif

#if WITH_TRANSPORT_UDP
static void progress_thread_udp(ni_t *ni)
{
//#if WITH_TRANSPORT_SHMEM
	/* Socket connection.*/
	if (ni->udp.dest_addr && ni->udp.map_done != 0) {
		int err;
		buf_t* udp_buf;

		udp_buf = udp_receive(ni);
                if (&udp_buf->mutex == NULL){
		  pthread_mutex_init(&udp_buf->mutex,NULL);
		}
	
                if (udp_buf != NULL){
                 	ptl_info("UDP progress thread, received data: %p type:%i\n",udp_buf,udp_buf->type);
		}
 

		if (udp_buf != NULL) {
		 	ptl_info("received UDP buf type: %i, SEND=%i RETURN=%i RECV=%i CONN_REQ=%i CONN_REP=%i\n",udp_buf->type,BUF_UDP_SEND,BUF_UDP_RETURN,BUF_UDP_RECEIVE,BUF_UDP_CONN_REQ,BUF_UDP_CONN_REP);	
			
			switch(udp_buf->type) {
			case BUF_UDP_SEND: {
				buf_t *buf;

				/* Mark it for return now. The target state machine might
				 * change its type to BUF_SHMEM_SEND. */
				udp_buf->type = BUF_UDP_RETURN;

				err = buf_alloc(ni, &buf);
				if (err) {
					WARN();
				} else {
					buf->data = udp_buf->internal_data;
					buf->length = udp_buf->length;
					buf->udp_buf = udp_buf;
					INIT_LIST_HEAD(&buf->list);
					process_recv_udp(ni, buf);
				}

//#if WITH_TRANSPORT_SHMEM && !USE_KNEM
				/* Don't send back if it's on the noknem list. */
				if (!list_empty(&buf->list))
					break;
//#endif

				if (udp_buf->type == BUF_UDP_SEND ||
					udp_buf->udp.dest_addr != ni->udp.dest_addr) {
					/* Requested to send the buffer back, or not the
					 * owner. Send the buffer back in both cases. */
					//shmem_enqueue(ni, udp_buf, udp_buf->udp.index_owner);
					//udp_send(ni, udp_buf, udp_buf->udp.index_owner);
					udp_send(ni, udp_buf, &udp_buf->dest.udp.dest_addr);
				} else {
					/* It was returned to us with a message from a remote
					 * rank. From send_message_udp(). */
					buf_put(udp_buf);
				}
			
				break;
			}

        		case BUF_UDP_RECEIVE: {
				//REG: TODO: fix UDP process recv
				ptl_info("processing received data message\n");
				if (udp_buf->put_ct != NULL){
					ptl_info("putct is : %p \n", udp_buf->put_ct);
				}
 				pthread_mutex_init(&udp_buf->mutex,NULL);
				udp_buf->obj.obj_ni = ni;
				udp_buf->conn = get_conn(ni, ni->id);
				udp_buf->conn->state = CONN_STATE_CONNECTED;
				process_recv_udp(ni,udp_buf);
				break;
  			} 

			case BUF_UDP_RETURN: {
				/* Buffer returned to us by remote node. */
				assert(udp_buf->udp.dest_addr == ni->udp.dest_addr);

				/* From send_message_udp(). */
				break;
     			}

			case BUF_UDP_CONN_REQ:{
			   	
				ptl_info("UDP connection request received, handling now.... \n");
				
				udp_buf->type = BUF_UDP_CONN_REP;
			        
				struct udp_conn_msg msg;
        			msg.msg_type = cpu_to_le16(UDP_CONN_MSG_REP);
        			msg.port = ntohs(ni->udp.src_port);
        			msg.req.options = ni->options;
        			msg.req.src_id = ni->id;
        			
        			udp_buf->transfer.udp.conn_msg = msg;
        			udp_buf->length = (sizeof(buf_t));	
				
				//send back to the requesting address
				udp_buf->udp.dest_addr = &udp_buf->udp.src_addr;
				udp_buf->dest.udp.dest_addr = udp_buf->udp.src_addr;			
				//udp_buf->udp.src_addr.sin_addr.s_addr = nid_to_addr(ni->id.phys.nid);
				//udp_buf->udp.src_addr.sin_port = pid_to_port(ni->id.phys.pid);

				udp_send(ni, udp_buf, udp_buf->udp.dest_addr);
				//REG: Note: this assumes that we have a reliable transport, otherwise things can go wrong here
				//udp_buf->conn->state = CONN_STATE_CONNECTED;
				ptl_info("Connection request reply sent, connection valid. \n");
				//free(udp_buf);
                                break;	
			
			}

			case BUF_UDP_CONN_REP:{
				ptl_info("UDP connection reply received, validating connection \n");
				udp_buf->conn->state = CONN_STATE_CONNECTED;
				
			        udp_buf->conn->udp.dest_addr = udp_buf->udp.src_addr;
			  									   	
				//ptl_info("connection established to: %s:%i from %s:%i \n",inet_ntoa(udp_buf->udp.dest_addr->sin_addr),ntohs(udp_buf->udp.dest_addr->sin_port),inet_ntoa(ni->iface->udp.sin.sin_addr),ntohs(ni->iface->udp.sin.sin_port));
			
				//ptl_info("local address: %s:%i \n",inet_ntoa(ni->iface->udp.sin.sin_addr),ntohs(ni->iface->udp.sin.sin_port));;
				int result;
				//ptl_info("release wait on %p \n",&udp_buf->conn->move_wait);
				//release the thread waiting on the establishment of a connection
				while (1) {
					result = atomic_read(&udp_buf->conn->udp.is_waiting);
					if (result > 0){
						ptl_info("release wait on %p \n",&udp_buf->conn->move_wait);
						pthread_cond_broadcast(&udp_buf->conn->move_wait);
						atomic_set(&udp_buf->conn->udp.is_waiting, 0);
						break;
					}
				}
				ptl_info("connection valid for reply\n");
				//free(udp_buf);
				break;
			}

			default:
				/* Should not happen. */
				abort();
			}
			//if a buffer was allocated for the recv, free it
			if (atomic_read(&ni->udp.self_recv) == 0){
				ptl_info("free recv buf \n");
				free(udp_buf);
			}
			//if we sent something to ourselves, flag it as processed
			else{
				atomic_dec(&ni->udp.self_recv);
			}
		}
	}
	
//#endif

//TODO: do we need this for UDP?
//#if WITH_TRANSPORT_SHMEM && !USE_KNEM
/*	struct list_head *l, *t;

	// TODO: instead of having a lock, the initiator should send
	//  the buf to itself, and on receiving it, the progress thread
	//  will put it on the list. That way, only the progress thread
	//  has access to the list. 
	PTL_FASTLOCK_LOCK(&ni->udp_lock);

	list_for_each_safe(l, t, &ni->udp_list) {
		buf_t *buf = list_entry(l, buf_t, list);
		struct udp *udp = buf->transfer.udp.udp;

		if (buf->transfer.udp.transfer_state_expected == udp->state) {
			if (udp->state == 0)
				process_init(buf);
			else if (udp->state == 2) {
				if (udp->init_done) {
					buf_t *udp_buf = buf->udp_buf;

					// The transfer is now done. Remove from
					// udp_list. 
					list_del(&buf->list);

					process_tgt(buf);

					if (udp_buf->type == BUF_UDP_SEND ||
						udp_buf->udp.dest_addr != ni->udp.dest_addr) {
						// Requested to send the buffer back, or not the
						// owner. Send the buffer back in both cases. 
						//shmem_enqueue(ni, udp_buf, udp_buf->udp.index_owner);
						//udp_send(ni, udp_buf, udp_buf->udp.index_owner);
						udp_send(ni, udp_buf, &udp_buf->dest.udp.dest_addr);
					} else {
						// It was returned to us with a message from a remote
						// rank. From send_message_udp(). 
						buf_put(udp_buf);
					}

				} else {
					process_tgt(buf);
				}
			}
		}
	}

	PTL_FASTLOCK_UNLOCK(&ni->udp_lock);
//#endif*/
}
#endif

#if WITH_TRANSPORT_SHMEM || IS_PPE || WITH_TRANSPORT_UDP
/**
 * Process a received message in shared memory.
 *
 * @param ni the ni to poll.
 * @param buf the received buffer.
 */
void process_recv_mem(ni_t *ni, buf_t *buf)
{
	enum recv_state state = STATE_RECV_PACKET;

	while(1) {
		ptl_info("tid:%lx buf:%p: recv state local = %s\n",
				 (long unsigned int)pthread_self(), buf,
				   recv_state_name[state]);
		switch (state) {
		case STATE_RECV_PACKET:
			state = recv_packet(buf);
			break;
		case STATE_RECV_REQ:
			state = recv_req(buf);
			break;
		case STATE_RECV_INIT:
			state = recv_init(MYNIGBL_ buf);
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

		case STATE_RECV_PACKET_RDMA:
		case STATE_RECV_SEND_COMP:
		case STATE_RECV_RDMA_COMP:
			/* Not reachable. */
			abort();
		}
	}
exit:
	return;
}
#endif

#if WITH_TRANSPORT_UDP
/**
 * Process a received message in UDP.
 *
 * @param ni the ni to poll.
 * @param buf the received buffer.
 */
void process_recv_udp(ni_t *ni, buf_t *buf)
{
	enum recv_state state = STATE_RECV_PACKET;

//REG: TODO: process receive packets to deliver data.
	while(1) {
		ptl_info("tid:%lx buf:%p: recv state local = %s\n",
				 (long unsigned int)pthread_self(), buf,
				   recv_state_name[state]);
		switch (state) {
		case STATE_RECV_PACKET:
			state = recv_packet(buf);
			ptl_info("recv_packet returned state: %s\n",recv_state_name[state]);
			break;
		case STATE_RECV_REQ:
			state = recv_req(buf);
			ptl_info("recv_req returned state: %s\n",recv_state_name[state]);
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

		case STATE_RECV_PACKET_RDMA:
		case STATE_RECV_SEND_COMP:
		case STATE_RECV_RDMA_COMP:
			/* Not reachable. */
			abort();
		}
	}
exit:
	return;
}
#endif

#if !IS_PPE
/**
 * Progress thread. Waits for ib, udp, and/or shared memory messages.
 *
 * @param arg opaque pointer to ni.
 */

static void *progress_thread(void *arg)
{
	ni_t *ni = arg;

	while(!ni->catcher_stop
#if WITH_TRANSPORT_SHMEM
		  || atomic_read(&ni->sbuf_pool.count)
#endif
		  ) {

		progress_thread_rdma(ni);

		progress_thread_udp(ni);

#if WITH_TRANSPORT_SHMEM
		/* Shared memory. Physical NIs don't have a receive queue. */
		if (ni->shmem.queue) {
			int err;
			buf_t *shmem_buf;

			shmem_buf = shmem_dequeue(ni);

			if (shmem_buf) {
				switch(shmem_buf->type) {
				case BUF_SHMEM_SEND: {
					buf_t *buf;

					/* Mark it for return now. The target state machine might
					 * change its type to BUF_SHMEM_SEND. */
					shmem_buf->type = BUF_SHMEM_RETURN;

					err = buf_alloc(ni, &buf);
					if (err) {
						WARN();
					} else {
						buf->data = shmem_buf->internal_data;
						buf->length = shmem_buf->length;
						buf->mem_buf = shmem_buf;
						INIT_LIST_HEAD(&buf->list);
						process_recv_mem(ni, buf);
					}

#if WITH_TRANSPORT_SHMEM && !USE_KNEM
					/* Don't send back if it's on the noknem list. */
					if (!list_empty(&buf->list))
						break;
#endif

					if (shmem_buf->type == BUF_SHMEM_SEND ||
						shmem_buf->shmem.index_owner != ni->mem.index) {
						/* Requested to send the buffer back, or not the
						 * owner. Send the buffer back in both cases. */
						shmem_enqueue(ni, shmem_buf, shmem_buf->shmem.index_owner);
					} else {
						/* It was returned to us with a message from a remote
						 * rank. From send_message_shmem(). */
						buf_put(shmem_buf);
					}
				}
					break;

				case BUF_SHMEM_RETURN:
					/* Buffer returned to us by remote node. */
					assert(shmem_buf->shmem.index_owner == ni->mem.index);

					/* From send_message_shmem(). */
					buf_put(shmem_buf);
					break;

				default:
					/* Should not happen. */
					abort();
				}
			}
		}
#endif

#if WITH_TRANSPORT_SHMEM && !USE_KNEM
		struct list_head *l, *t;

		/* TODO: instead of having a lock, the initiator should send
		 * the buf to itself, and on receiving it, the progress thread
		 * will put it on the list. That way, only the progress thread
		 * has access to the list. */
		PTL_FASTLOCK_LOCK(&ni->shmem.noknem_lock);

		list_for_each_safe(l, t, &ni->shmem.noknem_list) {
			buf_t *buf = list_entry(l, buf_t, list);
			struct noknem *noknem = buf->transfer.noknem.noknem;

			if (buf->transfer.noknem.transfer_state_expected == noknem->state) {
				if (noknem->state == 0)
					process_init(buf);
				else if (noknem->state == 2) {
					if (noknem->init_done) {
						buf_t *shmem_buf = buf->mem_buf;

						/* The transfer is now done. Remove from
						 * noknem_list. */
						list_del(&buf->list);

						process_tgt(buf);

						if (shmem_buf->type == BUF_SHMEM_SEND ||
							shmem_buf->shmem.index_owner != ni->mem.index) {
							/* Requested to send the buffer back, or not the
							 * owner. Send the buffer back in both cases. */
							shmem_enqueue(ni, shmem_buf, shmem_buf->shmem.index_owner);
						} else {
							/* It was returned to us with a message from a remote
							 * rank. From send_message_shmem(). */
							buf_put(shmem_buf);
						}

					} else {
						process_tgt(buf);
					}
				}
			}
		}

		PTL_FASTLOCK_UNLOCK(&ni->shmem.noknem_lock);
#endif
	}

	return NULL;
}

/* Add a progress thread. */
int start_progress_thread(ni_t *ni)
{
	int ret;

	ret = pthread_create(&ni->catcher, NULL, progress_thread, ni);
	if (ret) {
		WARN();
		ret = PTL_FAIL;
	} else {
		ni->has_catcher = 1;

		ret = PTL_OK;
	}

	return ret;
}

/* Stop the progress thread. */
void stop_progress_thread(ni_t *ni)
{
	if (ni->has_catcher) {
		ni->catcher_stop = 1;
		pthread_join(ni->catcher, NULL);
		ni->has_catcher = 0;
	}
}

#endif
