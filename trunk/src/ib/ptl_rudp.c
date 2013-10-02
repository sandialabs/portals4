/* TODO LIST:
 *      --send header needs to deal with iovecs 
 *      --need to handle NACKs retransmission
 *      --timeouts need to be handled (progress thread?)
 *      --needs to work for the PPE (NACK/ACK destination pids)
 *
 * Notes:   On recv side, drop messages with seq_num below
 *          that of the current seq_num
 */


#include "ptl_loc.h"
#include "ptl_rudp.h"

#if WITH_RUDP
int process_rudp_send_hdr(buf_t * buf, int len, ni_t *ni){

    int seq_num = 0;
    if (buf->type != BUF_UDP_CONN_REP && buf->type != BUF_UDP_CONN_REQ){
        ptl_info("set sequence number \n");
        seq_num = atomic_inc(&buf->conn->udp.send_seq_num);
        buf->transfer.udp.seq_num = seq_num;
        req_hdr_t *hdr;

        if (buf->internal_data != NULL){
            hdr = (req_hdr_t *)buf->internal_data;
        } else{
            hdr = (req_hdr_t *)&buf->data;
        }
        
        conn_t *temp_conn;
        
        temp_conn =  get_conn(ni, (ptl_process_t)le32_to_cpu(hdr->h1.src_rank));
        if (temp_conn == NULL)
            ptl_warn("RUDP couldn't fetch conn info, perhaps conn is closed?\n");       
        
        INIT_LIST_HEAD(&buf->list); 
        list_add_tail(&buf->list, &temp_conn->udp.rel_queued_bufs);
        ptl_info("^^^added buf seq num: %i to rel_queue\n",buf->transfer.udp.seq_num);
        buf->udp.in_progress = 1;
    }
    ptl_info("send header assembly complete\n");
    return 0;
}


int process_rudp_recv_hdr(buf_t * buf, int len, ni_t *ni){

    struct list_head *l, *t;
    buf_t * temp_buf = NULL;
    int found_one,ret = 0;
    
    //Look at the incoming header to find the connection
    req_hdr_t *hdr;

    if (buf->internal_data != NULL){
        hdr = (req_hdr_t *)buf->internal_data;
    } else{
        hdr = (req_hdr_t *)&buf->data;
    }
        
    //get the connection for this communication
    //this will be nid for physical addressing and rank for logical
    __le32 src_id = le32_to_cpu(hdr->h1.src_rank);

    ptl_process_t id;
    id.rank = src_id;
 
    conn_t * temp_conn;
    ptl_info("get conn for rank %i at local rank: %i\n",
             le32_to_cpu(hdr->h1.src_rank),ni->id.rank);
    temp_conn = get_conn(ni, (ptl_process_t)le32_to_cpu(hdr->h1.src_rank));
    if (temp_conn == NULL)
        ptl_error("RUDP couldn't get conn info on recv\n");

    ptl_info("got conn info \n");

    int last_loop = -1;

    //if(!list_empty(&temp_conn->udp.rel_queued_bufs))
    if (buf->type == BUF_UDP_ACK || buf->type == BUF_UDP_NACK){
    list_for_each_safe(l, t, &temp_conn->udp.rel_queued_bufs) {
        temp_buf = list_entry(l, buf_t, list);
        assert(&temp_buf != NULL);
        ptl_info("checking udp seq numbers %i:%i\n",temp_buf->transfer.udp.seq_num,
                buf->transfer.udp.seq_num);
        if (temp_buf->transfer.udp.seq_num == buf->transfer.udp.seq_num){
            //if (!(list_empty(&temp_conn->udp.rel_queued_bufs))){
                ptl_info("found one, delete it from queued bufs\n");
                //if (buf->type != BUF_UDP_NACK)
                //list_del(l);
                found_one = 1;
                //if (list_empty(&temp_conn->udp.rel_queued_bufs)){
                //   INIT_LIST_HEAD(&temp_conn->udp.rel_queued_bufs); 
                //}
                break;
            //}
        }
        else{
            ptl_info("no match\n");
            if (last_loop == temp_buf->transfer.udp.seq_num)
                break;
            last_loop = temp_buf->transfer.udp.seq_num;
            if (temp_buf->transfer.udp.seq_num == 0)
                break;
        }
    }
    }

    ptl_info("RUDP received buf of type: %i \n",buf->type);

    //Check to see if this receive is an ACK/NACK from a target
    switch (buf->type){
        //For an ACK, just remove the buf from the current
        //transfers list
        case BUF_UDP_ACK: {
            if (found_one == 1){
                ptl_info("***********ACK received seq: %i*********** \n",
                         buf->transfer.udp.seq_num);
                //if (!list_empty(&temp_conn->udp.rel_queued_bufs))
                //list_del_init(l);
                //if (list_empty(&temp_conn->udp.rel_queued_bufs))
                //    INIT_LIST_HEAD(&temp_conn->udp.rel_queued_bufs);
                break;
            }
            else{
                ptl_error("ACK for buf not in active transfer list\n");
                break;
            }
        }   
    
        //For a NACK, we need to send the buf again
        case BUF_UDP_NACK: {
            if (found_one ==1){
                //only handles the non-large message case
                ptl_info("@@@@ RUDP NACK sendto retransmission @@@@@\n");
                ret = sendto(ni->iface->udp.connect_s, temp_buf, 
                             sizeof(*temp_buf), 0,
                             (struct sockaddr *)temp_buf->udp.dest_addr, 
                             sizeof(*temp_buf->udp.dest_addr));
                if (ret == -1)
                    return ret;
                //add the buffer back to the list
                //INIT_LIST_HEAD(&temp_buf->list);
                //list_add_tail(&temp_buf->list, &temp_conn->udp.rel_queued_bufs);
            }
            break;
        }
    
        //send an ACK or NACK depending on the data received
        case BUF_UDP_RECEIVE || BUF_TGT || BUF_TRIGGERED_ME: {
            ptl_info("process recv for reliability want seq num: %i\n",
                     atomic_read(&temp_conn->udp.recv_seq_num));
            if (atomic_read(&temp_conn->udp.recv_seq_num) == buf->transfer.udp.seq_num){
                //got the correct message
                atomic_inc(&temp_conn->udp.recv_seq_num);
                ptl_info("next seq number set to: %i\n",atomic_read(&temp_conn->udp.recv_seq_num));
                //now we need to send an ack
                ptl_info("@@@@@@@send back ACK for seq num: %i to %s:%i\n",
                         buf->transfer.udp.seq_num,
                         inet_ntoa(temp_conn->sin.sin_addr),
                         ntohs(temp_conn->sin.sin_port));
                         /*inet_ntoa(buf->udp.src_addr.sin_addr),
                         ntohs(buf->udp.src_addr.sin_port));*/
                buf->type = BUF_UDP_ACK;
            }
            else{
                ptl_info("@@@@@@@@@send NACK for seq num: %i \n",buf->transfer.udp.seq_num);
                buf->type = BUF_UDP_NACK;
            }
            
            //now send either the ACK or NACK back
            if (ni->options & PTL_NI_PHYSICAL) {
                hdr->h1.src_nid = cpu_to_le32(ni->id.phys.nid);
                hdr->h1.src_pid = cpu_to_le32(ni->id.phys.pid);
            } else {
                hdr->h1.src_rank = cpu_to_le32(ni->id.rank);
            }

            //TODO: strip out the data so we have less network load
            //      on the ACK/NACK
            sendto(ni->iface->udp.connect_s, buf, sizeof(*buf), 0,
                   (struct sockaddr *)&temp_conn->sin,
                   sizeof(*&temp_conn->sin));
            break;           
        }
        
        default: 
            goto cleanup;
        

    } //end switch


cleanup:
    conn_put(temp_conn);
    if (buf->type != BUF_UDP_RECEIVE){
        //horrible hacking the errno return
        //basically indicate back to the portals lib
        //that nothing was there.
        errno = EAGAIN; 
        return -1;
    }
    return 0;
}
#endif


/**
 * @brief Intercept sendmsg calls for reliability header processing
 *
 * This allows the non-RUDP case to simply pass through to sendmsg
 * RUDP calls have a reliabilty header applied to it for the
 * given destination.
 *
 * @param[in] sockfd The socket to use for the send
 * @param[in] msg    The message to be sent, in strcut msghdr form
 * @param[in] flags  Appropriate flags to pass for the sendmsg operation
 * @param[in] ni     The portals network interface to use 
 *
 * @return size      Size of the message sent
 */
ssize_t ptl_sendmsg(int sockfd, const struct msghdr *msg, int flags, 
                    ni_t *ni)
{
    ssize_t ret;
    int hdr_status;
#if !WITH_RUDP
    ret = sendmsg(sockfd, msg, flags);
#else
    //send this reliably
    hdr_status = process_rudp_send_hdr((void *)(msg->msg_iov[0].iov_base),
                                       (int)(msg->msg_iov[0].iov_len), ni);
    
    //begin the send
    ptl_info("@@@@@@@@@ RUDP sendmsg @@@@@@@@@\n");
    ret = sendmsg(sockfd, msg, flags);
#endif    
    return ret;
}

/**
 * @brief Intercept sendto calls for reliability header processing
 *
 * This allows the non-RUDP case to simply pass through to sendto
 * RUDP calls have a reliabilty header applied to it for the
 * given destination.
 *
 * @param[in] sockfd     The socket to use for the send
 * @param[in] buf        The buffer to be sent
 * @param[in] len        The length of the buffer
 * @param[in] flags      Appropriate flags to pass for the sendmsg operation
 * @param[in] dest_addr  The destination address (struct sockaddr)
 * @param[in] addrlen    The length of the dest_addr struct
 * @param[in] ni         The portals network interface to use  
 *
 * @return size          Size of the message sent
 *
 */
ssize_t ptl_sendto(int sockfd, buf_t *buf, size_t len, int flags,
                   struct sockaddr *dest_addr, socklen_t addrlen,
                   ni_t *ni)
{
    ssize_t ret;
    int hdr_status;
#if !WITH_RUDP
    ret = sendto(sockfd,buf,len,flags,dest_addr,addrlen);
#else
    //send this reliably
    hdr_status = process_rudp_send_hdr(buf, len, ni);
    
    //begin send
    ptl_info("@@@@@@@@@ RUDP sendto @@@@@@@@@\n");
    ret = sendto(sockfd,buf,len,flags,dest_addr,addrlen);
    if (ret == -1)
        return ret;
 
   
#endif
    return ret;
}


/**
 * @brief Intercept recvmsg calls for reliability header processing
 *
 * This allows the non-RUDP case to simply pass through to recvmsg
 * RUDP calls have a reliabilty header processed
 *
 * @param[in] sockfd The socket to use for the recv
 * @param[in] msg    The message to be recvd, in strcut msghdr form
 * @param[in] flags  Appropriate flags to pass for the recvmsg operation
 * @param[in] ni     The portals network interface to use 
 *
 * @return size      Size of the message received
 */

ssize_t ptl_recvmsg(int sockfd, struct msghdr *msg, int flags, 
                    ni_t *ni)
{
    ssize_t ret;
    int hdr_status;
#if !WITH_RUDP
    ret = recvmsg(sockfd, msg, flags);
#else
    //send this reliably
    ptl_info("@@@@@@@@@ RUDP recvmsg @@@@@@@@@\n");
    ret = recvmsg(sockfd, msg, flags);
    if (ret == -1)
        return ret;
    if (flags == MSG_PEEK)
        return ret;

    hdr_status = process_rudp_recv_hdr(msg->msg_iov[0].iov_base, 
                                       msg->msg_iov[0].iov_len, ni);
#endif
    return ret;
}

/**
 * @brief Intercept recvfrom calls for reliability header processing
 *
 * This allows the non-RUDP case to simply pass through to recvfrom
 * RUDP calls have a reliabilty header processed
 *
 * @param[in] sockfd     The socket to use for the recv
 * @param[in] buf        A buffer to put the message in
 * @param[in] len        The length of the buffer
 * @param[in] flags      Flags to pass to the recvfrom operation
 * @param[in] src_addr   The source address (struct sockaddr)
 * @param[in] addrlen    The length of the src_addr struct
 * @param[in] ni         The portals network interface to use  
 *  
 * @return size          Size of the message received
 *
 */
ssize_t ptl_recvfrom(int sockfd, buf_t *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen, ni_t *ni)
{
    ssize_t ret;
    int hdr_status;
#if !WITH_RUDP
    ret = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
#else
    //recv this reliably
    ret = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    //error processing, just hand the error back to the caller
    if (ret == -1 || ret == 0)
        return ret;
    if (flags == MSG_PEEK)
        return ret;   

    hdr_status = process_rudp_recv_hdr(buf, len, ni);

#endif
    return ret;
}



