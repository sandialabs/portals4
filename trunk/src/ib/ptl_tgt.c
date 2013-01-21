/**
 * @file ptl_tgt.c
 *
 * @brief Target state machine.
 */
#include "ptl_loc.h"

/**
 * @brief Target state names for debugging output.
 */
static char *tgt_state_name[] = {
	[STATE_TGT_START]		= "tgt_start",
	[STATE_TGT_DROP]		= "tgt_drop",
	[STATE_TGT_GET_MATCH]		= "tgt_get_match",
	[STATE_TGT_GET_LENGTH]		= "tgt_get_length",
	[STATE_TGT_WAIT_CONN]		= "tgt_wait_conn",
	[STATE_TGT_DATA]		= "tgt_data",
	[STATE_TGT_DATA_IN]		= "tgt_data_in",
	[STATE_TGT_START_COPY]		= "tgt_start_copy",
	[STATE_TGT_RDMA]		= "tgt_rdma",
	[STATE_TGT_ATOMIC_DATA_IN]	= "tgt_atomic_data_in",
	[STATE_TGT_SWAP_DATA_IN]	= "tgt_swap_data_in",
	[STATE_TGT_DATA_OUT]		= "tgt_data_out",
	[STATE_TGT_WAIT_RDMA_DESC]	= "tgt_wait_rdma_desc",
	[STATE_TGT_SEND_ACK]		= "tgt_send_ack",
	[STATE_TGT_SEND_REPLY]		= "tgt_send_reply",
	[STATE_TGT_COMM_EVENT]		= "tgt_comm_event",
	[STATE_TGT_OVERFLOW_EVENT]	= "tgt_overflow_event",
	[STATE_TGT_WAIT_APPEND]		= "tgt_wait_append",
	[STATE_TGT_CLEANUP]		= "tgt_cleanup",
	[STATE_TGT_CLEANUP_2]		= "tgt_cleanup_2",
	[STATE_TGT_ERROR]		= "tgt_error",
	[STATE_TGT_DONE]		= "tgt_done",
};

/**
 * @brief Make a comm event from a message buf.
 *
 * @param[in] buf The message buf received by the target.
 */
static void make_comm_event(buf_t *buf)
{
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	unsigned operation = hdr->h1.operation;
	ptl_event_kind_t type;

	if (buf->ni_fail || !(buf->le->options
				& PTL_LE_EVENT_SUCCESS_DISABLE)) {

		if (operation == OP_PUT)
			type = PTL_EVENT_PUT;
		else if (operation == OP_GET)
			type = PTL_EVENT_GET;
		else
			type = PTL_EVENT_ATOMIC;

		make_target_event(buf, buf->pt->eq, type,
				  buf->le->user_ptr, buf->start);
	}

	buf->event_mask &= ~XT_COMM_EVENT;
}

/**
 * @brief Make a CT comm event from a buf.
 *
 * @param[in] buf The message buf received by the target.
 */
static void make_ct_comm_event(buf_t *buf)
{
	int bytes = (buf->le->options & PTL_LE_EVENT_CT_BYTES) ?
			CT_MBYTES : CT_EVENTS;

	make_ct_event(buf->le->ct, buf, bytes);

	buf->event_mask &= ~XT_CT_COMM_EVENT;
}

/**
 * @brief Initialize buf event mask.
 *
 * @param[in] buf The message buf received by the target.
 */
static void init_events(buf_t *buf)
{
	if (buf->pt->eq && !(buf->le->options &
				    PTL_LE_EVENT_COMM_DISABLE))
		buf->event_mask |= XT_COMM_EVENT;

	if (buf->le->ct && (buf->le->options &
				   PTL_LE_EVENT_CT_COMM))
		buf->event_mask |= XT_CT_COMM_EVENT;
}

/**
 * @brief Copy a data segment to an LE/ME list element and
 * save starting address.
 *
 * @param[in] buf The message buf received by the target.
 * @param[in,out] me The LE/ME list element.
 * @param[in] data The data to transfer.
 *
 * @return status
 */
static int tgt_copy_in(buf_t *buf, me_t *me, void *data)
{
	int err;
	ptl_size_t offset = buf->moffset;
	ptl_size_t length = buf->mlength;

	if (me->num_iov) {
#if IS_PPE
		mr_t *mr;

		if (me->mr_start)
			mr = me->mr_start;
		else {
			err = mr_lookup_app(obj_to_ni(me), me->start, length*sizeof(ptl_iovec_t), &mr);
			if (err) {
				WARN();
				return err;
			}
		}

		err = iov_copy_in(data, addr_to_ppe(me->start, mr), me->mr_list,
						  me->num_iov, offset, length);

		if (!me->mr_start)
			mr_put(mr);
#else
		err = iov_copy_in(data, (ptl_iovec_t *)me->start, me->mr_list,
				  me->num_iov, offset, length);
#endif
	} else {
		void *start = me->start + offset;
#if IS_PPE
		mr_t *mr;

		if (me->mr_start)
			mr = me->mr_start;
		else {
			err = mr_lookup_app(obj_to_ni(me), start, length, &mr);
			if (err) {
				WARN();
				return err;
			}
		}

		memcpy(addr_to_ppe(start, mr), data, length);
		err = PTL_OK;

		if (!me->mr_start)
			mr_put(mr);
#else
		memcpy(start, data, length);
#endif
		err = PTL_OK;
	}

	return err;
}

/**
 * @brief Handle atomic data in from a data segment to a list element and
 * save the starting address.
 *
 * @param[in] buf The message buf received by the target.
 * @param[in,out] me The LE/ME list element.
 * @param[in] data The data to transfer.
 *
 * @return status
 */
static int atomic_in(buf_t *buf, me_t *me, void *data)
{
	int err;
	ptl_size_t offset = buf->moffset;
	ptl_size_t length = buf->mlength;
	atom_op_t op;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;

	op = atom_op[hdr->atom_op][hdr->atom_type];
	assert(op);

	if (me->num_iov) {
#if IS_PPE
		mr_t *mr;

		if (me->mr_start)
			mr = me->mr_start;
		else {
			err = mr_lookup_app(obj_to_ni(me), me->start, length*sizeof(ptl_iovec_t), &mr);
			if (err) {
				WARN();
				return err;
			}
		}

		err = iov_atomic_in(op, atom_type_size[hdr->atom_type],
							data, addr_to_ppe(me->start, mr), me->mr_list,
							me->num_iov, offset, length);

		if (!me->mr_start)
			mr_put(mr);
#else
		err = iov_atomic_in(op, atom_type_size[hdr->atom_type],
							data, (ptl_iovec_t *)me->start, me->mr_list,
							me->num_iov, offset, length);
#endif
	} else {
		void *start = me->start + offset;
#if IS_PPE
		mr_t *mr;

		if (me->mr_start)
			mr = me->mr_start;
		else {
			err = mr_lookup_app(obj_to_ni(me), start, length, &mr);
			if (err) {
				WARN();
				return err;
			}
		}

		(*op)(addr_to_ppe(start, mr), data, length);

		if (!me->mr_start)
			mr_put(mr);
#else
		(*op)(start, data, length);
#endif
		err = PTL_OK;
	}

	return err;
}

/**
 * @brief Prepare a send buf to send an ack or reply message to the initiator.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return status
 */
static int prepare_send_buf(buf_t *buf)
{
	buf_t *send_buf;
	int err;
	ack_hdr_t *ack_hdr;
	ni_t *ni = obj_to_ni(buf);

	/* Determine whether to reuse the current buffer to reply, or get
	 * a new one. */
#if WITH_TRANSPORT_IB
	if (buf->conn->transport.type == CONN_TYPE_RDMA)
		err = buf_alloc(ni, &send_buf);
	else
#endif
		{
		if (!(buf->event_mask & XT_ACK_EVENT)) {
			/* No ack but a reply. The current buffer cannot be
			 * reused. */
			err = buf->conn->transport.buf_alloc(ni, &send_buf);
		} else {
			/* Itself. */
			send_buf = NULL;
			err = PTL_OK;
		}
	}

	if (err) {
		WARN();
		return PTL_FAIL;
	}

 	if (send_buf) {
		/* link send buf to buf */
#if WITH_TRANSPORT_IB
		send_buf->transfer.rdma.xxbuf = buf;
#endif
		buf_get(buf);
		buf->send_buf = send_buf;

		/* initiate response header */
		ack_hdr = (ack_hdr_t *)send_buf->data;

		ack_hdr->h1.data_in = 0;
		ack_hdr->h1.data_out = 0;
		ack_hdr->h1.version = PTL_HDR_VER_1;
		ack_hdr->h1.handle	= ((req_hdr_t *)buf->data)->h1.handle;
#if WITH_TRANSPORT_UDP
		ptl_info(" preparing response for handle: %i \n",ack_hdr->h1.handle);
#endif
#if IS_PPE
		ack_hdr->h1.hash = cpu_to_le32(ni->mem.hash);
		ack_hdr->h1.dst_nid = cpu_to_le32(buf->target.phys.nid);
		ack_hdr->h1.dst_pid = cpu_to_le32(buf->target.phys.pid);
		ack_hdr->h1.physical = !!(ni->options & PTL_NI_PHYSICAL);
		ack_hdr->h1.ni_type = ni->ni_type;
#endif

		send_buf->length = sizeof(*ack_hdr);
	} else {
#if IS_PPE
		ack_hdr = (ack_hdr_t *)buf->data;

		ack_hdr->h1.dst_nid = cpu_to_le32(buf->target.phys.nid);
		ack_hdr->h1.dst_pid = cpu_to_le32(buf->target.phys.pid);
#endif
	}

	return PTL_OK;
}

/**
 * @brief Initialize offset and optionally iov from moffset.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return status
 */
static int init_local_offset(buf_t *buf)
{
	me_t *me = buf->me;

	buf->cur_loc_iov_index = 0;
	buf->cur_loc_iov_off = 0;

	if (me->num_iov) {
		ptl_iovec_t *iov = (ptl_iovec_t *)me->start;
		ptl_size_t i = 0;
		ptl_size_t loc_offset = 0;
		ptl_size_t iov_offset = 0;

		for (i = 0; i < me->num_iov && loc_offset < buf->moffset;
			i++, iov++) {
			iov_offset = buf->moffset - loc_offset;
			if (iov_offset > iov->iov_len)
				iov_offset = iov->iov_len;
			loc_offset += iov_offset;
		}

		if (loc_offset < buf->moffset)
			return PTL_FAIL;

		buf->cur_loc_iov_index = i;
		buf->cur_loc_iov_off = iov_offset;
#if IS_PPE
		buf->start = ((ptl_iovec_t *)(me->mr_start->ppe_addr))[i].iov_base + iov_offset;
#else
		buf->start = iov->iov_base + iov_offset;
#endif
	} else {
		buf->cur_loc_iov_off = buf->moffset;
		buf->start = me->start + buf->moffset;
	}

	return PTL_OK;
}

/**
 * @brief target start state.
 *
 * This state is reached when a portals request is received by in ptl_recv.
 * The start state initializes the message buf.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_start(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	ptl_pt_index_t pt_index;
	ptl_process_t initiator;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;

	buf->operation = hdr->h1.operation;
	buf->pt = NULL;
	buf->in_atomic = 0;
	buf->matching.le = NULL;
	buf->le = NULL;
	buf->indir_sge = NULL;
	buf->send_buf = NULL;

#if IS_PPE
	buf->target.phys.nid = le32_to_cpu(hdr->src_nid);
	buf->target.phys.pid = le32_to_cpu(hdr->src_pid);
#endif

	switch (hdr->h1.operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (hdr->ack_req != PTL_NO_ACK_REQ)
			buf->event_mask |= XT_ACK_EVENT;
		break;
	case OP_GET:
	case OP_FETCH:
	case OP_SWAP:
		buf->event_mask |= XT_REPLY_EVENT;
		break;
	default:
		return STATE_TGT_ERROR;
	}

	/* initialize fields */
	INIT_LIST_HEAD(&buf->unexpected_list);
#if WITH_TRANSPORT_IB
	INIT_LIST_HEAD(&buf->transfer.rdma.rdma_list);
#endif
	buf->in_atomic = 0;

	/* get per conn info */
	initiator.phys.nid = le32_to_cpu(hdr->src_nid);
	initiator.phys.pid = le32_to_cpu(hdr->src_pid);

#if WITH_TRANSPORT_UDP
	ptl_info("initiator nid: %i pid: %i \n",hdr->src_nid,hdr->src_pid);
	ptl_info("ni: %p conn pool (%p) size: %i \n",ni,&ni->conn_pool,ni->conn_pool.size);
	ptl_info("buffer ni: %p \n",buf->obj.obj_ni);
#endif
	buf->conn = get_conn(ni, initiator);
	if (unlikely(!buf->conn)) {
		WARN();
		return STATE_TGT_ERROR;
	}

	/* allocate the ack/reply send buf */
	if (buf->event_mask & (XT_ACK_EVENT | XT_REPLY_EVENT)) {
		int err = prepare_send_buf(buf);
		if (err)
			return STATE_TGT_ERROR;
	}

	pt_index = le32_to_cpu(hdr->pt_index);
	if (pt_index >= ni->limits.max_pt_index) {
		buf->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}

	buf->pt = &ni->pt[pt_index];
	if (!buf->pt->in_use) {
		buf->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}

	/* synchronize with enable/disable APIs */
	PTL_FASTLOCK_LOCK(&buf->pt->lock);
	if (buf->pt->state != PT_ENABLED) {
		PTL_FASTLOCK_UNLOCK(&buf->pt->lock);
		buf->ni_fail = PTL_NI_PT_DISABLED;
		return STATE_TGT_DROP;
	}
	buf->pt->num_tgt_active++;
	PTL_FASTLOCK_UNLOCK(&buf->pt->lock);

	return STATE_TGT_GET_MATCH;
}

/**
 * @brief target drop message state.
 *
 * This state is reached when a request message is dropped.
 * If an ack or reply response is going to be sent make sure
 * we are connected else cleanup the buf and exit.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int request_drop(buf_t *buf)
{
	/* we didn't match anything so set start to NULL */
	buf->start = NULL;
	buf->put_resid = 0;
	buf->get_resid = 0;

	if (buf->event_mask & (XT_ACK_EVENT | XT_REPLY_EVENT)) {
		/* if we are already connected to the initiator skip wait_conn */
		if (likely(buf->conn->state >= CONN_STATE_CONNECTED))
			return STATE_TGT_DATA;
		else
			return STATE_TGT_WAIT_CONN;
	} else
		return STATE_TGT_CLEANUP;
}

/**
 * @brief Check if message matches a matching list element.
 *
 * @param[in] buf The message buf received by the target.
 * @param[in] me The matching list element.
 *
 * @return 1 If the message matches the ME.
 * @return 0 If the message does not match the ME.
 */
int check_match(buf_t *buf, const me_t *me)
{
	const ni_t *ni = obj_to_ni(buf);
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	ptl_size_t offset;
	ptl_size_t length = le64_to_cpu(hdr->rlength);
	ptl_size_t req_off = le64_to_cpu(hdr->roffset);

	if (ni->options & PTL_NI_LOGICAL) {
		ptl_process_t initiator;

		initiator.rank = le32_to_cpu(hdr->src_rank);

		if (!(me->id.rank == PTL_RANK_ANY ||
		     (me->id.rank == initiator.rank))) {
			return 0;
		}
	} else {
		ptl_process_t initiator;

		initiator.phys.nid = le32_to_cpu(hdr->src_nid);
		initiator.phys.pid = le32_to_cpu(hdr->src_pid);

		if (!(me->id.phys.nid == PTL_NID_ANY ||
		     (me->id.phys.nid == initiator.phys.nid)))
			return 0;
		if (!(me->id.phys.pid == PTL_PID_ANY ||
		     (me->id.phys.pid == initiator.phys.pid)))
			return 0;
	}

	offset = (me->options & PTL_ME_MANAGE_LOCAL) ?
			me->offset : req_off;

	if ((me->options & PTL_ME_NO_TRUNCATE) &&
	    ((offset + length) > me->length))
			return 0;

	return (le64_to_cpu(hdr->match_bits) | me->ignore_bits) ==
		(me->match_bits | me->ignore_bits);
}

/**
 * @brief Check if message passes permissions check.
 *
 * @param[in] buf The message buf received by the target.
 * @param[in] le The list/element that the message matches.
 *
 * @return The ni fail value to use.
 * @return PTL_NI_OK The message passes permission check.
 * @return PTL_NI_PERM_VIOLATION The message doesn't match uid.
 * @return PTL_NI_OP_VIOLATION The message has an invalid operation.
 */
int check_perm(buf_t *buf, const le_t *le)
{
	int ret = PTL_NI_OK;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	uint32_t uid = le32_to_cpu(hdr->uid);

	if (!(le->uid == PTL_UID_ANY || (le->uid == uid))) {
		ret = PTL_NI_PERM_VIOLATION;
	} else {
		switch (buf->operation) {
		case OP_ATOMIC:
		case OP_PUT:
			if (!(le->options & PTL_ME_OP_PUT))
				ret = PTL_NI_OP_VIOLATION;
			break;

		case OP_GET:
			if (!(le->options & PTL_ME_OP_GET))
				ret = PTL_NI_OP_VIOLATION;
			break;

		case OP_FETCH:
		case OP_SWAP:
			if ((le->options & (PTL_ME_OP_PUT | PTL_ME_OP_GET)) !=
			    (PTL_ME_OP_PUT | PTL_ME_OP_GET))
				ret = PTL_NI_OP_VIOLATION;
			break;
		}
	}

	return ret;
}

/**
 * @brief target get match state.
 *
 * This state is reached after the start state and looks for the
 * first matching list element on the priority list or failing that
 * the overflow list of the portals table entry addressed by the message.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_get_match(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	pt_t *pt = buf->pt;
	ptl_ni_fail_t ni_fail;

	/* Synchronize with LE/ME append/search APIs */
	PTL_FASTLOCK_LOCK(&pt->lock);

	/* Check the priority list.
	 * If we find a match take a reference to protect
	 * the list element pointer.
	 * Note buf->le and buf->me are in a union */
	list_for_each_entry(buf->le, &pt->priority_list, list) {
		if (ni->options & PTL_NI_NO_MATCHING) {
			le_get(buf->le);
			goto found_one;
		}

		if (check_match(buf, buf->me)) {
			me_get(buf->me);
			goto found_one;
		}
	}

	/* Check the overflow list */
	list_for_each_entry(buf->le, &pt->overflow_list, list) {
		if (ni->options & PTL_NI_NO_MATCHING) {
			le_get(buf->le);
			goto found_one;
		}

		if (check_match(buf, buf->me)) {
			me_get(buf->me);
			goto found_one;
		}
	}

	/* Failed to match any elements */
	if (pt->options & PTL_PT_FLOWCTRL) {
		pt->state |= PT_AUTO_DISABLED;
		PTL_FASTLOCK_UNLOCK(&pt->lock);
		buf->ni_fail = PTL_NI_PT_DISABLED;
	} else {
		PTL_FASTLOCK_UNLOCK(&pt->lock);
		buf->ni_fail = PTL_NI_DROPPED;
	}
	buf->le = NULL;
	WARN();

	return STATE_TGT_DROP;

found_one:
	/* Check to see if we have permission for the operation */
	ni_fail = check_perm(buf, buf->le);
	if (ni_fail) {
		PTL_FASTLOCK_UNLOCK(&pt->lock);
		le_put(buf->le);
		buf->le = NULL;
		buf->ni_fail = ni_fail;
		return STATE_TGT_DROP;
	}

	if (buf->le->ptl_list == PTL_OVERFLOW_LIST) {
		/* take a reference to the buf for the
		 * unexpected list entry */
		buf_get(buf);

		/* Mark the buffer as busy. If something is trying to dequeue
		 * it (ie. through an append), then it will have to wait until
		 * the transfer is completed. */
		buf->unexpected_busy = 1;
		list_add_tail(&buf->unexpected_list,
					  &pt->unexpected_list);

#if WITH_TRANSPORT_SHMEM || IS_PPE
		/* If it is a shared memory buffer, then the data is actually
		 * in a different buffer. This second buffer will be used to
		 * send back an ack/reply, while the first buffer has been put
		 * on the unexpected list. So sever the connection between the
		 * two buffers right now to avoid races with MEAppend() and
		 * sending that ack. */
		if (buf->data != buf->internal_data) {
			memcpy(buf->internal_data, buf->data, buf->length);
			buf->data = buf->internal_data;
		}
#endif
	}

	buf->matching_list = buf->le->ptl_list;

	PTL_FASTLOCK_UNLOCK(&pt->lock);

	/* now that we have determined the list element
	 * compute the remaining event mask bits */
	init_events(buf);

	return STATE_TGT_GET_LENGTH;
}

/**
 * @brief target get length state.
 *
 * This state is reached after successfully finding a
 * list element that matches. It computes the actual length and offset
 * for the data transfer. These are based on whether the list element
 * is managed by the initiator or the target and the operation type.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_get_length(buf_t *buf)
{
	int err;
	const ni_t *ni = obj_to_ni(buf);
	me_t *me = buf->me;
	ptl_size_t offset;
	ptl_size_t length;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	uint64_t rlength = le64_to_cpu(hdr->rlength);
	uint64_t roffset = le64_to_cpu(hdr->roffset);

	/* note only MEs can have PTL_ME_MANAGE_LOCAL set */
	offset = (me->options & PTL_ME_MANAGE_LOCAL) ? me->offset : roffset;

	if (offset > me->length) {
		/* Messages that start outside the bounds of the ME are
		 * truncated to zero bytes. */
		length = 0;
	} else {
		ptl_size_t room = me->length - offset;
		length = (room >= rlength) ? rlength : room;
	}

	switch (buf->operation) {
	case OP_PUT:
		if (length > ni->limits.max_msg_size)
			length = ni->limits.max_msg_size;
		buf->put_resid = length;
		buf->get_resid = 0;
		break;

	case OP_GET:
		if (length > ni->limits.max_msg_size)
			length = ni->limits.max_msg_size;
		buf->put_resid = 0;
		buf->get_resid = length;
		break;

	case OP_ATOMIC:
		if (length > ni->limits.max_atomic_size)
			length = ni->limits.max_atomic_size;
		buf->put_resid = length;
		buf->get_resid = 0;
		break;

	case OP_FETCH:
		if (length > ni->limits.max_atomic_size)
			length = ni->limits.max_atomic_size;
		buf->put_resid = length;
		buf->get_resid = length;
		break;

	case OP_SWAP:
		if (hdr->atom_op == PTL_SWAP) {
			if (length > ni->limits.max_atomic_size)
				length = ni->limits.max_atomic_size;
		} else {
			if (length > atom_type_size[hdr->atom_type])
				length = atom_type_size[hdr->atom_type];
		}
		buf->put_resid = length;
		buf->get_resid = length;
		break;
	}

	buf->mlength = length;
	buf->moffset = offset;

	/*
	 * If locally managed update to reserve space for the
	 * associated RDMA data. Note the early states in the
	 * state machine only run on the progress thread so no
	 * other request message can run this code until we return.
	 */
	if (me->options & PTL_ME_MANAGE_LOCAL)
		me->offset += length;

	/*
	 * Unlink if required to prevent further use of this
	 * ME/LE.
	 */
	if ((me->options & PTL_ME_USE_ONCE) ||
	    ((me->options & PTL_ME_MANAGE_LOCAL) && me->min_free &&
	    ((me->length - me->offset) < me->min_free)))
		le_unlink(buf->le, !(me->options &
			  PTL_ME_EVENT_UNLINK_DISABLE));

	/* initialize buf->cur_loc_iov_index/off and buf->start */
	err = init_local_offset(buf);
	if (err)
		return STATE_TGT_ERROR;

	/* if we are already connected to the initiator skip wait_conn */
	if (likely(buf->conn->state >= CONN_STATE_CONNECTED))
		return STATE_TGT_DATA;

#if WITH_TRANSPORT_UDP
	//REG: UDP needs to connect earlier than this, so it should always be connected at this point
	return STATE_TGT_DATA;
#endif

	/* we need a connection if we are sending an ack/reply
	 * or doing an RDMA operation */
	if ((buf->event_mask & (XT_ACK_EVENT | XT_REPLY_EVENT)) ||
	    (buf->data_out || (buf->data_in && (buf->data_in->data_fmt
						!= DATA_FMT_IMMEDIATE))))
		return STATE_TGT_WAIT_CONN;

	return STATE_TGT_DATA;
}

/**
 * @brief target wait conn state.
 *
 * This state is reached after the get length state. If not connected
 * the buf exits the state machine until a connection is established
 * and reenters in this state.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_wait_conn(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	conn_t *conn = buf->conn;

	/* if we are connected to the initiator we're done here */
	if (conn->state >= CONN_STATE_CONNECTED)
		return STATE_TGT_DATA;

	/* if not connected. Add the buf to the pending list. It will be
	 * retried once connected/disconnected. */
	pthread_mutex_lock(&conn->mutex);
	if (conn->state < CONN_STATE_CONNECTED) {
		if (conn->state == CONN_STATE_DISCONNECTED) {
			/* Initiate connection. */
			if (conn->transport.init_connect(ni, conn)) {
				pthread_mutex_unlock(&conn->mutex);
				return STATE_TGT_ERROR;
			}
		}

#if WITH_TRANSPORT_IB || WITH_TRANSPORT_UDP
		pthread_cond_wait(&conn->move_wait, &conn->mutex);
#endif

		pthread_mutex_unlock(&conn->mutex);

		if (conn->state == CONN_STATE_CONNECTED)
			return STATE_TGT_DATA;

		WARN();
		return STATE_TGT_ERROR;
	}
	pthread_mutex_unlock(&conn->mutex);

	return STATE_TGT_DATA;
}

/**
 * @brief target data state.
 *
 * This state is reached after the get length state or wait conn.
 * It selects the first data phase.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_data(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);

	/* save the addressing information to the initiator
	 * in buf */
	if (buf->conn->state >= CONN_STATE_CONNECTED)
		set_buf_dest(buf, buf->conn);

	/* This implementation guarantees atomicity between
	 * the three atomic type operations by only alowing a
	 * single operation at a time to be processed. */

	// TODO we could improve performance by having more locks
	// here covering operations that do no overlap. Also
	// we could think some more about how to protect between
	// atomic and regular get/put operations.
	if (buf->operation == OP_ATOMIC ||
	    buf->operation == OP_SWAP ||
	    buf->operation == OP_FETCH) {
		pthread_mutex_lock(&ni->atomic_mutex);
		buf->in_atomic = 1;
	}

	/* process data out, then data in */
	if (buf->get_resid)
		return STATE_TGT_DATA_OUT;
	else if (buf->put_resid)
		return (buf->operation == OP_ATOMIC) ?
			STATE_TGT_ATOMIC_DATA_IN :
			STATE_TGT_DATA_IN;
	else {
		/* Dropping. */
#if WITH_TRANSPORT_SHMEM && !USE_KNEM
		if (buf->data_out && buf->data_out->data_fmt == DATA_FMT_NOKNEM)
			return STATE_TGT_DATA_OUT;
		else if (buf->data_in && buf->data_in->data_fmt == DATA_FMT_NOKNEM)
			return STATE_TGT_DATA_IN;
#endif
		return STATE_TGT_COMM_EVENT;
	}
}

/**
 * @brief target data out state.
 *
 * This state is reached after finding a match, computing length/offset and
 * establishing that there is a connection. It handles sending data
 * from get, fetch and swap operations to the initiators get_md depending
 * on the data descriptor and length of the data. Short data is
 * sent inline with the reply event information. Long data is sent using
 * RDMA write operations to the initiator and may require first copying
 * an indirect sge list from the initiators iovec is too long.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_data_out(buf_t *buf)
{
	int err;
	data_t *data = buf->data_out;
	struct ack_hdr *send_hdr = (ack_hdr_t *)buf->send_buf->data;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;

	if (!data)
		return STATE_TGT_ERROR;

	buf->rdma_dir = DATA_DIR_OUT;

	/* The initiator picks how we reply. */
	if (data->data_fmt == DATA_FMT_IMMEDIATE) {
		/* We assummed that both the initiator and the target have the
		 * same max inline data. */
		me_t *me = buf->me;

		assert(buf->mlength <= get_param(PTL_MAX_INLINE_DATA));

		send_hdr->h1.data_out = 1;

		if (me->num_iov) {
#if IS_PPE
			mr_t *mr;

			if (me->mr_start)
				mr = me->mr_start;
			else {
				err = mr_lookup_app(obj_to_ni(me), me->start, me->length*sizeof(ptl_iovec_t), &mr);
				if (err) {
					WARN();
					return err;
				}
			}

			err = append_immediate_data(addr_to_ppe(me->start, mr), me->mr_list,
										me->num_iov, DATA_DIR_OUT,
										buf->moffset, buf->mlength, buf->send_buf);

			if (!me->mr_start)
				mr_put(mr);
#else
			err = append_immediate_data(me->start, me->mr_list,
										me->num_iov, DATA_DIR_OUT,
										buf->moffset, buf->mlength, buf->send_buf);
#endif
		} else {
#if IS_PPE
			mr_t *mr;

			if (me->mr_start)
				mr = me->mr_start;
			else {
				err = mr_lookup_app(obj_to_ni(me), me->start+buf->moffset, buf->mlength, &mr);
				if (err) {
					WARN();
					return err;
				}
			}

			err = append_immediate_data(me->start, &mr, me->num_iov, DATA_DIR_OUT,
										buf->moffset, buf->mlength, buf->send_buf);

			if (!me->mr_start)
				mr_put(mr);
#else
			err = append_immediate_data(me->start, NULL, me->num_iov, DATA_DIR_OUT,
										buf->moffset, buf->mlength, buf->send_buf);
#endif
		}
		if (err)
			return STATE_TGT_ERROR;

		/* check to see if we still need data in phase */
		if (buf->put_resid) {
			if (buf->operation == OP_FETCH)
				return STATE_TGT_ATOMIC_DATA_IN;
			else if (buf->operation == OP_SWAP)
				return (hdr->atom_op == PTL_SWAP)
					? STATE_TGT_DATA_IN
					: STATE_TGT_SWAP_DATA_IN;
			else
				return  STATE_TGT_DATA_IN;
		}

		return STATE_TGT_COMM_EVENT;
	}

	/* all atomic or swap data should fit as immediate data so */
	assert(buf->in_atomic == 0);

	return buf->conn->transport.tgt_data_out(buf, data);
}

#if WITH_TRANSPORT_SHMEM && !USE_KNEM
static int tgt_start_copy(buf_t *buf)
{
	/* Add to the data queue. */
	ni_t *ni = obj_to_ni(buf);

	PTL_FASTLOCK_LOCK(&ni->shmem.noknem_lock);
	list_add_tail(&buf->list, &ni->shmem.noknem_list);
	PTL_FASTLOCK_UNLOCK(&ni->shmem.noknem_lock);

	return STATE_TGT_RDMA;
}
#else
static int tgt_start_copy(buf_t *buf) { abort(); }
#endif

/**
 * @brief target rdma state.
 *
 * This state can be reached from tgt_data_in or tgt_data_out. It
 * generates rdma commands to copy data between the md at the
 * initiator and the le/me at the target. It may require leaving
 * and reentering the state machine if there are not enough rdma
 * resources.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_rdma(buf_t *buf)
{
	int err;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	ptl_size_t *resid = buf->rdma_dir == DATA_DIR_IN ?
				&buf->put_resid : &buf->get_resid;
#if WITH_TRANSPORT_SHMEM && !USE_KNEM
	/* It is possible that post_tgt_dma() sets the target_done flag,
	 * and that the initiator replies with init_done before we reach
	 * the exit test. However we must leave the state machine so the
	 * receive state machine can remove the buffer from the
	 * noknem_list; this function will be called again, and this time
	 * was_done will be 1. May be this part needs a nicer design. */
	int was_done = buf->transfer.noknem.noknem ? buf->transfer.noknem.noknem->init_done : 0;
#endif

	/* post one or more RDMA operations */
	err = buf->conn->transport.post_tgt_dma(buf);
	if (err)
		return STATE_TGT_ERROR;

	/* if there is more work to do leave the state
	 * machine and have the completion of the rdma
	 * operation reenter this state to issue more
	 * operations. */
	if (*resid
#if WITH_TRANSPORT_IB
		|| atomic_read(&buf->rdma.rdma_comp)
#endif
#if WITH_TRANSPORT_SHMEM && !USE_KNEM
		|| (was_done == 0)
#endif
		)
		return STATE_TGT_RDMA;

	/* here we are done so, if we got one, free the
	 * indirect sge list */
	if (buf->indir_sge) {
		free(buf->indir_sge);
		buf->indir_sge = NULL;
	}

	/* check to see if there is another data phase */
	if (buf->put_resid) {
		/* re-initialize buf->cur_loc_iov_index/off */
		err = init_local_offset(buf);
		if (err)
			return STATE_TGT_ERROR;

		if (hdr->h1.operation == OP_FETCH)
			return STATE_TGT_ATOMIC_DATA_IN;
		else if (hdr->h1.operation == OP_SWAP)
			return (hdr->atom_op == PTL_SWAP) ?
				STATE_TGT_DATA_IN :
				STATE_TGT_SWAP_DATA_IN;
		else
			return  STATE_TGT_DATA_IN;
	}

	/* ok we are done transfering data */
	return STATE_TGT_COMM_EVENT;
}

#if WITH_TRANSPORT_UDP
/**
 * @brief target udp state.
 *
 * This state can be reached from tgt_data_in or tgt_data_out. It
 * generates udp commands to copy data between the md at the
 * initiator and the le/me at the target. It may require leaving
 * and reentering the state machine if there are not enough udp
 * resources. (UO & REB) - not sure!!!
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_udp(buf_t *buf)
{
	int err;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	ptl_size_t *resid = buf->rdma_dir == DATA_DIR_IN ?
				&buf->put_resid : &buf->get_resid;

	/* post one or more RDMA operations */
	err = buf->conn->transport.post_tgt_dma(buf);
	if (err)
		return STATE_TGT_ERROR;

	/* if there is more work to do leave the state
	 * machine and have the completion of the rdma
	 * operation reenter this state to issue more
	 * operations. */
	if (*resid
		|| (buf->transfer.udp.udp && buf->transfer.udp.udp->init_done == 0)
		)
		return STATE_TGT_UDP;

	/* check to see if there is another data phase */
	if (buf->put_resid) {
		/* re-initialize buf->cur_loc_iov_index/off */
		err = init_local_offset(buf);
		if (err)
			return STATE_TGT_ERROR;

		if (hdr->h1.operation == OP_FETCH)
			return STATE_TGT_ATOMIC_DATA_IN;
		else if (hdr->h1.operation == OP_SWAP)
			return (hdr->atom_op == PTL_SWAP) ?
				STATE_TGT_DATA_IN :
				STATE_TGT_SWAP_DATA_IN;
		else
			return  STATE_TGT_DATA_IN;
	}

	/* ok we are done transfering data */
	return STATE_TGT_COMM_EVENT;
}
#else
static int tgt_udp(buf_t *buf)
{
	/* This state can only be reached through UDP. */
	abort();
	return STATE_TGT_COMM_EVENT;
}
#endif // WITH_TRANSPORT_UDP


#if WITH_TRANSPORT_IB
/**
 * @brief Send rdma read for indirect scatter/gather list
 * and wait for response.
 *
 * We arrive in this state during RDMA data in or data out processing
 * if the number of remote data segments is larger than will fit
 * in the buf's data descriptor so that we need to copy an indirect
 * list from the initiator.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_wait_rdma_desc(buf_t *buf)
{
	int err;

	/* if this is the first time we get here
	 * rdma_desc_ok will be zero and we call
	 * process_rdma_desc() to post the rdma
	 * read for it. When the operation completes
	 * we will reenter here from recv with
	 * rdma_desc_ok = 1*/
	if (!buf->rdma_desc_ok) {
		err = process_rdma_desc(buf);
		if (err)
			return STATE_TGT_ERROR;
		else
			return STATE_TGT_WAIT_RDMA_DESC;
	} else {
		/* Was set in process_rdma_desc(). */
		buf->transfer.rdma.xxbuf = NULL;
	}

	/* setup the remote end of the dma state
	 * to point to the indirect scatter/gather list */
	if (buf->rdma_dir == DATA_DIR_IN) {
		buf->transfer.rdma.cur_rem_sge = buf->indir_sge;
		buf->transfer.rdma.num_rem_sge =
			(le32_to_cpu(buf->data_in->rdma.sge_list[0].length))
				/sizeof(struct ibv_sge);
		buf->transfer.rdma.cur_rem_off = 0;
	} else {
		buf->transfer.rdma.cur_rem_sge = buf->indir_sge;
		buf->transfer.rdma.num_rem_sge =
			(le32_to_cpu(buf->data_out->rdma.sge_list[0].length))
				/sizeof(struct ibv_sge);
		buf->transfer.rdma.cur_rem_off = 0;
	}

	return STATE_TGT_RDMA;
}
#else
static int tgt_wait_rdma_desc(buf_t *buf)
{
	/* This state can only be reached through IB. */
	abort();
	return STATE_TGT_RDMA;
}
#endif

#if (WITH_TRANSPORT_SHMEM && USE_KNEM)
/**
 * @brief target shared memory read long iovec descriptor state.
 *
 * This state is reached if the number of iovec entries is too large to
 * fit into a buf and we are using shared memory.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_shmem_desc(buf_t *buf)
{
	int next;
	data_t *data;
	size_t len;
	ni_t *ni = obj_to_ni(buf);
	void *indir_sge;
	mr_t *mr;

	data = buf->rdma_dir == DATA_DIR_IN ? buf->data_in : buf->data_out;
	len = data->mem.mem_iovec[0].length;

	/*
	 * Allocate and map indirect buffer and setup to read
	 * descriptor list from initiator memory.
	 */
	indir_sge = malloc(len);
	if (!indir_sge) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	if (mr_lookup_self(obj_to_ni(buf), indir_sge, len, &mr)) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	copy_mem_to_mem(ni, DATA_DIR_IN, &data->mem.mem_iovec[0], indir_sge, mr, len);

	buf->indir_sge = indir_sge;
	buf->mr_list[buf->num_mr++] = mr;
	buf->transfer.mem.cur_rem_iovec = indir_sge;
	buf->transfer.mem.cur_rem_off = 0;
	buf->transfer.mem.num_rem_iovecs = len/sizeof(struct mem_iovec);

	next = STATE_TGT_RDMA;
done:
	return next;
}
#else
static int tgt_shmem_desc(buf_t *buf)
{
	/* Invalid state in this configuration. */
	abort();
}
#endif

/**
 * @brief target data in state.
 *
 * This state handles the data in phase for a put or swap operation.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_data_in(buf_t *buf)
{
	int err;
	me_t *me = buf->me;
	data_t *data = buf->data_in;
	int next;

	buf->rdma_dir = DATA_DIR_IN;

	if (data->data_fmt == DATA_FMT_IMMEDIATE) {
		err = tgt_copy_in(buf, me, data->immediate.data);
		if (err)
			return STATE_TGT_ERROR;

		next = STATE_TGT_COMM_EVENT;
	} else {
		next = buf->conn->transport.tgt_data_out(buf, data);
	}

	/* this can happen for a simple swap operation */
	if (buf->in_atomic) {
		ni_t *ni = obj_to_ni(buf);

		pthread_mutex_unlock(&ni->atomic_mutex);
		buf->in_atomic = 0;
	}

	return next;
}

/**
 * @brief target atomic data in state.
 *
 * This phase handles the data in phase for an atomic or fetch operation.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_atomic_data_in(buf_t *buf)
{
	int err;
	data_t *data = buf->data_in;
	me_t *me = buf->me;
	ni_t *ni;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;

	/* assumes that max_atomic_size is <= PTL_MAX_INLINE_DATA */
	if (data->data_fmt != DATA_FMT_IMMEDIATE) {
		WARN();
		return STATE_TGT_ERROR;
	}

	// TODO should we return an ni fail??
	if (hdr->atom_op > PTL_BXOR || hdr->atom_type >= PTL_DATATYPE_LAST) {
		WARN();
		return STATE_TGT_ERROR;
	}

	err = atomic_in(buf, me, data->immediate.data);
	if (err)
		return STATE_TGT_ERROR;

	assert(buf->in_atomic);

	ni = obj_to_ni(buf);
	pthread_mutex_unlock(&ni->atomic_mutex);
	buf->in_atomic = 0;

	return STATE_TGT_COMM_EVENT;
}

/**
 * @brief target swap data in state.
 *
 * Handle swap operation for all cases where
 * the length is limited to a single data item.
 * (PTL_SWAP allows length up to max atomic size
 * but is handled as a get and a put combined.)
 *
 * This is a bit complicated because the LE/ME may have
 * its data stored in an iovec with arbitrary
 * byte boundaries. Since the length is small it is
 * simpler to just copy the data out of the iovec,
 * perform the swap operation and then copy the result
 * back into the me for that case.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_swap_data_in(buf_t *buf)
{
	int err;
	me_t *me = buf->me; /* can be LE or ME */
	data_t *data = buf->data_in;
	uint8_t copy[sizeof(datatype_t)];
	void *dst;
	ni_t *ni;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	void *operand = buf->data + sizeof(req_hdr_t);
     void *source = data->immediate.data;
     datatype_t tmp;
#if IS_PPE
	mr_t *mr;
#endif

	assert(data->data_fmt == DATA_FMT_IMMEDIATE);

	if (unlikely(me->num_iov)) {
		err = iov_copy_out(copy, (ptl_iovec_t *)me->start, NULL,
						   me->num_iov, buf->moffset, buf->mlength);
		if (err)
			return STATE_TGT_ERROR;

		dst = copy;
	} else {
		void *start = me->start + buf->moffset;
#if IS_PPE
		if (me->mr_start)
			mr = me->mr_start;
		else {
			err = mr_lookup_app(obj_to_ni(me), start, atom_type_size[hdr->atom_type], &mr);
			if (err) {
				WARN();
				return err;
			}
		}
#endif
		dst = addr_to_ppe(start, mr);
	}

     /* immediate data might not be aligned.  Make sure it's aligned */
     if (0 != ((uintptr_t) source & (atom_type_size[hdr->atom_type] - 1))) {
          memcpy(&tmp, source, atom_type_size[hdr->atom_type]);
          source = &tmp;
     }
	err = swap_data_in(hdr->atom_op, hdr->atom_type, dst,
			         source, operand);

#if IS_PPE
	if (!me->mr_start)
		mr_put(mr);
#endif

	if (err)
		return STATE_TGT_ERROR;

	if (unlikely(me->num_iov)) {
		err = tgt_copy_in(buf, buf->me, copy);
		if (err)
			return STATE_TGT_ERROR;
	}

	assert(buf->in_atomic);

	ni = obj_to_ni(buf);
	pthread_mutex_unlock(&ni->atomic_mutex);
	buf->in_atomic = 0;

	return STATE_TGT_COMM_EVENT;
}

/**
 * @brief target comm event state.
 *
 * This state is reached when we are ready to deliver a conn event to
 * the target side event queue or counting event.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_comm_event(buf_t *buf)
{
	if (buf->le &&
		buf->le->ptl_list == PTL_OVERFLOW_LIST) {

		/* The buf should be on the unexpected list, unless an
		 * append/search operation removed it since the buffer was in
		 * the check_match state. Tell the potential waiter the buffer
		 * is now ready. */
		buf->unexpected_busy = 0;
		pthread_cond_signal(&buf->cond);
	}

	if (buf->event_mask & XT_COMM_EVENT)
		make_comm_event(buf);

	if (buf->event_mask & XT_CT_COMM_EVENT)
		make_ct_comm_event(buf);

	if (buf->event_mask & XT_REPLY_EVENT)
		return STATE_TGT_SEND_REPLY;

	if (buf->event_mask & XT_ACK_EVENT)
		return STATE_TGT_SEND_ACK;

	return STATE_TGT_CLEANUP;
}

/**
 * @brief target send ack state.
 *
 * This state is reached when we are ready to deliver an ack
 * (or non-ack) to the initiator.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_send_ack(buf_t *buf)
{
	int err;
	buf_t *ack_buf;
	ack_hdr_t *ack_hdr;
 	const int ack_req = ((req_hdr_t *)(buf->data))->ack_req;

	/* Find a buffer to send the ack. Depending on the transport we
	 * may or may not be able to reuse the buffer in which we got the
	 * request. */
	if (buf->send_buf) {
		ack_buf = buf->send_buf;
		ack_hdr = (ack_hdr_t *)ack_buf->data;

		ack_hdr->h1.data_in = 0;
		ack_hdr->h1.data_out = 0;	/* can get reset to one for short replies */
		ack_hdr->h1.pkt_fmt = PKT_FMT_REPLY;

	}
#if WITH_TRANSPORT_SHMEM || IS_PPE
	else if (buf->mem_buf) {
		ack_buf = buf->mem_buf;
		ack_hdr = (ack_hdr_t *)ack_buf->internal_data;
	}
#endif
	else {
		/* Reusing received buffer. */
		ack_buf = buf;
		ack_hdr = (ack_hdr_t *)buf->data;
	}

	/* Reset some header values while keeping others. */
	ack_buf->length = sizeof(*ack_hdr);

	ack_hdr->h1.ni_fail = buf->ni_fail;
	ack_hdr->mlength	= cpu_to_le64(buf->mlength);
	ack_hdr->moffset	= cpu_to_le64(buf->moffset);
	ack_hdr->h1.matching_list = buf->matching_list;

	switch (ack_req) {
	case PTL_ACK_REQ:
		ack_hdr->h1.operation = OP_ACK;
		break;
	case PTL_CT_ACK_REQ:
		ack_buf->length -= sizeof(ack_hdr->moffset); /* don't need offset */
		ack_hdr->h1.operation = OP_CT_ACK;
		break;
	case PTL_OC_ACK_REQ:
		ack_buf->length -= (sizeof(ack_hdr->moffset) +
							sizeof(ack_hdr->mlength)); /* don't need offset nor length */
		ack_hdr->h1.operation = OP_OC_ACK;
		break;
	default:
		WARN();
		return STATE_TGT_ERROR;
	}

	/* Initiator is still waiting for an ACK to unblock its buf. */
	if (buf->le && buf->le->options & PTL_LE_ACK_DISABLE) {
		ack_buf->length = sizeof(ack_hdr_t) - sizeof(ack_hdr->moffset) -
			sizeof(ack_hdr->mlength); /* don't need offset nor length */
		ack_hdr->h1.operation = OP_NO_ACK;
	}

	if (buf->le && buf->le->ptl_list == PTL_PRIORITY_LIST) {
		/* The LE must be released before we sent the ack. */
		le_put(buf->le);
		buf->le = NULL;
	}

 	if (buf->send_buf) {
		ack_buf->dest = buf->dest;
		ack_buf->conn = buf->conn;

		/* Inline the data if it fits. That may save waiting for a
		 * completion. */
		ack_buf->conn->transport.set_send_flags(ack_buf, 1);

		err = ack_buf->conn->transport.send_message(ack_buf, 0);
		if (err) {
			WARN();
			return STATE_TGT_ERROR;
		}
	} else {
		switch(buf->conn->transport.type) {
#if WITH_TRANSPORT_SHMEM
		case CONN_TYPE_SHMEM:
			/* The same buffer is used to send the data back. Let the
			 * progress thread return it. */
			assert(buf->mem_buf);
			buf->mem_buf->type = BUF_SHMEM_SEND;
			break;
#endif

#if IS_PPE
		case CONN_TYPE_MEM:
			buf->mem_buf->type = BUF_MEM_SEND;
			break;
#endif

#if WITH_TRANSPORT_UDP
		case CONN_TYPE_UDP:
			ack_buf->dest.udp.dest_addr = buf->udp.src_addr;
			//ack_buf->dest = buf->dest;
			ack_buf->conn = buf->conn;
			ptl_info("buffer handle for initiator: %i \n",ack_hdr->h1.handle);

			err = ack_buf->conn->transport.send_message(ack_buf, 0);
			if (err) {
				WARN();
				return STATE_TGT_ERROR;
			}
			break;
#endif

#if WITH_TRANSPORT_IB
		case CONN_TYPE_RDMA:
			/* That should not be possible. */
			abort();
			break;
#endif

		default:
			/* Unreachable. */
			abort();
		}
  	}

	return STATE_TGT_CLEANUP;
}

/**
 * @brief target send reply state.
 *
 * This state is reached when we are ready to send a reply to the
 * initiator.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_send_reply(buf_t *buf)
{
	int err;
	buf_t *rep_buf;
	ack_hdr_t *rep_hdr;

	rep_buf = buf->send_buf;
	rep_hdr = (ack_hdr_t *)rep_buf->data;

	rep_hdr->h1.ni_fail = buf->ni_fail;
	rep_hdr->mlength	= cpu_to_le64(buf->mlength);
	rep_hdr->moffset	= cpu_to_le64(buf->moffset);
	rep_hdr->h1.operation = OP_REPLY;
	rep_hdr->h1.matching_list = buf->matching_list;

	if (buf->le && buf->le->ptl_list == PTL_PRIORITY_LIST) {
		/* The LE must be released before we sent the ack. */
		le_put(buf->le);
		buf->le = NULL;
	}

	rep_buf->dest = buf->dest;
#if WITH_TRANSPORT_UDP
	rep_buf->dest.udp.dest_addr = buf->udp.src_addr;
#endif
	rep_buf->conn = buf->conn;

	/* Inline the data if it fits. That may save waiting for a
	 * completion. */
	rep_buf->conn->transport.set_send_flags(rep_buf, 1);

	err = rep_buf->conn->transport.send_message(rep_buf, 0);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	return STATE_TGT_CLEANUP;
}

/**
 * @brief target cleanup state.
 *
 * This state is reached after delivering comm and ack/reply events.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_cleanup(buf_t *buf)
{
	int state;
	pt_t *pt;

	if (buf->matching.le) {
		/* On the overflow list, and was already matched by an
		 * ME/LE. */
		assert(buf->le->ptl_list == PTL_OVERFLOW_LIST);
		state = STATE_TGT_OVERFLOW_EVENT;
	} else if (buf->le && buf->le->ptl_list == PTL_OVERFLOW_LIST) {
		state = STATE_TGT_WAIT_APPEND;
	}	else
		state = STATE_TGT_CLEANUP_2;

	assert(!buf->indir_sge);
#if WITH_TRANSPORT_IB
	assert(list_empty(&buf->transfer.rdma.rdma_list));
#endif

	if (buf->send_buf) {
		buf_put(buf->send_buf);
		buf->send_buf = NULL;
	}

	pt = buf->pt;
	if (pt) {
		PTL_FASTLOCK_LOCK(&pt->lock);
		pt->num_tgt_active--;
		if ((pt->state & PT_AUTO_DISABLED) && !pt->num_tgt_active) {
			pt->state = PT_DISABLED;
			PTL_FASTLOCK_UNLOCK(&pt->lock);

			// TODO: don't send if PTL_LE_EVENT_FLOWCTRL_DISABLE ?
			make_target_event(buf, pt->eq,
							  PTL_EVENT_PT_DISABLED,
							  buf->matching.le ? buf->matching.le->user_ptr
							  : NULL, NULL);
		} else
			PTL_FASTLOCK_UNLOCK(&pt->lock);
	}

	return state;
}

/**
 * @brief target cleanup_2 state.
 *
 * This state is reached after handling list apppend processing
 * (if necessary) and cleans up the matching list element.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static void tgt_cleanup_2(buf_t *buf)
{
	/* tgt must release reference to any LE/ME */
	if (buf->le) {
		le_put(buf->le);
		buf->le = NULL;
	}

	if (buf->conn) {
		conn_put(buf->conn);
		buf->conn = NULL;
	}
}

/**
 * @brief target wait append state.
 *
 * This state is reached for a message that matches in the overflow
 * list and is waiting for an append to the priority list or a search
 * operation to occur.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
/* The XT is on the overflow list and waiting for a ME/LE search/append. */
static int tgt_wait_append(buf_t *buf)
{
	int state;

	if (buf->matching.le)
		state = STATE_TGT_OVERFLOW_EVENT;
	else
		state = STATE_TGT_WAIT_APPEND;

	return state;
}

/**
 * @brief target overflow event state.
 *
 * This state is reached when a matching append or search occurs.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return The next state.
 */
static int tgt_overflow_event(buf_t *buf)
{
	le_t *le = buf->matching.le;

	assert(le);

	if (!(le->options & PTL_LE_EVENT_OVER_DISABLE)) {
		switch (buf->operation) {
		case OP_PUT:
			make_target_event(buf, buf->pt->eq,
					  PTL_EVENT_PUT_OVERFLOW,
					  le->user_ptr, buf->start);
			break;

		case OP_ATOMIC:
			make_target_event(buf, buf->pt->eq,
					  PTL_EVENT_ATOMIC_OVERFLOW,
					  le->user_ptr, buf->start);
			break;

		case OP_FETCH:
		case OP_SWAP:
			make_target_event(buf, buf->pt->eq,
					  PTL_EVENT_FETCH_ATOMIC_OVERFLOW,
					  le->user_ptr, buf->start);
			break;

		case OP_GET:
			make_target_event(buf, buf->pt->eq,
					  PTL_EVENT_GET_OVERFLOW,
					  le->user_ptr, buf->start);
			break;
		}

		/* Update the counter if we can. If LE comes from PtlLESearch,
		 * then ct is NULL. */
		if ((le->options & PTL_LE_EVENT_CT_OVERFLOW) && le->ct)
			make_ct_event(le->ct, buf, CT_MBYTES);
	}

	/* drop the matching list element */
	le_put(le);
	buf->matching.le = NULL;

	return STATE_TGT_CLEANUP_2;
}

/**
 * @brief target state machine.
 *
 * This routine implements the target side state machine. It can
 * be called from ptl_recv when a portals request message is received
 * from an initiator or from the completion of an RDMA operations or
 * from the connection event thread.
 *
 * @param[in] buf The message buf received by the target.
 *
 * @return status
 */
int process_tgt(buf_t *buf)
{
	int err = PTL_OK;
	enum tgt_state state;
	
#if WITH_TRANSPORT_UDP
	ptl_info("locking buffer for target processing \n");	
#endif

	pthread_mutex_lock(&buf->mutex);

#if WITH_TRANSPORT_UDP
	ptl_info("got lock for target buffer processing \n");
#endif

	state = buf->tgt_state;

	while(1) {
		ptl_info("%p: tgt state = %s event mask: %i\n",
				 buf, tgt_state_name[state],buf->event_mask);

		switch (state) {
		case STATE_TGT_START:
			state = tgt_start(buf);
			break;
		case STATE_TGT_GET_MATCH:
			state = tgt_get_match(buf);
			break;
		case STATE_TGT_GET_LENGTH:
			state = tgt_get_length(buf);
			break;
		case STATE_TGT_WAIT_CONN:
			state = tgt_wait_conn(buf);
			if (state == STATE_TGT_WAIT_CONN)
				goto exit;
			break;
		case STATE_TGT_DATA:
			state = tgt_data(buf);
			break;
		case STATE_TGT_DATA_IN:
			state = tgt_data_in(buf);
			break;
		case STATE_TGT_WAIT_RDMA_DESC:
			state = tgt_wait_rdma_desc(buf);
			if (state == STATE_TGT_WAIT_RDMA_DESC)
				goto exit;
			break;
		case STATE_TGT_SHMEM_DESC:
			state = tgt_shmem_desc(buf);
			break;
		case STATE_TGT_START_COPY:
			state = tgt_start_copy(buf);
			goto exit;
			break;
		case STATE_TGT_RDMA:
			state = tgt_rdma(buf);
			if (state == STATE_TGT_RDMA)
				goto exit;
			break;
		case STATE_TGT_UDP:
			state = tgt_udp(buf);
			if (state == STATE_TGT_UDP)
				 goto exit;
			break;
		case STATE_TGT_ATOMIC_DATA_IN:
			state = tgt_atomic_data_in(buf);
			break;
		case STATE_TGT_SWAP_DATA_IN:
			state = tgt_swap_data_in(buf);
			break;
		case STATE_TGT_DATA_OUT:
			state = tgt_data_out(buf);
			break;
		case STATE_TGT_COMM_EVENT:
			state = tgt_comm_event(buf);
			break;
		case STATE_TGT_SEND_ACK:
			state = tgt_send_ack(buf);
			break;
		case STATE_TGT_SEND_REPLY:
			state = tgt_send_reply(buf);
			break;
		case STATE_TGT_DROP:
			WARN();
			state = request_drop(buf);
			break;
		case STATE_TGT_WAIT_APPEND:
			state = tgt_wait_append(buf);
			if (state == STATE_TGT_WAIT_APPEND)
				goto exit;
			break;
		case STATE_TGT_OVERFLOW_EVENT:
			state = tgt_overflow_event(buf);
			break;
		case STATE_TGT_ERROR:
			if (buf->in_atomic) {
				ni_t *ni = obj_to_ni(buf);
				pthread_mutex_unlock(&ni->atomic_mutex);
				buf->in_atomic = 0;
			}
			err = PTL_FAIL;
			state = STATE_TGT_CLEANUP;
			break;
		case STATE_TGT_CLEANUP:
			state = tgt_cleanup(buf);
			break;
		case STATE_TGT_CLEANUP_2:
			tgt_cleanup_2(buf);
			buf->tgt_state = STATE_TGT_DONE;
			pthread_mutex_unlock(&buf->mutex);
#if !WITH_TRANSPORT_UDP
			buf_put(buf);		/* match buf_alloc */
#endif
			return err;
		case STATE_TGT_DONE:
			/* buf isn't valid anymore. */
			goto done;
		}
	}

exit:
	buf->tgt_state = state;
done:
	pthread_mutex_unlock(&buf->mutex);
	return err;
}
