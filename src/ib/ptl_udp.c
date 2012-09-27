/**
 * @file ptl_udp.c
 *
 * @brief UDP transport operations used by target.
 */

#include "ptl_loc.h"

/**
 * @brief Send a message using UDP.
 *
 * @param[in] buf
 * @param[in] signaled
 *
 * @return status
 */
static int send_message_udp(buf_t *buf, int from_init)
{
	ssize_t ret;

	/* Keep a reference on the buffer so it doesn't get freed. */
	assert(buf->obj.obj_pool->type == POOL_BUF);
	buf_get(buf);
	buf->type = BUF_UDP_SEND;

	buf->udp.dest_addr = buf->obj.obj_ni->udp.dest_addr;

	// increment the sequence number associated with the
	// send-side of this connection
	atomic_inc(&buf->conn->udp.send_seq);
	udp_send(buf->obj.obj_ni, buf, buf->dest.udp.dest_addr);

	//assert(ret == buf->length);

	return PTL_OK;
}

static void udp_set_send_flags(buf_t *buf, int can_signal)
{
	/* The data is always in the buffer. */
	buf->event_mask |= XX_INLINE;
}

static void attach_bounce_buffer(buf_t *buf, data_t *data)
{
	void *bb;
	ni_t *ni = obj_to_ni(buf);

	while ((bb = ll_dequeue_obj_alien(&ni->udp.udp_buf.head->free_list,
					ni->udp.udp_buf.head,
					ni->udp.udp_buf.head->head_index0)) == NULL)
		SPINLOCK_BODY();

	buf->transfer.udp.data = bb;
	buf->transfer.udp.data_length = ni->udp.udp_buf.buf_size;
	buf->transfer.udp.bounce_offset = bb - (void *)ni->udp.udp_buf.head;

	data->udp.bounce_offset = buf->transfer.udp.bounce_offset;
}

static void append_init_data_udp_iovec(data_t *data, md_t *md,
										  int iov_start, int num_iov,
										  ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_UDP;

	data->udp.target_done = 0;
	data->udp.init_done = 0;

	buf->transfer.udp.transfer_state_expected = 0; /* always the initiator here */
	buf->transfer.udp.udp = &data->udp;

	//TODO: what are bounce buffers and are they relevant for UDP
	attach_bounce_buffer(buf, data);

	buf->transfer.udp.num_iovecs = num_iov;
	buf->transfer.udp.iovecs = &((ptl_iovec_t *)md->start)[iov_start];
	buf->transfer.udp.offset = 0;

	buf->transfer.udp.length_left = length;

	buf->length += sizeof(*data);
}

static void append_init_data_udp_direct(data_t *data, mr_t *mr, void *addr,
										   ptl_size_t length, buf_t *buf)
{
	data->data_fmt = DATA_FMT_UDP;

	data->udp.target_done = 0;
	data->udp.init_done = 0;

	buf->transfer.udp.transfer_state_expected = 0; /* always the initiator here */
	buf->transfer.udp.udp = &data->udp;

	//TODO: what are bounce buffers and are they relevant for UDP
	attach_bounce_buffer(buf, data);

	/* Describes local memory */
	buf->transfer.udp.my_iovec.iov_base = addr;
	buf->transfer.udp.my_iovec.iov_len = length;

	buf->transfer.udp.num_iovecs = 1;
	buf->transfer.udp.iovecs = &buf->transfer.udp.my_iovec;
	buf->transfer.udp.offset = 0;

	buf->transfer.udp.length_left = length;

	buf->length += sizeof(*data);
}

/**
 * @brief Build and append a data segment to a request message.
 *
 * @param[in] md the md that contains the data
 * @param[in] dir the data direction, in or out
 * @param[in] offset the offset into the md
 * @param[in] length the length of the data
 * @param[in] buf the buf the add the data segment to
 * @param[in] type the transport type
 *
 * @return status
 */
//TODO: Code review needs to take place here (UO & REB)
static int init_prepare_transfer_udp(md_t *md, data_dir_t dir, ptl_size_t offset,
					ptl_size_t length, buf_t *buf)
{
	int err = PTL_OK;
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	data_t *data = (data_t *)(buf->data + buf->length);
	int num_sge;

	//TODO: not sure if we need these iov_ stuff
	ptl_size_t iov_start = 0;
	ptl_size_t iov_offset = 0;

	if (length <= get_param(PTL_MAX_INLINE_DATA)) {
		err = append_immediate_data(md->start, NULL, md->num_iov, dir, offset, length, buf);
	}
	else {
		if (md->options & PTL_IOVEC) {
			ptl_iovec_t *iovecs = md->start;

			/* Find the index and offset of the first IOV as well as the
			 * total number of IOVs to transfer. */
			num_sge = iov_count_elem(iovecs, md->num_iov, offset, 
						 length, &iov_start, &iov_offset);
			if (num_sge < 0) {
				WARN();
				return PTL_FAIL;
			}

			append_init_data_udp_iovec(data, md, iov_start, num_sge, length, buf);

			/* @todo this is completely bogus */
			/* Adjust the header offset for iov start. */
			hdr->roffset = cpu_to_le64(le64_to_cpu(hdr->roffset) - iov_offset);
			abort();//todo
		} else {
			void *addr;
			mr_t *mr;
			ni_t *ni = obj_to_ni(md);

			addr = md->start + offset;
			err = mr_lookup_app(ni, addr, length, &mr);
			if (!err) {
				buf->mr_list[buf->num_mr++] = mr;

				append_init_data_udp_direct(data, mr, addr, length, buf);
			}
			abort();//todo
		}
	}

	if (!err)
		assert(buf->length <= BUF_DATA_SIZE);

	return err;
}

//TODO: We need to implement the function do_upd_transfer
//      correctly for UDP transfers...shouldn't this be 
//      just a regular use of sendto/recvfrom routine (UO & REB)
static int do_udp_transfer(buf_t *buf)
{
	struct udp *udp = buf->transfer.udp.udp;
	ptl_size_t *resid = buf->rdma_dir == DATA_DIR_IN ?
		&buf->put_resid : &buf->get_resid;
	ptl_size_t to_copy;
	int err;

	if (udp->state != 2)
		return PTL_OK;

	if (udp->init_done) {
		assert(udp->target_done);
		return PTL_OK;
	}

	udp->state = 3;

	if (*resid) {
		if (buf->rdma_dir == DATA_DIR_IN) {
			to_copy = udp->length;
			if (to_copy > *resid)
				to_copy = *resid;

			err = iov_copy_in(buf->transfer.udp.data, buf->transfer.udp.iovecs,
							  NULL,
							  buf->transfer.udp.num_iovecs,
							  buf->transfer.udp.offset, to_copy);
		} else {
			to_copy = buf->transfer.udp.data_length;
			if (to_copy > *resid)
				to_copy = *resid;

			err = iov_copy_out(buf->transfer.udp.data, buf->transfer.udp.iovecs,
							   NULL,
							   buf->transfer.udp.num_iovecs,
							   buf->transfer.udp.offset, to_copy);

			udp->length = to_copy;
		}

		/* That should never happen since all lengths were properly
		 * computed before entering. */
		assert(err == PTL_OK);

	} else {
		/* Dropped case. Nothing to transfer, but the buffer must
		 * still be returned. */
		to_copy = 0;
		err = PTL_OK;
	}

	buf->transfer.udp.offset += to_copy;
	*resid -= to_copy;

	if (*resid == 0)
		udp->target_done = 1;

	/* Tell the initiator the buffer is his again. */
	__sync_synchronize();
	udp->state = 0;

	return err;
}

static int udp_tgt_data_out(buf_t *buf, data_t *data)
{
	int next;
	ni_t *ni = obj_to_ni(buf);

	if (data->data_fmt != DATA_FMT_UDP) {
		assert(0);
		WARN();
		next = STATE_TGT_ERROR;
	}

fprintf(stderr,"udp_tgt_data_out sets transfer_state_expected buf %p\n",buf);
	buf->transfer.udp.transfer_state_expected = 2; /* always the target here */
	buf->transfer.udp.udp = &data->udp;

	if ((buf->rdma_dir == DATA_DIR_IN && buf->put_resid) ||
		(buf->rdma_dir == DATA_DIR_OUT && buf->get_resid)) {
		if (buf->me->options & PTL_IOVEC) {
			buf->transfer.udp.num_iovecs = buf->me->length;
			buf->transfer.udp.iovecs = buf->me->start;
		} else {
			buf->transfer.udp.num_iovecs = 1;
			buf->transfer.udp.iovecs = &buf->transfer.udp.my_iovec;

			buf->transfer.udp.my_iovec.iov_base = buf->me->start;
			buf->transfer.udp.my_iovec.iov_len = buf->me->length;
		}
	}

	//TODO: We need to get the buffer and the length content put in the buf->transfer correctly
	//      (UO & REB)
	buf->transfer.udp.offset = buf->moffset;
	buf->transfer.udp.length_left = buf->get_resid; // the residual length left
	buf->transfer.udp.data = (void *)ni->udp.udp_buf.head + data->udp.bounce_offset;
	//buf->transfer.udp.data = (void *)ni->udp.udp_buf.head;
	buf->transfer.udp.data_length = ni->udp.udp_buf.buf_size;

	return STATE_TGT_START_COPY;
}

/**
 * @brief send a buf to a pid using UDP socket.
 *
 * @param[in] ni the network interface
 * @param[in] buf the buf
 * @param[in] dest the destination socket info
 */
void udp_send(ni_t *ni, buf_t *buf, struct sockaddr_in *dest)
{
	int err;

	// TODO: Don't know the meaning of this (UO & REB)
	buf->obj.next = NULL;

	// first, send the actual buffer
	err = sendto(ni->iface->udp.connect_s, (char*)buf, sizeof(buf_t), 0, dest, 
	             sizeof((struct sockaddr_in)*dest));
	if(err == -1) {
		WARN();
		ptl_warn("error sending buffer to socket");
		return;
	}
}

/**
 * @brief receive a buf using a UDP socket.
 *
 * @param[in] ni the network interface.
 */
buf_t *udp_receive(ni_t *ni)
{
	int err;
	struct sockaddr_in temp_sin;
	socklen_t lensin = sizeof(temp_sin);	

	buf_t * thebuf = (buf_t*)calloc(1, sizeof(buf_t));

	err = recvfrom(ni->iface->udp.connect_s, thebuf, sizeof(buf_t), 0, &temp_sin, &lensin);
	if(err == -1) {
                free(thebuf);
		WARN();
		ptl_warn("error receiving main buffer from socket");
		return NULL;
	}

	return (buf_t *)thebuf;
}

/* change the state of conn; we are now connected (UO & REB) */
void PtlSetMap_udp(ni_t *ni, ptl_size_t map_size, const ptl_process_t *mapping)
{
	int i;

	for (i = 0; i < map_size; i++) {
		conn_t *conn;
		ptl_process_t id;

		//if (get_param(PTL_ENABLE_UDP)) {
			/* Connect local ranks through XPMEM or SHMEM. */
			id.rank = i;
			conn = get_conn(ni, id);
			if (!conn) {
				/* It's hard to recover from here. */
				WARN();
				abort();
				return;
			}

#if WITH_TRANSPORT_UDP
			conn->transport = transport_udp;
#else
#error
#endif
			conn->state = CONN_STATE_CONNECTED;

			conn_put(conn);			/* from get_conn */
		//}
	}
}

/**
 * @param[in] ni
 * @param[in] conn
 *
 * @return status
 *
 * conn must be locked
 */
static int init_connect_udp(ni_t *ni, conn_t *conn)
{
	int ret;

	assert(conn->transport.type == CONN_TYPE_UDP);

	if (ni->shutting_down)
		return PTL_FAIL;

	conn_get(conn);

	assert(conn->state == CONN_STATE_DISCONNECTED);

	/* Send the connect request. */
	struct udp_conn_msg msg;
	msg.msg_type = cpu_to_le16(UDP_CONN_MSG_REQ);
	msg.port = cpu_to_le16(ni->udp.src_port);
	msg.req.options = ni->options;
	msg.req.src_id = ni->id;
	msg.req_cookie = (uintptr_t)conn;

	/* Send the request to the listening socket on the remote node. */
	ret = sendto(ni->iface->udp.connect_s, &msg, sizeof(msg), 0,
				 &conn->sin, sizeof(conn->sin));
	if (ret == -1) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

struct transport transport_udp = {
	.type = CONN_TYPE_UDP,
	.buf_alloc = buf_alloc,
	.init_connect = init_connect_udp,
	.send_message = send_message_udp,
	.set_send_flags = udp_set_send_flags,
	.init_prepare_transfer = init_prepare_transfer_udp,
	//.post_tgt_dma = do_udp_transfer,
	//.tgt_data_out = udp_tgt_data_out,
};

struct transport_ops transport_remote_udp = {
	.init_iface = init_iface_udp,
	.NIInit = PtlNIInit_UDP,
};
