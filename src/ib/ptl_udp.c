/**
 * @file ptl_udp.c
 *
 * @brief UDP transport operations used by target.
 */

#include "ptl_loc.h"
#include "ptl_rudp.h"

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
    buf_get(buf);

    //set the buffer type to be received at the other end
    buf->type = BUF_UDP_RECEIVE;

    // increment the sequence number associated with the
    // send-side of this connection
    //atomic_inc(&buf->conn->udp.send_seq);

#if WITH_RUDP
    ptl_info("&&&&&&&&&& Reliable UDP send &&&&&&&&&\n");
#endif

    udp_send(buf->obj.obj_ni, buf, &buf->dest.udp.dest_addr);

    buf_put(buf);

    return PTL_OK;
}

static void udp_set_send_flags(buf_t *buf, int can_signal)
{
    /* The data is always in the buffer. */
    buf->event_mask |= XX_INLINE;
}

static void append_init_data_udp_iovec(data_t *data, md_t *md, int iov_start,
                                       int num_iov, ptl_size_t length,
                                       buf_t *buf)
{
    data->data_fmt = DATA_FMT_UDP;

    data->udp.target_done = 0;
    data->udp.init_done = 0;

    buf->transfer.udp.transfer_state_expected = 0;  /* always the initiator here */
    buf->transfer.udp.udp = &data->udp;

    buf->transfer.udp.num_iovecs = num_iov;
    ptl_info("appending data for %i iovecs \n", num_iov);
    int i;

    buf->transfer.udp.iovecs = calloc(1, sizeof(ptl_iovec_t) * num_iov);

    for (i = 0; i < num_iov; i++) {
        buf->transfer.udp.iovecs[i].iov_base = md->udp_list[i].iov_base;
        buf->transfer.udp.iovecs[i].iov_len = md->udp_list[i].iov_len;
        //ptl_info("item is: %x length: %i \n",md->udp_list[i].iov_base,md->udp_list[i].iov_len);
    }

    buf->transfer.udp.my_iovec.iov_base = calloc(1, md->length);
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

    buf->transfer.udp.transfer_state_expected = 0;  /* always the initiator here */
    buf->transfer.udp.udp = &data->udp;
    buf->transfer.udp.data = (unsigned char *)&data;

    /* Describes local memory */
    buf->transfer.udp.my_iovec.iov_base = addr_to_ppe(addr, mr);
    buf->transfer.udp.my_iovec.iov_len = length;

    buf->transfer.udp.num_iovecs = 1;
    buf->transfer.udp.iovecs = &buf->transfer.udp.my_iovec;
    buf->transfer.udp.offset = 0;

    buf->transfer.udp.length_left = length;

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

static int init_prepare_transfer_udp(md_t *md, data_dir_t dir,
                                     ptl_size_t offset, ptl_size_t length,
                                     buf_t *buf)
{
    int err = PTL_OK;
    req_hdr_t *hdr = (req_hdr_t *) buf->data;
    data_t *data = (data_t *)(buf->data + sizeof(req_hdr_t));
    int num_sge;

    ptl_size_t iov_start = 0;
    ptl_size_t iov_offset = 0;

    if (length <= get_param(PTL_MAX_INLINE_DATA)) {
        mr_t **mr_list;

        if (md->mr_list) {
            mr_list = md->mr_list;
        } else {
            void *addr;
            mr_t *mr;
            ni_t *ni = obj_to_ni(md);

            addr = md->start + offset;
            if (mr_lookup_app(ni, addr, length, &mr))
                abort();
            mr_list = &mr;
        }

        ptl_info("small transfer inlining data \n");
        if (append_immediate_data
            (md->start, mr_list, md->num_iov, dir, offset, length, buf))
            abort();
    } else {
        if (md->options & PTL_IOVEC) {
            ptl_warn("using native iovecs \n");
            ptl_iovec_t *iovecs = md->start;

            // Find the index and offset of the first IOV as well as the
            //  total number of IOVs to transfer. 
            num_sge =
                iov_count_elem(iovecs, md->num_iov, offset, length,
                               &iov_start, &iov_offset);
            if (num_sge < 0) {
                WARN();
                return PTL_FAIL;
            }

            append_init_data_udp_iovec(data, md, iov_start, num_sge, length,
                                       buf);

            hdr->roffset = 0;

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
                ptl_info("addr to send is: %p, buf addr is: %p \n", addr,
                         buf->transfer.udp.my_iovec.iov_base);
            } else {
                abort();
            }
        }
    }

    if (!err)
        assert(buf->length <= BUF_DATA_SIZE);

    return err;
}

/**
 * @brief Perform data movement for put/get operations 
 *
 * @param[in] buf the buffer to transfer the data to/from
 *
 * @return status
*/
static int do_udp_transfer(buf_t *buf)
{
    struct udp *udp = buf->transfer.udp.udp;
    ptl_size_t *resid =
        buf->rdma_dir == DATA_DIR_IN ? &buf->put_resid : &buf->get_resid;
    ptl_size_t to_copy;
    int err;
    int is_iovec;

    if (udp->state != 2)
        return PTL_OK;

    if (udp->init_done) {
        assert(udp->target_done);
        return PTL_OK;
    }

    udp->state = 3;

    if (*resid) {
        mr_t **mr_list;

        if (buf->rdma_dir == DATA_DIR_IN) {
            //Put operation copy
            to_copy = udp->length;
            ptl_info("copy into local mem, length: %i from: %p \n",
                     (int)to_copy, buf->transfer.udp.data);
            if (to_copy > *resid)
                to_copy = *resid;

            mr_list =
                (buf->me->mr_list) ? buf->me->mr_list : &buf->me->mr_start;
            err =
                iov_copy_in(buf->transfer.udp.data, buf->transfer.udp.iovecs,
                            mr_list, buf->transfer.udp.num_iovecs,
                            buf->transfer.udp.offset, to_copy);
        } else {
            //Get operation response
            is_iovec = !!(buf->me->options & PTL_IOVEC);
            buf->transfer.udp.is_iovec = is_iovec;
            ptl_info("iovec response? %i \n", is_iovec);

            if (is_iovec) {
                buf->transfer.udp.num_iovecs = buf->me->num_iov;
            }
            to_copy = buf->rlength;
            if (to_copy > *resid)
                to_copy = *resid;

            err = PTL_OK;
            buf->send_buf = buf;

            if (buf->transfer.udp.is_iovec == 0) {
                //forget copying, just use the iovec  
                buf->transfer.udp.my_iovec.iov_base =
                    addr_to_ppe(buf->transfer.udp.my_iovec.iov_base,
                                buf->me->mr_start);
                buf->send_buf->transfer.udp.data =
                    buf->transfer.udp.my_iovec.iov_base;
            }

            if (is_iovec == 1) {
                mr_list =
                    (buf->me->mr_list) ? buf->me->mr_list : &buf->
                    me->mr_start;
                err =
                    iov_copy_out(buf->send_buf->transfer.udp.
                                 my_iovec.iov_base, buf->me->start, mr_list,
                                 buf->me->num_iov, buf->transfer.udp.offset,
                                 to_copy);
                buf->send_buf->transfer.udp.num_iovecs = buf->me->num_iov;
            }
            //We need to setup the reply buffer for this data
            buf->send_buf->rlength = to_copy;
            buf->send_buf->transfer.udp.udp->length = to_copy;
            buf->send_buf->mlength = to_copy;
            udp->length = to_copy;
        }

        /* That should never happen since all lengths were properly
         * computed before entering. */
        assert(err == PTL_OK);

    } else {
        ptl_info("udp transfer dropping \n");
        /* Dropped case. Nothing to transfer, but the buffer must
         * still be returned. */
        to_copy = 0;
        err = PTL_OK;
    }

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
    if (data->data_fmt != DATA_FMT_UDP) {
        assert(0);
        WARN();
        return STATE_TGT_ERROR;
    }

    ptl_info("udp_tgt_data_out sets  %p direction: %i\n", buf, buf->rdma_dir);
    buf->transfer.udp.transfer_state_expected = 2;  /* always the target here */
    buf->transfer.udp.udp = &data->udp;

    if ((buf->rdma_dir == DATA_DIR_IN && buf->put_resid) ||
        (buf->rdma_dir == DATA_DIR_OUT && buf->get_resid)) {
        if (buf->me->options & PTL_IOVEC) {
            buf->transfer.udp.num_iovecs = buf->me->num_iov;
            buf->me->start = addr_to_ppe(buf->me->start, buf->me->mr_start);
            buf->transfer.udp.iovecs = buf->me->start;
            ptl_info("num iovecs: %i \n", (int)buf->transfer.udp.num_iovecs);
        } else {
            buf->transfer.udp.num_iovecs = 1;
            buf->transfer.udp.iovecs = &buf->transfer.udp.my_iovec;

            buf->transfer.udp.my_iovec.iov_base = buf->me->start;
            buf->transfer.udp.my_iovec.iov_len = buf->me->length;
        }
    }

    buf->transfer.udp.offset = buf->moffset;
    buf->transfer.udp.length_left = buf->get_resid; // the residual length left
    buf->transfer.udp.data_length = buf->rlength;

    return STATE_TGT_UDP;
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

    const struct sockaddr_in target = *dest;

    int MAX_UDP_MSG_SIZE = 1488;
    uint32_t max_len_size;
    max_len_size = sizeof(int);
    getsockopt(ni->iface->udp.connect_s, SOL_SOCKET, SO_SNDBUF,
               (int *)&MAX_UDP_MSG_SIZE, &max_len_size);
    //65507 is the max IPv4 UDP message size (65536 - 8 byte UDP header - 20 byte IP header)
    //Send buffers can sometimes exceed this size, so we need to cap the max packet size
    if (MAX_UDP_MSG_SIZE > 65507)
        MAX_UDP_MSG_SIZE = 65507;

    ptl_info("max udp message size is: %i \n", MAX_UDP_MSG_SIZE);

    //check for send to self, use local memory for transfer
    if (((dest->sin_port == ni->id.phys.pid) &&
         (dest->sin_addr.s_addr == nid_to_addr(ni->id.phys.nid)))) {
        ptl_info("sending to self! \n");
        if (buf->rlength <= sizeof(buf_t)) {
            if (buf->transfer.udp.conn_msg.msg_type !=
                le16_to_cpu(UDP_CONN_MSG_REP)) {
                //the only multiple outstanding self sends that are valid are
                //connection request replies and ACKS, otherwise, wait until the previous
                //send has been received and processed
                ack_hdr_t *hdr = (ack_hdr_t *) buf->data;
                if (!
                    (hdr->h1.operation == OP_ACK ||
                     hdr->h1.operation == OP_CT_ACK ||
                     hdr->h1.operation == OP_OC_ACK)) {
                    while (atomic_read(&ni->udp.self_recv) > 0) {
                        ptl_info("queuing for self send \n");
                        sleep(1);
                    }
                }
            }
            buf->udp.src_addr = *dest;
            ni->udp.self_recv_addr = buf;
            ni->udp.self_recv_len = sizeof(buf_t);
            ptl_info("self ref addr is: %p \n", ni->udp.self_recv_addr);
            atomic_inc(&ni->udp.self_recv);
            return;
        } else {
            ptl_warn("large message self sends not yet supported \n");
            abort();
        }

    }

    struct md *send_md = NULL;

    if ((buf->put_md != NULL) || buf->get_md != NULL) {
        if (buf->put_md != NULL) {
            if (buf->put_md->options) {
                if (!!(buf->put_md->options & PTL_IOVEC)) {
                    ptl_warn("IO vec put transfer %i \n", (int)buf->rlength);
                    send_md = buf->put_md;
                }
            }
        } else {
            if (buf->get_md->options) {
                if (!!(buf->get_md->options & PTL_IOVEC)) {
                    ptl_info("IO vec get transfer %i \n", (int)buf->rlength);
                    send_md = buf->get_md;
                }
            }
        }

    }
    //the buf has data and is not a small message or an ack
    //TODO: Adjust this to the actual data size available in the buf_t immediate data
    if (buf->rlength > sizeof(buf_t)) {
        //this means that we have a message that is too large for an immediate send
        //we must send it as a iovec upto the maximum UDP message size (64KB)

        ptl_info("starting large message send \n");
        if (send_md == NULL) {
            ptl_info("data ptr`: %p length: %i \n",
                     buf->transfer.udp.my_iovec.iov_base,
                     (int)buf->transfer.udp.my_iovec.iov_len);
            buf->transfer.udp.is_iovec = 0;
        } else {
            if (buf->transfer.udp.is_iovec == 1) {
                ptl_info
                    ("Flagged IO vec, data prt: %p data: %llu number of vecs: %i \n",
                     buf->transfer.udp.data,
                     *(long long unsigned int *)buf->transfer.udp.data,
                     (int)buf->transfer.udp.num_iovecs);
            } else if (!!(send_md->options & PTL_IOVEC)) {
                ptl_info("IO vec, data prt: %p number of vecs: %i \n",
                         buf->transfer.udp.iovecs,
                         (int)buf->transfer.udp.num_iovecs);
                //Now we just copy the iovecs into a sinlge buffer for sending as one large message
                int i;
                int cur_pntr = 0;
                for (i = 0; i < buf->transfer.udp.num_iovecs; i++) {
                    memcpy(buf->transfer.udp.my_iovec.iov_base + cur_pntr,
                           buf->transfer.udp.iovecs[i].iov_base,
                           buf->transfer.udp.iovecs[i].iov_len);
                    cur_pntr += buf->transfer.udp.iovecs[i].iov_len;
                }
                buf->transfer.udp.is_iovec = 0;
            } else {
                buf->transfer.udp.is_iovec = 0;
            }
        }

        int segments = 0;
        struct msghdr buf_msg_hdr;
        int bytes_remain;
        int cur_ptr;
        struct sockaddr_in msg_dest;
        int current_iovec = 0;
        msg_dest = *dest;
        req_hdr_t *hdr = (req_hdr_t *) buf->internal_data;

        segments = (buf->rlength / (MAX_UDP_MSG_SIZE - sizeof(buf_t))) + 1;

        hdr->fragment_seq = 0;

        int iovec_elements;

        if (buf->transfer.udp.is_iovec == 0) {
            iovec_elements = 2;
            buf->transfer.udp.num_iovecs = iovec_elements;
            buf->transfer.udp.is_iovec = 0;
        } else {
            iovec_elements = buf->transfer.udp.num_iovecs;
            if (iovec_elements == buf->rlength) {
                iovec_elements = 2;
                buf->transfer.udp.num_iovecs = 2;
                buf->transfer.udp.is_iovec = 0;
            } else {
                iovec_elements = buf->transfer.udp.num_iovecs++;
            }
            current_iovec = 0;

        }
        //for a non-iovec based send
        //we only have two iovec elements, the buf_t and the data
        //for an io-vec based send we have 1 + the number of
        //elements in the sending io-vec
        struct iovec iov[iovec_elements + 1];

        ptl_info("# of segments: %i \n", segments);
        assert(segments > 0);

        buf->udp.src_addr = target;
        ptl_info("set buf target to: %s:%d \n", inet_ntoa(target.sin_addr),
                 ntohs(target.sin_port));


        iov[0].iov_base = buf;
        iov[0].iov_len = sizeof(buf_t);

        if (buf->transfer.udp.is_iovec == 0) {
            buf->transfer.udp.is_iovec = 0;
            iov[1].iov_base = (void *)buf->transfer.udp.my_iovec.iov_base;
            ptl_info("sending iov: %p \n",
                     buf->transfer.udp.my_iovec.iov_base);
            if (buf->rlength > MAX_UDP_MSG_SIZE - sizeof(buf_t)) {
                iov[1].iov_len = MAX_UDP_MSG_SIZE - sizeof(buf_t);
                bytes_remain =
                    buf->rlength - (MAX_UDP_MSG_SIZE - sizeof(buf_t));
                cur_ptr = (MAX_UDP_MSG_SIZE - sizeof(buf_t));
            } else {
                iov[1].iov_len = buf->rlength;
                bytes_remain = 0;
            }
            //this is always equal to 2 for non-iovec sends
            current_iovec = iovec_elements;
        } else {
            int i = 1;
            bytes_remain = buf->rlength;
            int current_size = 0;

            int total_size = 0;
            for (i = 0; i < buf->transfer.udp.num_iovecs; i++) {
                total_size += buf->transfer.udp.iovecs[i].iov_len;
            }

            bytes_remain = total_size;
            ptl_info("total size of iovec is: %i \n", total_size);
            i = 1;

            if (total_size < (MAX_UDP_MSG_SIZE - sizeof(buf_t))) {
                //just send all of the iovecs in the message data iovec
                for (i = 1; i <= iovec_elements; i++) {
                    iov[i].iov_base =
                        buf->transfer.udp.iovecs[i - 1].iov_base;
                    iov[i].iov_len = buf->transfer.udp.iovecs[i - 1].iov_len;
                    current_iovec++;
                }
                current_iovec++;
            } else {
                while (bytes_remain && current_iovec <= iovec_elements) {

                    if ((current_size +
                         buf->transfer.udp.iovecs[current_iovec].iov_len) <
                        (MAX_UDP_MSG_SIZE - sizeof(buf_t))) {
                        iov[i].iov_base =
                            buf->transfer.udp.iovecs[current_iovec].iov_base;
                        ptl_info
                            ("native IOvec send, sending iovec #%i of %i \n",
                             current_iovec + 1,
                             (int)buf->transfer.udp.num_iovecs);
                        iov[i].iov_len =
                            buf->transfer.udp.iovecs[current_iovec].iov_len;
                        bytes_remain = bytes_remain - iov[i].iov_len;
                        current_size = current_size + iov[i].iov_len;
                        current_iovec++;
                        i++;
                    } else {
                        //if there's any space left, send part of the next iovec.
                        if (current_size < (MAX_UDP_MSG_SIZE - sizeof(buf_t))) {
                            int space_left =
                                (MAX_UDP_MSG_SIZE - sizeof(buf_t)) -
                                current_size;
                            iov[i].iov_base =
                                buf->transfer.udp.
                                iovecs[current_iovec].iov_base;
                            iov[i].iov_len = space_left;
                            current_size += iov[i].iov_len;
                            buf->transfer.udp.
                                iovecs[current_iovec].iov_base += space_left;
                            buf->transfer.udp.iovecs[current_iovec].iov_len -=
                                space_left;
                            i++;
                        }
                        ptl_info
                            ("current message data payload full, send additional segments \n");
                        //send iovec datagram and then loop back to prepare another
                        buf_msg_hdr.msg_name = (void *)dest;
                        buf_msg_hdr.msg_namelen = sizeof(*dest);
                        buf_msg_hdr.msg_iov = (struct iovec *)&iov;
                        buf_msg_hdr.msg_iovlen = i;
                        buf->transfer.udp.msg_num_iovecs = i;
                        ptl_info
                            ("message header iov num set to %i , size: %i, length: %i\n",
                             (int)buf_msg_hdr.msg_iovlen, (int)buf->rlength,
                             (int)current_size);
                        buf_msg_hdr.msg_flags = 0;

                        ptl_info("sending large message to: %s:%i \n",
                                 inet_ntoa(((struct sockaddr_in *)
                                            buf_msg_hdr.msg_name)->sin_addr),
                                 ntohs(((struct sockaddr_in *)
                                        buf_msg_hdr.msg_name)->sin_port));

                        buf_msg_hdr.msg_control = NULL;
                        buf_msg_hdr.msg_controllen = 0;
                        ptl_info("send IOvec, segment #%i of #%i \n",
                                 hdr->fragment_seq + 1, segments);
                        err =
                            ptl_sendmsg(ni->iface->udp.connect_s,
                                        (void *)&buf_msg_hdr, 0, ni);
                        hdr->fragment_seq++;
                        current_size = 0;
                        i = 1;
                    }

                }
                //current_iovec is used in the next section as the length of the to be sent message, so set it to i.
                bytes_remain = 0;
                current_iovec = i;

            }
        }


        ptl_info("iov lengths: %u %u \n", (int)iov[0].iov_len,
                 (int)iov[1].iov_len);

        buf_msg_hdr.msg_name = (void *)dest;
        buf_msg_hdr.msg_namelen = sizeof(*dest);
        buf_msg_hdr.msg_iov = (struct iovec *)&iov;
        buf_msg_hdr.msg_iovlen = current_iovec;
        buf->transfer.udp.msg_num_iovecs = current_iovec;
        ptl_info("message header iov num set to %i \n",
                 (int)buf_msg_hdr.msg_iovlen);
        buf_msg_hdr.msg_flags = 0;

        ptl_info("sending large message to: %s:%i \n",
                 inet_ntoa(((struct sockaddr_in *)buf_msg_hdr.
                            msg_name)->sin_addr),
                 ntohs(((struct sockaddr_in *)buf_msg_hdr.
                        msg_name)->sin_port));

        buf_msg_hdr.msg_control = NULL;
        buf_msg_hdr.msg_controllen = 0;

        ptl_info("is message iovec? %i \n", buf->transfer.udp.is_iovec);
        ptl_info("sending UDP message of size: %i, maximum: %i \n",
                 (int)(buf_msg_hdr.msg_iov[0].iov_len +
                       buf_msg_hdr.msg_iov[1].iov_len),
                 (int)MAX_UDP_MSG_SIZE);
        ptl_info("send segment #%i of #%i \n", hdr->fragment_seq + 1,
                 segments);
        err =
            ptl_sendmsg(ni->iface->udp.connect_s, (void *)&buf_msg_hdr, 0,
                        ni);
        if (err == -1) {
            ptl_error
                ("error while sending multi segment message: %s\n remaining data: %i \n",
                 strerror(errno), bytes_remain);
        }
        //this continues sending other datagrams for the non-iovec case
        while (bytes_remain) {

            hdr->fragment_seq++;
            //keep sending multiple UDP segments until all the data is sent
            if (bytes_remain >= (MAX_UDP_MSG_SIZE - sizeof(buf_t))) {
                //more UDP segments to this message will follow
                cur_ptr = MAX_UDP_MSG_SIZE - sizeof(buf_t);
                bytes_remain -= (MAX_UDP_MSG_SIZE - sizeof(buf_t));
            } else {
                //last UDP segment in sequence
                cur_ptr = bytes_remain;
                bytes_remain -= bytes_remain;
            }
            iov[1].iov_base += cur_ptr;
            iov[1].iov_len = cur_ptr;
            buf_msg_hdr.msg_iov = (struct iovec *)&iov;
            ptl_info("send segment #%i of #%i \n", hdr->fragment_seq + 1,
                     segments);
#ifdef __APPLE__
            //We can overrun the send buffer without a wait here
            //due to Mac's having very small network buffers
            usleep(50);
#endif
            err =
                ptl_sendmsg(ni->iface->udp.connect_s, (void *)&buf_msg_hdr, 0,
                            ni);
            if (err == -1) {
                ptl_error
                    ("error while sending multi segment message: %s\n remaining data: %i \n",
                     strerror(errno), bytes_remain);
            }
        }


    } else if (buf->rlength <= sizeof(buf_t)) { // for immediate data, just send the actual buffer
        err =
            ptl_sendto(ni->iface->udp.connect_s, buf, sizeof(*buf), 0,
                       (struct sockaddr *)dest, sizeof(*dest), ni);
    }

    if (err == -1) {
        WARN();
        ptl_error("error sending to: %s:%d \n", inet_ntoa(dest->sin_addr),
                  ntohs(dest->sin_port));
        ptl_error("error sending buffer to socket: %i %s \n",
                  ni->iface->udp.connect_s, strerror(errno));
        abort();
        return;
    }
    ptl_info
        ("UDP send completed successfully to: %s:%d from: %d size:%lu %i %i\n",
         inet_ntoa(target.sin_addr), ntohs(target.sin_port),
         ntohs(ni->iface->udp.sin.sin_port), sizeof(*buf), (int)buf->rlength,
         err);

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

    buf_t *thebuf = (buf_t *)calloc(1, sizeof(buf_t));

    if (atomic_read(&ni->udp.self_recv) >= 1) {
        ptl_info("got a message from self %p \n", ni->udp.self_recv_addr);
        free(thebuf);
        thebuf = (buf_t *)ni->udp.self_recv_addr;
        return thebuf;
    }


    int flags;
    flags = MSG_PEEK;
    //REG: we need to perform a quick message peek here to determine if it is a short or long message
    // this can be tweaked performance wise to only peek on the beginning of the buf for the length
    // this peak is also used to determine if it is a multi-segment message
    err =
        ptl_recvfrom(ni->iface->udp.connect_s, thebuf, sizeof(*thebuf), flags,
                     (struct sockaddr *)&temp_sin, &lensin, ni);

    if (err == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            // OK, nothing ready to fetch
            free(thebuf);
            return NULL;
        } else {
            // Error, recvfrom returned unexpected error
            free(thebuf);
            WARN();
            ptl_warn("error when peeking at message: %s \n", strerror(errno));
            return NULL;
        }
    }

    req_hdr_t *hdr;

    //first check to see if this is meant for this ni
    if (thebuf->internal_data != NULL) {
        hdr = (req_hdr_t *) thebuf->internal_data;
    } else {
        hdr = (req_hdr_t *) & thebuf->data;
    }

    if (((hdr->h1.physical == 0) && (!!(ni->options & PTL_NI_PHYSICAL))) ||
        ((hdr->h1.physical == 1) && (!!(ni->options & PTL_NI_LOGICAL)))) {
        //this datagram is not meant for us
        ptl_info("packet not meant for this logical NI, dropping \n");
        free(thebuf);
        //this time interval is just to back off, it is completely arbitrary
        //although 20us is a reasonable approximation of the time to 
        //fetch a recv through the kernel UDP networking stack
        usleep(20);
        return NULL;
    }
    //we are going to be handling multiple messages, implemented through a recvmsg call
    if (thebuf->rlength > sizeof(buf_t)) {
        ptl_info("peek indicates large message of size: %i\n",
                 (int)thebuf->rlength);

        //first I/O vector will be the traditional portals buf
        //second will be the data for the buf, so we need to set the data pointer appropriately once
        //the data has arrived
        struct msghdr buf_msg_hdr;
        char *buf_data;
        int buf_data_len;
        struct iovec iov[thebuf->transfer.udp.num_iovecs + 1];

        //this is the size of the largest UDP message that can be sent
        //without fragmenting into multiple datagrams
        int MAX_UDP_MSG_SIZE;
        uint32_t max_len_size;
        max_len_size = sizeof(int);
        int current_message_size;

        getsockopt(ni->iface->udp.connect_s, SOL_SOCKET, SO_RCVBUF,
                   (int *)&MAX_UDP_MSG_SIZE, &max_len_size);
        //65507 is the max IPv4 UDP message size (65536 - 8 byte UDP header - 20 byte IP header)
        if (MAX_UDP_MSG_SIZE > 65507)
            MAX_UDP_MSG_SIZE = 65507;
        buf_data = calloc(1, (size_t) MAX_UDP_MSG_SIZE);

        iov[0].iov_base = thebuf;
        iov[0].iov_len = sizeof(buf_t);
        iov[1].iov_base = buf_data;
        //just combine all data into one big iov
        iov[1].iov_len = thebuf->rlength;

        buf_msg_hdr.msg_name = &temp_sin;
        buf_msg_hdr.msg_namelen = sizeof(temp_sin);
        buf_msg_hdr.msg_iov = (struct iovec *)&iov;

        //we only ever have two iovecs, the header and the data
        buf_msg_hdr.msg_iovlen = 2;

        err = recvmsg(ni->iface->udp.connect_s, &buf_msg_hdr, 0);
        if (err == -1) {
            free(thebuf);
            free(buf_data);
            WARN();
            ptl_warn("error receiving main buffer from socket: %d %s\n",
                     ni->iface->udp.connect_s, strerror(errno));
            abort();
            return NULL;

        }

        current_message_size = err - sizeof(buf_t);
        ptl_info("received message of size: %i %i %i\n", err,
                 (int)iov[0].iov_len, (int)iov[1].iov_len);

        //process received IO vectors
        thebuf = (buf_t *)buf_msg_hdr.msg_iov[0].iov_base;
        thebuf->transfer.udp.num_iovecs = buf_msg_hdr.msg_iovlen;

        struct md *recv_md = NULL;

        //check if this is an iovec
        //if it is, copy all of the iovecs over
        if (thebuf->transfer.udp.num_iovecs > 2) {
            int cur_iov_copy_loc = 0;
            int i;
            for (i = 1; i < thebuf->transfer.udp.num_iovecs; i++) { //msg_num_iovecs
                ptl_info("copy iovec #%i of %i\n", i,
                         (int)thebuf->transfer.udp.num_iovecs);
                memcpy((buf_data + cur_iov_copy_loc),
                       buf_msg_hdr.msg_iov[i].iov_base,
                       buf_msg_hdr.msg_iov[i].iov_len);
                cur_iov_copy_loc =
                    cur_iov_copy_loc + buf_msg_hdr.msg_iov[i].iov_len;
            }
            thebuf->transfer.udp.my_iovec.iov_base = buf_data;
            thebuf->transfer.udp.my_iovec.iov_len = thebuf->rlength;
        } else {
            //if not, copy just the data in the 2 element iovec
            ptl_info("not an iovec \n");
            memcpy(buf_data, buf_msg_hdr.msg_iov[1].iov_base,
                   (size_t) buf_msg_hdr.msg_iovlen);
            buf_data_len = buf_msg_hdr.msg_iovlen;
            thebuf->transfer.udp.my_iovec.iov_base =
                buf_msg_hdr.msg_iov[1].iov_base;
            thebuf->transfer.udp.my_iovec.iov_len =
                buf_msg_hdr.msg_iov[1].iov_len;
        }

        thebuf->transfer.udp.data = (unsigned char *)buf_data;

        temp_sin = *(struct sockaddr_in *)buf_msg_hdr.msg_name;
        lensin = buf_msg_hdr.msg_namelen;

        int MAX_UDP_RECV_SIZE = 1488;
        uint32_t max_recv_size;
        max_recv_size = sizeof(int);
        getsockopt(ni->iface->udp.connect_s, SOL_SOCKET, SO_SNDBUF,
                   (int *)&MAX_UDP_RECV_SIZE, &max_recv_size);
        if (MAX_UDP_RECV_SIZE > 65507)
            MAX_UDP_RECV_SIZE = 65507;

        if ((thebuf->rlength + sizeof(buf_t)) > MAX_UDP_RECV_SIZE) {
            //this message is large enough to span multiple UDP messages, so we need to fetch them all
            //first message will be the portals buf, and data upto 64K
            //subsequent messages need to be added to the received data buffer as extra data
            //there is no finalization footer etc.
            //THIS ASSUMES UDP IS RELIABLE AND IN-ORDER, which it is not unless a reliability layer is present.
            //this also assumes that there is not data incoming from multiple sources on this socket/port           
            ptl_warn
                ("Message size exceeds that of a single datagram, this operational mode is not recommended \n");

            ptl_info("ni->iface is %p \n", ni->iface);

            buf_t *big_buf;

            //create or fetch a large message buffer from a linked list
            int found_one = 0;
            struct list_head *l, *t;

            list_for_each_safe(l, t, &ni->udp_list) {
                big_buf = list_entry(l, buf_t, list);
                ptl_info("list buf @%p \n", big_buf);
                ptl_info
                    ("compare in progress list item to present transfer %p %i %s:%i\n",
                     ni->iface, ni->iface->id.phys.pid,
                     inet_ntoa(big_buf->udp.src_addr.sin_addr),
                     ntohs(big_buf->udp.src_addr.sin_port));
                //We only need to check the pid
                if (ntohs(thebuf->udp.src_addr.sin_port) ==
                    ntohs(big_buf->udp.src_addr.sin_port)) {
                    //ptl_info("found match for system, checking logical NI \n");
                    //found a matching buf, check to see if addressing mode is correct
                    if (((!!(ni->options & PTL_NI_LOGICAL)) &&
                         (hdr->h1.physical == 0)) ||
                        ((!!(ni->options & PTL_NI_PHYSICAL)) &&
                         (hdr->h1.physical == 1))) {
                        //this is an exact match
                        found_one = 1;
                        ptl_info("found a matching in-progress transfer \n");
                        break;
                    }
                }

            }
            //if a buffer didn't already exist, this is a new incoming
            //large message, so allocate one
            if (found_one != 1) {
                //size is 16MB because we have 8 bits available for number of 64K packets
                //this is completely arbitrary, and could be adjusted up or down
                ptl_info
                    ("not an oustanding transfer, allocate a new buffer \n");
                //copy the buf over to the first iovec
                big_buf = calloc(1, sizeof(buf_t));
                memcpy(big_buf, buf_msg_hdr.msg_iov[0].iov_base,
                       sizeof(buf_t));
                //set the 16MB buffer
                big_buf->transfer.udp.data = calloc(1, 65536 << 8);
                ptl_info
                    ("memory region starting at %p allocated for transfer reception \n",
                     big_buf->transfer.udp.data);
                ptl_info("destination pid is: %s:%d \n",
                         inet_ntoa(big_buf->udp.src_addr.sin_addr),
                         ntohs(big_buf->udp.src_addr.sin_port));
                big_buf->transfer.udp.my_iovec.iov_len = 0;
                ptl_info("adding buffer @%p to the list \n", big_buf);
                //add the buffer to the udp outstanding transfer list
                INIT_LIST_HEAD(&big_buf->list);
                list_add_tail(&big_buf->list, &ni->udp_list);
                big_buf->transfer.udp.fragment_count = 0;
                big_buf->transfer.udp.cur_iov_copy_loc = 0;
            }

            ptl_info("copy data to big receive buffer for segment #%i\n",
                     hdr->fragment_seq);
            //Copy the incoming data to the big buffer's data region
            //In the correct section for its sequence number
            //so we don't have to worry about out of order segments, this is handled here
            ptl_info("copying to location: %p \n",
                     big_buf->transfer.udp.data +
                     ((MAX_UDP_RECV_SIZE -
                       sizeof(buf_t)) * hdr->fragment_seq));

            if (big_buf->transfer.udp.is_iovec) {
                if ((big_buf->put_md != NULL) || big_buf->get_md != NULL) {
                    if (big_buf->put_md != NULL) {
                        if (big_buf->put_md->options) {
                            if (big_buf->put_md->options & PTL_IOVEC) {
                                ptl_warn("IO vec put transfer %i \n",
                                         (int)big_buf->rlength);
                                recv_md = big_buf->put_md;
                            }
                        }
                    }
                } else {
                    if (big_buf->get_md->options) {
                        if (big_buf->get_md->options & PTL_IOVEC) {
                            ptl_info("IO vec get transfer %i \n",
                                     (int)big_buf->rlength);
                            recv_md = big_buf->get_md;
                        }
                    }
                }
            } else {
                ptl_info("Not a iovec recv \n");
                recv_md = NULL;
            }

            memcpy((big_buf->transfer.udp.data +
                    (((MAX_UDP_RECV_SIZE -
                       sizeof(buf_t)) * hdr->fragment_seq))),
                   buf_msg_hdr.msg_iov[1].iov_base, current_message_size);

            ptl_info("segment size was data:%i max data size:%lu \n",
                     current_message_size, MAX_UDP_RECV_SIZE - sizeof(buf_t));
            big_buf->transfer.udp.my_iovec.iov_len += current_message_size;

            //increment the fragment counter and check to see if it equals the total
            big_buf->transfer.udp.fragment_count++;
            ptl_info("have #%i segments of #%i size: %i\n",
                     (int)big_buf->transfer.udp.fragment_count,
                     (int)((thebuf->rlength /
                            (MAX_UDP_RECV_SIZE - sizeof(buf_t))) + 1),
                     (int)thebuf->rlength);
            //check to see if the transfer is complete
            if (big_buf->transfer.udp.fragment_count ==
                ((thebuf->rlength / (MAX_UDP_RECV_SIZE - sizeof(buf_t))) +
                 1)) {
                //we're done the transfer
                ptl_info
                    ("transfer complete length: %i, removing buffer from active transfers list \n",
                     (int)big_buf->transfer.udp.my_iovec.iov_len);

                ptl_info("data at end (%p) is: %llu \n",
                         (big_buf->transfer.udp.data + big_buf->rlength),
                         *(long long unsigned int *)(big_buf->transfer.
                                                     udp.data +
                                                     (current_message_size)));

                if (!(list_empty(&ni->udp_list))) {
                    list_del(l);
                }
                big_buf->transfer.udp.my_iovec.iov_base =
                    big_buf->transfer.udp.data;
                big_buf->transfer.udp.my_iovec.iov_len = thebuf->rlength;
                thebuf = big_buf;
                thebuf->recv_buf = big_buf;
            }
            //if it does not, return nothing as we are still in progress
            else {
                ptl_info
                    ("transfer not complete, wait for more incoming datagrams \n");
                return NULL;
            }
        }
    } else {
        //this is a small transfer with immediate data, fetch it.
        err =
            ptl_recvfrom(ni->iface->udp.connect_s, thebuf, sizeof(*thebuf), 0,
                         (struct sockaddr *)&temp_sin, &lensin, ni);
        if (err == -1) {
            if (errno != EAGAIN) {
                free(thebuf);
                WARN();
                ptl_warn("error receiving main buffer from socket: %d %s\n",
                         ni->iface->udp.connect_s, strerror(errno));
                return NULL;

            } else {
                //Nothing to fetch
                free(thebuf);
                return NULL;
            }
        }
    }

    if (&thebuf->transfer.udp.conn_msg != NULL) {
        ptl_info("process received message type \n");
        struct udp_conn_msg *msg = &thebuf->transfer.udp.conn_msg;

        if (msg->msg_type == le16_to_cpu(UDP_CONN_MSG_REQ)) {
            ptl_info("received a UDP connection request \n");
            thebuf->type = BUF_UDP_CONN_REQ;
        } else if (msg->msg_type == le16_to_cpu(UDP_CONN_MSG_REP)) {
            ptl_info("recieved a UDP connection reply \n");
            thebuf->type = BUF_UDP_CONN_REP;
        } else {
            ptl_info("received a UDP data packet \n");
            thebuf->type = BUF_UDP_RECEIVE;
        }

    }

    if (thebuf->type == BUF_UDP_RECEIVE) {
        thebuf->obj.obj_ni = ni;
        thebuf->conn =
            get_conn(ni, (ptl_process_t)le32_to_cpu(hdr->h1.src_rank));
        //atomic_inc(&thebuf->conn->udp.recv_seq);
    }

    ptl_info
        ("received data from %s:%i type:%i data size: %lu message size:%u %i \n",
         inet_ntoa(temp_sin.sin_addr), ntohs(temp_sin.sin_port), thebuf->type,
         sizeof(*(thebuf->data)), (int)thebuf->rlength, err);

    thebuf->udp.src_addr = temp_sin;
    return (buf_t *)thebuf;
}

/* change the state of conn; we are now connected (UO & REB) */
void PtlSetMap_udp(ni_t *ni, ptl_size_t map_size,
                   const ptl_process_t *mapping)
{
    int i;

    ptl_info("Creating a connection for PTlSetMapn");
    for (i = 0; i < map_size; i++) {
#if WITH_TRANSPORT_SHMEM
        if (mapping[i].phys.nid != ni->iface->id.phys.nid) {
#endif
            conn_t *conn;

            ptl_process_t id;
            id.rank = i;

            conn = get_conn(ni, id);
            if (!conn) {
                /* It's hard to recover from here. */
                WARN();
                abort();
                return;
            };

            if (conn->transport.type == CONN_TYPE_UDP)
                conn->transport = transport_udp;

            conn->udp.dest_addr.sin_addr.s_addr =
                nid_to_addr(mapping[i].phys.nid);
            conn->udp.dest_addr.sin_port = pid_to_port(mapping[i].phys.pid);
            ptl_info("setmap connection: %s:%i rank: %i\n",
                     inet_ntoa(conn->udp.dest_addr.sin_addr),
                     htons(conn->udp.dest_addr.sin_port), id.rank);

            //We are not connected until we've exchanged messages
            conn_put(conn);            /* from get_conn */

        }
        ptl_info("Done creating connection for PtlSetMap\n");
#if WITH_TRANSPORT_SHMEM
    }                                  //end the check for offnode connections
#endif
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

    /* Create a buffer for sending the connection request message */
    buf_t *conn_buf = (buf_t *)calloc(1, sizeof(buf_t));
    conn_buf->type = BUF_UDP_CONN_REQ;

    /* Send the connect request. */
    struct udp_conn_msg msg;
    msg.msg_type = cpu_to_le16(UDP_CONN_MSG_REQ);
    msg.port = ntohs(ni->udp.src_port);
    msg.req.options = ni->options;
    msg.req.src_id = ni->id;
    msg.req_cookie = (uintptr_t) conn;


    struct req_hdr *hdr;
    hdr = (struct req_hdr *)&conn_buf->internal_data;
    conn_buf->data = (void *)&conn_buf->internal_data;

    ((struct hdr_common *)hdr)->version = PTL_HDR_VER_1;

    hdr->h1.src_nid = cpu_to_le32(ni->id.phys.nid);
    hdr->h1.src_pid = cpu_to_le32(ni->id.phys.pid);
    ptl_info("addressing type for connection is: %x \n",
             !!(ni->options & PTL_NI_LOGICAL));
    hdr->h1.physical = !!(ni->options & PTL_NI_PHYSICAL);

    conn_buf->transfer.udp.conn_msg = msg;
    conn_buf->length = (sizeof(buf_t));
    conn_buf->conn = conn;
    conn_buf->udp.dest_addr = &conn->sin;

    struct sockaddr_in temp_sin;

    temp_sin.sin_addr.s_addr = nid_to_addr(ni->id.phys.nid);

    ptl_info("nid: %s pid: %i \n", inet_ntoa(temp_sin.sin_addr),
             (ni->id.phys.pid));
    ptl_info("addr: %s : %i \n", inet_ntoa(conn->sin.sin_addr),
             ntohs(conn_buf->udp.dest_addr->sin_port));

    if (conn_buf->udp.dest_addr->sin_port == ntohs(ni->id.phys.pid)) {
        if (temp_sin.sin_addr.s_addr == conn->sin.sin_addr.s_addr) {
            //we are sending to ourselves, so we don't need a connection
            ptl_info("sending to self\n");
            conn_buf->udp.src_addr = conn->sin;

            ni->udp.self_recv_addr = conn_buf;
            ni->udp.self_recv_len = conn_buf->length;
            //since setmap does not have to be run, make sure its valid
            ni->udp.dest_addr = conn_buf->udp.dest_addr;
            ni->udp.map_done = 1;
            //now let the progress thread know that we've sent something to ourselves
            atomic_set(&ni->udp.self_recv, 1);
            return PTL_OK;
        }
    }

    ptl_info("to send msg size: %lu in UDP message size: %lu\n", sizeof(msg),
             sizeof(buf_t));

    /* Send the request to the listening socket on the remote node. */
    /* This does not send just the msg, but a buf to maintain compatibility with the progression thread */
    ret =
        ptl_sendto(ni->iface->udp.connect_s, conn_buf, conn_buf->length, 0,
                   (struct sockaddr *)&conn->sin, sizeof(conn->sin), ni);
    if (ret == -1) {
        WARN();
        return PTL_FAIL;
    }

    ptl_info
        ("succesfully sent connection request to listener: %s:%d from: %d size:%i\n",
         inet_ntoa(conn_buf->udp.dest_addr->sin_addr),
         htons(conn_buf->udp.dest_addr->sin_port), htons(ni->udp.src_port),
         ret);

    free(conn_buf);
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
    .NIFini = cleanup_udp,
};
