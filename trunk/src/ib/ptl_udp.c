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
	/* Keep a reference on the buffer so it doesn't get freed. */
	//assert(buf->obj.obj_pool->type == POOL_BUF);
	//assert(buf->obj.obj_pool->type == (BUF_UDP_SEND || BUF_UDP_RETURN || BUF_UDP_CONN_REQ || BUF_UDP_CONN_REP));
	buf_get(buf);

	//ptl_info("buffer data pointer is: %p %i\n",buf->data_out,buf->data_out->udp.length);

	buf->type = BUF_UDP_SEND;
	
	//buf->recv_buf = 0;
	      
        // increment the sequence number associated with the
	// send-side of this connection
	atomic_inc(&buf->conn->udp.send_seq);
        udp_send(buf->obj.obj_ni, buf, &buf->dest.udp.dest_addr);
  	
	buf_put(buf);

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
	buf->transfer.udp.data = (unsigned char *)&data;

	//TODO: what are bounce buffers and are they relevant for UDP
	//REG
	//attach_bounce_buffer(buf, data);

	//REG: Is there a corresponding send_msg for this? if so where?
	/* Describes local memory */
	buf->transfer.udp.my_iovec.iov_base = addr;
	buf->transfer.udp.my_iovec.iov_len = length;
	buf->data_out->udp.iov_data.iov_base = malloc(sizeof(struct iovec));

	memcpy(buf->data_out->udp.iov_data.iov_base, addr, sizeof(addr));
	buf->data_out->udp.length = length;
	buf->data_out->udp.iov_data.iov_len = length;

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
	data_t *data = (data_t *)(buf->data + sizeof(req_hdr_t));//(buf->internal_data);//(buf->data + sizeof(req_hdr_t));//buf->length);
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
				buf->mr_list[buf->num_mr + 1] = mr;
				buf->num_mr++;
				append_init_data_udp_direct(data, mr, addr, length, buf);
			}
			//REG: TODO handle large messages, currently it creates a IO vector for the message and places all of it in the IO vec.
			//abort();//todo
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

	if (*resid == 0) {
		udp->init_done = 1;
		udp->target_done = 1;
	}

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

        ptl_info("udp_tgt_data_out sets transfer_state_expected buf %p\n",buf);
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
	buf->transfer.udp.data_length = buf->rlength;//ni->udp.udp_buf.buf_size;

	return STATE_TGT_UDP;//STATE_TGT_START_COPY;
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

	//ptl_info("pointer to data_out: %p \n",buf->data_out);

	// TODO: Don't know the meaning of this (UO & REB)
	//buf->obj.next = NULL;
	
        const struct sockaddr_in target = *dest;
        //target.sin_addr = dest->sin_addr;
        //target.sin_port = dest->sin_port;

	int MAX_UDP_MSG_SIZE;
	uint32_t max_len_size;
	max_len_size = sizeof(int);
	getsockopt(ni->iface->udp.connect_s, SOL_SOCKET, SO_SNDBUF, (int*)&MAX_UDP_MSG_SIZE, &max_len_size);

	//the buf has data and is not a small message or an ack
	if (buf->rlength > sizeof(buf_t)){//(buf->data_out && (buf->data_out->data_fmt != DATA_FMT_IMMEDIATE) && (buf->data_out->data_fmt != DATA_FMT_NONE)){
		//this means that we have a message that is too large for an immediate send
		//we must do multiple UDP sends to transfer the whole message
		ptl_info("starting large message send \n");
		ptl_info("data ptr: %p length: %i \n",buf->data_out,buf->data_out->udp.length);
		int segments;
		struct msghdr buf_msg_hdr;
		int bytes_remain;
		int cur_ptr;
		struct sockaddr_in msg_dest;
		msg_dest = *dest;;

		segments = ((buf->rlength - sizeof(buf_t)) +  MAX_UDP_MSG_SIZE -1)/MAX_UDP_MSG_SIZE;
		segments++;

		struct iovec iov[segments];

		ptl_info("# of segments: %i \n",segments);
		assert(segments > 0);

		iov[0].iov_base = buf;
		iov[0].iov_len = sizeof(buf_t);
		
		iov[1].iov_base = (void *)buf->data_out->udp.iov_data.iov_base;
		ptl_info("sending iov: %p \n",buf->data_out->udp.iov_data.iov_base);
		
		if (buf->rlength > MAX_UDP_MSG_SIZE - sizeof(buf_t)){
			iov[1].iov_len = MAX_UDP_MSG_SIZE - sizeof(buf_t);
			bytes_remain = buf->rlength - (MAX_UDP_MSG_SIZE - sizeof(buf_t));
			cur_ptr = (MAX_UDP_MSG_SIZE -sizeof(buf_t));
		}
		else{
			iov[1].iov_len = buf->rlength;
			bytes_remain = 0;
		}

		ptl_info("iov lengths: %i %i \n",(int)iov[0].iov_len,(int)iov[1].iov_len);
		ptl_info("start set flags\n");		

		buf_msg_hdr.msg_name = (void *)dest;
		buf_msg_hdr.msg_namelen = sizeof(*dest);
		buf_msg_hdr.msg_iov = &iov;//(struct iovec *)&iov;
		buf_msg_hdr.msg_iovlen = (int)segments;
		buf_msg_hdr.msg_flags = 0;

		ptl_info("set flags\n");

		ptl_info("sending large message to: %s:%i \n",inet_ntoa(((struct sockaddr_in *)buf_msg_hdr.msg_name)->sin_addr),ntohs(((struct sockaddr_in*)buf_msg_hdr.msg_name)->sin_port));
	
		//struct cmsghdr ctrl_msg;
		//ctrl_msg.cmsg_len = 0;
		//ctrl_msg.cmsg_level = 0;
		//ctrl_msg.cmsg_type = 0;
			
		buf_msg_hdr.msg_control = NULL;//(void *)&ctrl_msg;
		//buf_msg_hdr.msg_controllen = sizeof(struct cmsghdr);

		ptl_info("sending UDP message of size: %i, maximum: %i \n",(int)(buf_msg_hdr.msg_iov[0].iov_len+buf_msg_hdr.msg_iov[1].iov_len+buf_msg_hdr.msg_namelen),(int)MAX_UDP_MSG_SIZE);

                err = sendmsg(ni->iface->udp.connect_s, (void *)&buf_msg_hdr, 0);

		while (bytes_remain){
			//do we need to null iov[0] here?
		//	iov[0].iov_base = NULL;
	//		iov[0].iov_len = 0;
			//keep sending multiple UDP messages until all the data is sent
//			iov[1].iov_base = buf->data + cur_ptr;
			if (bytes_remain > MAX_UDP_MSG_SIZE){
				//more UDP messages will follow
//				iov[1].iov_len = MAX_UDP_MSG_SIZE;
				cur_ptr += MAX_UDP_MSG_SIZE;
				bytes_remain -= MAX_UDP_MSG_SIZE;
			}	
			else {
				//last UDP message in sequence
//				iov[1].iov_len = bytes_remain;
				cur_ptr += bytes_remain;
				bytes_remain -= bytes_remain;
			}
			buf_msg_hdr.msg_iov = (struct iovec *)&iov;
			err = sendmsg(ni->iface->udp.connect_s, (void *)&buf_msg_hdr, 0);
		}                      
		
		
	}
	else{
		// for immediate data, just send the actual buffer
		//struct msghdr * buf_msg_hdr;
		//buf_msg_hdr->msg_name = &target;
		err = sendto(ni->iface->udp.connect_s, buf, sizeof(*buf), 0, (struct sockaddr *)dest, sizeof(*dest));//(struct sockaddr*)&target,sizeof(target));//(struct sockaddr*)(buf_msg_hdr->msg_name),sizeof(struct sockaddr));//(struct sockaddr*)&target, sizeof(target));
        }

	if(err == -1) {
		WARN();
                ptl_error("error sending to: %s:%d \n",inet_ntoa(dest->sin_addr),ntohs(dest->sin_port));
		ptl_error("error sending buffer to socket: %i %s \n",ni->iface->udp.connect_s,strerror(errno));
		abort();
		return;
	}
        ptl_info("UDP send completed successfully to: %s:%d from: %s:%d size:%lu %i\n",inet_ntoa(target.sin_addr),ntohs(target.sin_port),inet_ntoa(ni->iface->udp.sin.sin_addr),ntohs(ni->iface->udp.sin.sin_port),sizeof(*buf),(int)buf->rlength);
  		
}

/**
 * @brief receive a buf using a UDP socket.
 *
 * @param[in] ni the network interface.
 */
buf_t *udp_receive(ni_t *ni){

	int err;
	struct sockaddr_in temp_sin;
	socklen_t lensin = sizeof(temp_sin);	

	buf_t * thebuf = (buf_t*)calloc(1, sizeof(buf_t));

	int flags;
	flags = MSG_PEEK;
	//REG: we need to perform a quick message peek here to determine if it is a short or long message
	// this can be tweaked performance wise to only peek on the beginning of the buf for the length
	err = recvfrom(ni->iface->udp.connect_s, thebuf, sizeof(*thebuf), flags, (struct sockaddr*)&temp_sin, &lensin);

	if (err == -1) {
	    if (errno != EAGAIN){
		free(thebuf);
		WARN();
		ptl_warn("error when peeking at message \n");
		return NULL;
	    }
	    else{
		//Nothing to fetch
		free(thebuf);	
		return NULL;
	    }
	}

	//we are going to be handling multiple messages, implemented through a recvmsg call
	if (thebuf->rlength > sizeof(*thebuf)){
		ptl_info("peek indicates large message of size: %i\n",(int)thebuf->rlength);
		
	    	 //first I/O vector will be the traditional portals buf
		 //second will be the data for the buf, so we need to set the data pointer appropriately once
	 	 //the data has arrived
		struct msghdr buf_msg_hdr;
		char * buf_data;
		int buf_data_len;
		struct iovec iov[2];

		//this is the size of the largest UDP message that can be sent
		int MAX_UDP_MSG_SIZE;
        	uint32_t max_len_size;
        	max_len_size = sizeof(int);
        	getsockopt(ni->iface->udp.connect_s, SOL_SOCKET, SO_RCVBUF, (int*)&MAX_UDP_MSG_SIZE, &max_len_size);
		buf_data = calloc(1, MAX_UDP_MSG_SIZE);
		
		iov[0].iov_base = thebuf;
		iov[0].iov_len = sizeof(buf_t);
		iov[1].iov_base = buf_data;
		iov[1].iov_len = sizeof(buf_data);
	
		buf_msg_hdr.msg_name = &temp_sin;
		buf_msg_hdr.msg_namelen = sizeof(temp_sin);
		buf_msg_hdr.msg_iov = &iov;
		buf_msg_hdr.msg_iovlen = 2;
		
		err = recvmsg(ni->iface->udp.connect_s, &buf_msg_hdr, 0);
		if(err == -1) {
                        free(thebuf);
                        WARN();
                        ptl_warn("error receiving main buffer from socket: %d %s\n",ni->iface->udp.connect_s,strerror(errno));
                        abort();
			return NULL;

                }
	

	
		ptl_info("received large message of size: %i \n",err);
		//process received IO vectors
		
		thebuf = (buf_t *)buf_msg_hdr.msg_iov[0].iov_base;
		buf_data = (char *)buf_msg_hdr.msg_iov[1].iov_base;
		buf_data_len = buf_msg_hdr.msg_iovlen;

		thebuf->data_in = buf_data;
		thebuf->data_in->udp.length = buf_msg_hdr.msg_iov[1].iov_len;
		temp_sin = *(struct sockaddr_in*)buf_msg_hdr.msg_name;
		lensin = buf_msg_hdr.msg_namelen;
				
	 
		if (thebuf->rlength > 65507){
			//this message is large enough to span multiple UDP messages, so we need to fetch them all
			//first message will be the portals buf, and data upto 64K
			//subsequent messages need to be added to the received data buffer as extra data
			//there is no finalization footer etc.
			//THIS ASSUMES UDP IS RELIABLE AND IN-ORDER, which it is not unless a reliability layer is present.
			//this also assumes that there is not data incoming from multiple sources on this socket/port		
			ptl_warn("Message size exceeds that of a single datagram, this operational mode is not recommended \n");		
			abort();	
		}	
	}
	else {
	
        err = recvfrom(ni->iface->udp.connect_s, thebuf, sizeof(*thebuf), 0, (struct sockaddr*)&temp_sin, &lensin);
		if(err == -1) {
            		if (errno != EAGAIN){
            		    	free(thebuf);
	    			WARN();
				ptl_warn("error receiving main buffer from socket: %d %s\n",ni->iface->udp.connect_s,strerror(errno));
				return NULL;
       	   		}
		
       	    		else {
				//Nothing to fetch
       	      			  free(thebuf);
	     		  	return NULL;
       	  		}
		}
	}
	
	if(&thebuf->transfer.udp.conn_msg != NULL){
		ptl_info("process received message type \n");
		struct udp_conn_msg *msg = &thebuf->transfer.udp.conn_msg;
	
		if (msg->msg_type == UDP_CONN_MSG_REQ){
		   ptl_info("received a UDP connection request \n");
		   thebuf->type = BUF_UDP_CONN_REQ;
		}
		else if (msg->msg_type == UDP_CONN_MSG_REP){
		   ptl_info("recieved a UDP connection reply \n");
		   thebuf->type = BUF_UDP_CONN_REP;
		}
		else{
	   	   ptl_info("received a UDP data packet \n");
		   thebuf->type = BUF_UDP_RECEIVE;
		}

		}
	
	ptl_info("received data from %s:%i type:%i data size: %lu message size:%u %i \n",inet_ntoa(temp_sin.sin_addr),ntohs(temp_sin.sin_port),thebuf->type,sizeof(*(thebuf->data)),thebuf->length,err);
	 	
        thebuf->udp.src_addr = temp_sin;
        return (buf_t *)thebuf;
}

/* change the state of conn; we are now connected (UO & REB) */
void PtlSetMap_udp(ni_t *ni, ptl_size_t map_size, const ptl_process_t *mapping)
{
	int i;

        ptl_info("Creating a connection for PTlSetMap\n");
	for (i = 0; i < map_size; i++) {
		conn_t *conn;
                
		conn = get_conn(ni, ni->id);
		if (!conn) {
			/* It's hard to recover from here. */
			WARN();
			abort();
			return;
		}

#if WITH_TRANSPORT_UDP
			conn->transport = transport_udp;
			//ptl_info("connection: %s:%i rank: %i\n",inet_ntoa(conn->sin.sin_addr),htons(conn->sin.sin_port),id.rank);
#else
			/* This should never happen */
 			ptl_error("Creating UDP Map for Non-UDP Transport \n");
#endif
			//We are not connected until we've exchanged messages
			//conn->state = CONN_STATE_CONNECTED;

			conn_put(conn);			/* from get_conn */
                	
	}
	ptl_info("Done creating connection for PtlSetMap\n");
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

//	assert(conn->transport.type == CONN_TYPE_UDP);

//	if (ni->shutting_down)
//		return PTL_FAIL;

	//conn_get(conn);

	//assert(conn->state == CONN_STATE_DISCONNECTED);

	/* Create a buffer for sending the connection request message */
	buf_t * conn_buf = (buf_t *)calloc(1, sizeof(buf_t));
        conn_buf->type = BUF_UDP_CONN_REQ;

        /* Send the connect request. */
	struct udp_conn_msg msg;
	msg.msg_type = cpu_to_le16(UDP_CONN_MSG_REQ);
	msg.port = ntohs(ni->udp.src_port);//REG: cpu_to_le16(ni->udp.src_port);
	msg.req.options = ni->options;
	msg.req.src_id = ni->id;
	msg.req_cookie = (uintptr_t)conn;


	struct req_hdr *hdr;
	hdr = (struct req_hdr*)&conn_buf->internal_data;
	conn_buf->data = (void *)&conn_buf->internal_data;//hdr;

	((struct hdr_common*)hdr)->version = PTL_HDR_VER_1;
	
	hdr->src_nid = cpu_to_le32(ni->id.phys.nid);
        hdr->src_pid = cpu_to_le32(ni->id.phys.pid);
	
	conn_buf->transfer.udp.conn_msg = msg;
        conn_buf->length = (sizeof(buf_t));
	conn_buf->conn = conn;       
	conn_buf->udp.dest_addr = &conn->sin;

/*	struct sockaddr_in * temp_sin;
	temp_sin->sin_addr.s_addr = nid_to_addr(ni->id.phys.nid);

	ptl_info("nid: %s pid: %i \n",inet_ntoa(temp_sin->sin_addr),(ni->id.phys.pid));
	ptl_info("addr: %s : %i \n",inet_ntoa(conn->sin.sin_addr),ntohs(conn_buf->udp.dest_addr->sin_port));

	if (conn_buf->udp.dest_addr->sin_port == ntohs(ni->id.phys.pid)) {
		if (temp_sin->sin_addr.s_addr == conn->sin.sin_addr.s_addr) {
			//we are sending to ourselves, so we don't need a connection
			ptl_info("sending to self! \n");
			//REG: TODO to make this work, must allow for sending to self, which requires different ports
			//REG: do we want to change the ptl_map though to allow this to happen?
			ni->iface->udp.connect_s = socket(PF_LOCAL, SOCK_DGRAM, 0);
			int ret;
			ret = bind(ni->iface->udp.connect_s, (struct sockaddr *)&conn->sin, sizeof(conn->sin));
			//conn->udp.loop_to_self = 1;
			conn->state = CONN_STATE_CONNECTED;
			//return PTL_OK;
		}
	}
*/	
	//conn->udp.loop_to_self = 0;
 
	ptl_info("to send msg size: %lu in UDP message size: %lu\n",sizeof(msg),sizeof(buf_t));
        
	/* Send the request to the listening socket on the remote node. */	
        /* This does not send just the msg, but a buf to maintain compatibility with the progression thread */
	ret = sendto(ni->iface->udp.connect_s, conn_buf, conn_buf->length, 0,
				(struct sockaddr*)&conn->sin, sizeof(conn->sin));
	if (ret == -1) {
		WARN();
		return PTL_FAIL;
	}
        
        ptl_info("succesfully send connection request to listener: %s:%d from: %d size:%i\n",inet_ntoa(conn_buf->udp.dest_addr->sin_addr),htons(conn_buf->udp.dest_addr->sin_port),htons(ni->udp.src_port),ret);
        //free(conn_buf); 
	//conn_put(conn);
	return PTL_OK;
}

struct transport transport_udp = {
	.type = CONN_TYPE_UDP,
	.buf_alloc = buf_alloc,
	.init_connect = init_connect_udp,
	.send_message = send_message_udp,
	.set_send_flags = udp_set_send_flags,
	.init_prepare_transfer = init_prepare_transfer_udp,
	.post_tgt_dma = do_udp_transfer,
	.tgt_data_out = udp_tgt_data_out,
};

struct transport_ops transport_remote_udp = {
	.init_iface = init_iface_udp,
	.NIInit = PtlNIInit_UDP,
};
