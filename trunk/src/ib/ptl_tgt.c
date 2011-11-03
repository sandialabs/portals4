#include "ptl_loc.h"

/*
 * tgt_state_name
 *	for debugging output
 */
static char *tgt_state_name[] = {
	[STATE_TGT_START]		= "tgt_start",
	[STATE_TGT_DROP]		= "tgt_drop",
	[STATE_TGT_GET_MATCH]		= "tgt_get_match",
	[STATE_TGT_GET_LENGTH]		= "tgt_get_length",
	[STATE_TGT_WAIT_CONN]		= "tgt_wait_conn",
	[STATE_TGT_DATA_IN]		= "tgt_data_in",
	[STATE_TGT_RDMA]		= "tgt_rdma",
	[STATE_TGT_ATOMIC_DATA_IN]	= "tgt_atomic_data_in",
	[STATE_TGT_SWAP_DATA_IN]	= "tgt_swap_data_in",
	[STATE_TGT_DATA_OUT]		= "tgt_data_out",
	[STATE_TGT_RDMA_DESC]		= "tgt_rdma_desc",
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

/*
 * make_comm_event
 */
static int make_comm_event(buf_t *buf)
{
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	unsigned operation = hdr->operation;

	if (buf->ni_fail || !(buf->le->options
				& PTL_LE_EVENT_SUCCESS_DISABLE)) {

		ptl_event_kind_t type;

		if (operation == OP_PUT)
			type = PTL_EVENT_PUT;
		else if (operation == OP_GET)
			type = PTL_EVENT_GET;
		else if (operation == OP_ATOMIC ||
			 operation == OP_FETCH ||
			 operation == OP_SWAP) {
			type = PTL_EVENT_ATOMIC;
		} else {
			WARN();
			return STATE_TGT_ERROR;
		}

		make_target_event(buf, buf->pt->eq, type,
				  buf->le->user_ptr,
				  buf->le->start+buf->moffset);
	}

	buf->event_mask &= ~XT_COMM_EVENT;

	return PTL_OK;
}

/*
 * make_ct_comm_event
 */
static void make_ct_comm_event(buf_t *buf)
{
	int bytes = (buf->le->options & PTL_LE_EVENT_CT_BYTES) ?
			CT_MBYTES : CT_EVENTS;

	make_ct_event(buf->le->ct, buf, bytes);

	buf->event_mask &= ~XT_CT_COMM_EVENT;
}

/*
 * init_events
 *	decide whether comm eq/ct events will happen
 *	for this message
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

/*
 * tgt_copy_in
 *	copy data from data segment into le/me
 */
static int tgt_copy_in(buf_t *buf, me_t *me, void *data)
{
	int err;
	ptl_size_t offset = buf->moffset;
	ptl_size_t length = buf->mlength;

	if (me->num_iov) {
		void *dst_start;
		err = iov_copy_in(data, (ptl_iovec_t *)me->start,
				  me->num_iov, offset, length, &dst_start);
		if (err)
			return STATE_TGT_ERROR;

		buf->start = dst_start;
	} else {
		buf->start = me->start + offset;
		memcpy(buf->start, data, length);
	}

	return PTL_OK;
}

/*
 * atomic_in
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

	/* this implementation assumes that the architecture can support
	 * misaligned arithmetic operations. This is OK for x86
	 * and x86 variants and generally most modern architectures */
	if (me->num_iov) {
		err = iov_atomic_in(op, atom_type_size[hdr->atom_type],
				    data, (ptl_iovec_t *)me->start,
				    me->num_iov, offset, length);
		if (err) {
			WARN();
			return STATE_TGT_ERROR;
		}
	} else {
		(*op)(me->start + offset, data, length);
	}

	return PTL_OK;
}


/*
 * copy_out
 *	copy data to data segment from le/me
 */
static int copy_out(buf_t *buf, me_t *me, void *data)
{
	int err;
	ptl_size_t offset = buf->moffset;
	ptl_size_t length = buf->mlength;

	if (me->num_iov) {
		err = iov_copy_out(data, (ptl_iovec_t *)me->start,
				me->num_iov, offset, length);
		if (err) {
			WARN();
			return STATE_TGT_ERROR;
		}
	} else
		memcpy(data, me->start + offset, length);

	return PTL_OK;
}

/* Allocate a send buffer to store the ack or the reply. */
static int prepare_send_buf(buf_t *buf)
{
	buf_t *send_buf;
	int err;
	hdr_t *send_hdr;
	ni_t *ni = obj_to_ni(buf);

	if (buf->conn->transport.type == CONN_TYPE_RDMA)
		err = buf_alloc(ni, &send_buf);
	else
		err = sbuf_alloc(ni, &send_buf);
	if (err) {
		WARN();
		return PTL_FAIL;
	}

	send_buf->xxbuf = buf;
	buf_get(buf);
	buf->send_buf = send_buf;

	send_hdr = (hdr_t *)send_buf->data;

	memset(send_hdr, 0, sizeof(*send_hdr));
	send_buf->length = sizeof(*send_hdr);

	send_hdr->data_in = 0;
	send_hdr->data_out = 0;	/* can get reset to one for short replies */

	return PTL_OK;
}

/*
 * tgt_start
 *	get portals table entry from request
 */
static int tgt_start(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	uint32_t pt_index = le32_to_cpu(hdr->pt_index);
	ptl_process_t initiator;

	buf->event_mask = 0;

	switch (hdr->operation) {
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
	}

	INIT_LIST_HEAD(&buf->unexpected_list);
	INIT_LIST_HEAD(&buf->rdma_list);
	buf->matching.le = NULL;

	/* get per conn info */
	initiator.phys.nid = le32_to_cpu(hdr->src_nid);
	initiator.phys.pid = le32_to_cpu(hdr->src_pid);

	buf->conn = get_conn(ni, initiator);
	if (unlikely(!buf->conn)) {
		WARN();
		return STATE_TGT_ERROR;
	}

	/* Allocate the ack/reply buffer */
	if ((buf->event_mask & (XT_ACK_EVENT | XT_REPLY_EVENT)) &&
		(prepare_send_buf(buf) != PTL_OK))
		return STATE_TGT_ERROR;

	if (pt_index >= ni->limits.max_pt_index) {
		WARN();
		buf->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}

	buf->pt = &ni->pt[pt_index];
	if (!buf->pt->in_use) {
		WARN();
		buf->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}

	/* Serialize between progress and API */
	pthread_spin_lock(&buf->pt->lock);
	if (!buf->pt->enabled || buf->pt->disable) {
		pthread_spin_unlock(&buf->pt->lock);
		buf->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}
	buf->pt->num_tgt_active++;
	pthread_spin_unlock(&buf->pt->lock);

	return STATE_TGT_GET_MATCH;
}

/*
 * request_drop
 *	drop a request
 */
static int request_drop(buf_t *buf)
{
	/* logging ? */

	return STATE_TGT_WAIT_CONN;
}

/*
 * check_match
 *	determine if ME matches XT request info.
 */
int check_match(buf_t *buf, const me_t *me)
{
	const ni_t *ni = obj_to_ni(buf);
	ptl_size_t offset;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	ptl_size_t length = le64_to_cpu(hdr->length);
	uint64_t roffset = le64_to_cpu(hdr->offset);
	ptl_process_t initiator;

	initiator.phys.nid = le32_to_cpu(hdr->src_nid);
	initiator.phys.pid = le32_to_cpu(hdr->src_pid);

	if (ni->options & PTL_NI_LOGICAL) {
		if (!(me->id.rank == PTL_RANK_ANY ||
		     (me->id.rank == initiator.rank)))
			return 0;
	} else {
		if (!(me->id.phys.nid == PTL_NID_ANY ||
		     (me->id.phys.nid == initiator.phys.nid)))
			return 0;
		if (!(me->id.phys.pid == PTL_PID_ANY ||
		     (me->id.phys.pid == initiator.phys.pid)))
			return 0;
	}

	offset = (me->options & PTL_ME_MANAGE_LOCAL) ?
			me->offset : roffset;

	if ((me->options & PTL_ME_NO_TRUNCATE) &&
	    ((offset + length) > me->length))
			return 0;

	return (le64_to_cpu(hdr->match_bits) | me->ignore_bits) ==
		(me->match_bits | me->ignore_bits);
}

/*
 * check_perm
 *	check permission on incoming request packet against ME/LE
 * Returns 0 for permission granted, else an error code
 */
int check_perm(buf_t *buf, const le_t *le)
{
	int ret = PTL_NI_OK;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	uint32_t uid = le32_to_cpu(hdr->uid);

	if (!(le->uid == PTL_UID_ANY || (le->uid == uid))) {
		WARN();
		ret = PTL_NI_PERM_VIOLATION;
	} else {
		switch (hdr->operation) {
		case OP_ATOMIC:
		case OP_PUT:
			if (!(le->options & PTL_ME_OP_PUT)) {
				ret = PTL_NI_OP_VIOLATION;
			}
			break;

		case OP_GET:
			if (!(le->options & PTL_ME_OP_GET)) {
				ret = PTL_NI_OP_VIOLATION;
			}
			break;

		case OP_FETCH:
		case OP_SWAP:
			if ((le->options & (PTL_ME_OP_PUT | PTL_ME_OP_GET))
				!= (PTL_ME_OP_PUT | PTL_ME_OP_GET)) {
				ret = PTL_NI_OP_VIOLATION;
			}
			break;

		default:
			ret = PTL_NI_OP_VIOLATION;
			break;
		}
	}

	return ret;
}

/*
 * tgt_get_match
 *	get matching entry from PT
 */
static int tgt_get_match(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	struct list_head *l;
	int perm_ret;
	pt_t *pt = buf->pt;

	/* have to protect against a race with le/me append/search
	 * which change the pt lists */
	pthread_spin_lock(&pt->lock);

	if (pt->options & PTL_PT_FLOWCTRL) {
		if (list_empty(&pt->priority_list) &&
		    list_empty(&pt->overflow_list)) {
			WARN();
			pt->disable |= PT_AUTO_DISABLE;
			pthread_spin_unlock(&pt->lock);
			buf->ni_fail = PTL_NI_FLOW_CTRL;
			buf->le = NULL;
			return STATE_TGT_DROP;
		}
	}

	list_for_each(l, &pt->priority_list) {
		buf->le = list_entry(l, le_t, list);
		if (ni->options & PTL_NI_NO_MATCHING) {
			le_get(buf->le);
			goto done;
		}

		if (check_match(buf, buf->me)) {
			me_get(buf->me);
			goto done;
		}
	}

	list_for_each(l, &pt->overflow_list) {
		buf->le = list_entry(l, le_t, list);
		if (ni->options & PTL_NI_NO_MATCHING) {
			le_get(buf->le);
			goto done;
		}

		if (check_match(buf, buf->me)) {
			me_get(buf->me);
			goto done;
		}
	}

	pthread_spin_unlock(&pt->lock);
	WARN();
	buf->le = NULL;
	buf->ni_fail = PTL_NI_DROPPED;
	return STATE_TGT_DROP;

done:
	if ((perm_ret = check_perm(buf, buf->le))) {
		pthread_spin_unlock(&pt->lock);
		le_put(buf->le);
		buf->le = NULL;

		buf->ni_fail = perm_ret;
		return STATE_TGT_DROP;
	}

	if (buf->le->ptl_list == PTL_OVERFLOW_LIST) {
		buf_get(buf);
		list_add_tail(&buf->unexpected_list,
			      &buf->le->pt->unexpected_list);
	}

	pthread_spin_unlock(&pt->lock);
	return STATE_TGT_GET_LENGTH;
}

/*
 * tgt_get_length
 *	determine the data in/out transfer lengths
 */
static int tgt_get_length(buf_t *buf)
{
	const ni_t *ni = obj_to_ni(buf);
	me_t *me = buf->me;
	ptl_size_t offset;
	ptl_size_t length;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	uint64_t rlength = le64_to_cpu(hdr->length);
	uint64_t roffset = le64_to_cpu(hdr->offset);

	/* note le->options & PTL_ME_MANAGE_LOCAL is always zero */
	offset = (me->options & PTL_ME_MANAGE_LOCAL) ? me->offset : roffset;

	if (offset > me->length) {
		/* Messages that are outside the bounds of the ME are
		 * truncated to zero bytes. */
		length = 0;
	} else {
		ptl_size_t room;

		room = me->length - offset;
		length = (room >= rlength) ? rlength : room;
	}

	switch (hdr->operation) {
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

	init_events(buf);

	/*
	 * If locally managed update to reserve space for the
	 * associated RDMA data.
	 */
	if (me->options & PTL_ME_MANAGE_LOCAL)
		me->offset += length;

	/*
	 * Unlink if required to prevent further use of this
	 * ME/LE.
	 */
	if ((me->options & PTL_ME_USE_ONCE) ||
		((me->options & PTL_ME_MANAGE_LOCAL) && me->min_free &&
		 ((me->length - me->offset) < me->min_free))) {
		le_unlink(buf->le, !(me->options & PTL_ME_EVENT_UNLINK_DISABLE));
	}

	return STATE_TGT_WAIT_CONN;
}

/*
 * tgt_wait_conn
 *	check whether we need a connection to init
 *	and if so wait until we are connected
 */
static int tgt_wait_conn(buf_t *buf)
{
	ni_t *ni = obj_to_ni(buf);
	conn_t *conn = buf->conn;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	unsigned operation = hdr->operation;

	/* we need a connection if we are sending an ack/reply
	 * or doing an RDMA operation */
	if (!(buf->event_mask & (XT_ACK_EVENT | XT_REPLY_EVENT)) &&
	    !(buf->data_out || (buf->data_in && (buf->data_in->data_fmt
						!= DATA_FMT_IMMEDIATE))))
		goto out1;

	if (conn->state >= CONN_STATE_CONNECTED)
		goto out2;

	/* if not connected. Add the buf to the pending list. It will be
	 * retried once connected/disconnected. */
	pthread_mutex_lock(&conn->mutex);
	if (conn->state < CONN_STATE_CONNECTED) {
		pthread_spin_lock(&conn->wait_list_lock);
		list_add_tail(&buf->list, &conn->buf_list);
		pthread_spin_unlock(&conn->wait_list_lock);

		if (conn->state == CONN_STATE_DISCONNECTED) {
			/* Initiate connection. */
			if (init_connect(ni, conn)) {
				pthread_mutex_unlock(&conn->mutex);
				pthread_spin_lock(&conn->wait_list_lock);
				list_del(&buf->list);
				pthread_spin_unlock(&conn->wait_list_lock);
				return STATE_TGT_ERROR;
			}
		}

		pthread_mutex_unlock(&conn->mutex);
		return STATE_TGT_WAIT_CONN;
	}
	pthread_mutex_unlock(&conn->mutex);

out2:
#ifdef USE_XRC
	if (conn->state == CONN_STATE_XRC_CONNECTED)
		set_tgt_dest(buf, conn->main_connect);
	else
#endif
		set_tgt_dest(buf, conn);

out1:
	if (operation == OP_ATOMIC ||
		operation == OP_SWAP ||
		operation == OP_FETCH) {
		pthread_mutex_lock(&ni->atomic_mutex);
		buf->in_atomic = 1;
	} else {
		buf->in_atomic = 0;
	}

	if (buf->get_resid)
		return STATE_TGT_DATA_OUT;

	if (buf->put_resid)
		return (operation == OP_ATOMIC) ? STATE_TGT_ATOMIC_DATA_IN
						    : STATE_TGT_DATA_IN;

	return STATE_TGT_COMM_EVENT;
}

buf_t *tgt_alloc_rdma_buf(buf_t *buf)
{
	buf_t *rdma_buf;
	int err;

	err = buf_alloc(obj_to_ni(buf), &rdma_buf);
	if (err) {
		WARN();
		return NULL;
	}

	rdma_buf->type = BUF_RDMA;
	rdma_buf->xxbuf = buf;
	buf_get(buf);
	rdma_buf->dest = buf->dest;

	return rdma_buf;
}

/*
 * tgt_rdma_init_loc_off
 *	initialize local offsets into ME/LE for RDMA
 */
static int tgt_rdma_init_loc_off(buf_t *buf)
{
	me_t *me = buf->me;

	if (debug)
		printf("me->num_iov(%d), buf->moffset(%d)\n",
			me->num_iov, (int)buf->moffset);

	/* Determine starting vector and vector offset for local le/me */
	buf->cur_loc_iov_index = 0;
	buf->cur_loc_iov_off = 0;

	if (me->num_iov) {
		ptl_iovec_t *iov = (ptl_iovec_t *)me->start;
		ptl_size_t i = 0;
		ptl_size_t loc_offset = 0;
		ptl_size_t iov_offset = 0;

		if (debug)
			printf("*iov(%p)\n", (void *)iov);

		for (i = 0; i < me->num_iov && loc_offset < buf->moffset;
			i++, iov++) {
			iov_offset = buf->moffset - loc_offset;
			if (iov_offset > iov->iov_len)
				iov_offset = iov->iov_len;
			loc_offset += iov_offset;
			if (debug)
				printf("In loop: loc_offset(%d) moffset(%d)\n",
					(int)loc_offset, (int)buf->moffset);
		}
		if (loc_offset < buf->moffset) {
			WARN();
			return PTL_FAIL;
		}

		buf->cur_loc_iov_index = i;
		buf->cur_loc_iov_off = iov_offset;

		buf->start = iov->iov_base + iov_offset;
	} else {
		buf->cur_loc_iov_off = buf->moffset;
		buf->start = me->start + buf->moffset;
	}

	if (debug)
		printf("cur_loc_iov_index(%d), cur_loc_iov_off(%d)\n",
			(int)buf->cur_loc_iov_index,
			(int)buf->cur_loc_iov_off);
	return PTL_OK;
}

static int tgt_data_out(buf_t *buf)
{
	data_t *data = buf->data_out;
	hdr_t *send_hdr = (hdr_t *)buf->send_buf->data;
	int next;
	int err;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	unsigned operation = hdr->operation;

	if (!data) {
		WARN();
		return STATE_TGT_ERROR;
	}

	/* If reply data fits in the reply message, then use immediate
	 * data instead of rdma.
	 * TODO: ensure it's faster than KNEM too. */
	if (buf->mlength < get_param(PTL_MAX_INLINE_DATA)) {
		send_hdr->data_out = 1;
		err = append_tgt_data(buf->me, buf->moffset,
				      buf->mlength, buf->send_buf);
		if (err)
			return STATE_TGT_ERROR;

		/* check to see if we still need data in phase */
		if (buf->put_resid) {
			if (operation == OP_FETCH)
				return STATE_TGT_ATOMIC_DATA_IN;

			if (operation == OP_SWAP)
				return (hdr->atom_op == PTL_SWAP)
					? STATE_TGT_DATA_IN
					: STATE_TGT_SWAP_DATA_IN;

			return  STATE_TGT_DATA_IN;
		}

		assert(buf->in_atomic == 0);

		return STATE_TGT_COMM_EVENT;
	}

	assert(buf->in_atomic == 0);

	buf->rdma_dir = DATA_DIR_OUT;

	switch (data->data_fmt) {
	case DATA_FMT_RDMA_DMA:
		buf->rdma.cur_rem_sge = &data->rdma.sge_list[0];
		buf->rdma.cur_rem_off = 0;
		buf->rdma.num_rem_sge = le32_to_cpu(data->rdma.num_sge);

		if (tgt_rdma_init_loc_off(buf))
			return STATE_TGT_ERROR;

		next = STATE_TGT_RDMA;
		break;

	case DATA_FMT_SHMEM_DMA:
		buf->shmem.cur_rem_iovec = &data->shmem.knem_iovec[0];
		buf->shmem.num_rem_iovecs = data->shmem.num_knem_iovecs;
		buf->shmem.cur_rem_off = 0;

		if (tgt_rdma_init_loc_off(buf))
			return STATE_TGT_ERROR;

		next = STATE_TGT_RDMA;
		break;

	case DATA_FMT_RDMA_INDIRECT:
		next = STATE_TGT_RDMA_DESC;
		break;

	case DATA_FMT_SHMEM_INDIRECT:
		next = STATE_TGT_SHMEM_DESC;
		break;

	default:
		abort();
		WARN();
		return STATE_TGT_ERROR;
		break;
	}

	return next;
}

/*
 * tgt_rdma
 *	initiate as many RDMA requests as possible for a XT
 */
static int tgt_rdma(buf_t *buf)
{
	int err;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	ptl_size_t *resid = buf->rdma_dir == DATA_DIR_IN ?
				&buf->put_resid : &buf->get_resid;

	/* post one or more RDMA operations */
	err = buf->conn->transport.post_tgt_dma(buf);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	/* more work to do */
	if (*resid || atomic_read(&buf->rdma.rdma_comp))
		return STATE_TGT_RDMA;

	/* check to see if we still need data in phase */
	if (buf->put_resid) {
		if (hdr->operation == OP_FETCH)
			return STATE_TGT_ATOMIC_DATA_IN;

		if (hdr->operation == OP_SWAP)
			return (hdr->atom_op == PTL_SWAP) ? STATE_TGT_DATA_IN
							 : STATE_TGT_SWAP_DATA_IN;

		return  STATE_TGT_DATA_IN;
	}

	return STATE_TGT_COMM_EVENT;
}

/*
 * tgt_rdma_desc
 *	initiate read of indirect descriptors for initiator IOV
 */
static int tgt_rdma_desc(buf_t *buf)
{
	data_t *data;
	uint64_t raddr;
	uint32_t rkey;
	uint32_t rlen;
	struct ibv_sge sge;
	int err;
	int next;
	buf_t *rdma_buf;

	data = buf->rdma_dir == DATA_DIR_IN ? buf->data_in : buf->data_out;

	/*
	 * Allocate and map indirect buffer and setup to read
	 * descriptor list from initiator memory.
	 */
	raddr = le64_to_cpu(data->rdma.sge_list[0].addr);
	rkey = le32_to_cpu(data->rdma.sge_list[0].lkey);
	rlen = le32_to_cpu(data->rdma.sge_list[0].length);

	if (debug)
		printf("RDMA indirect descriptors:radd(0x%" PRIx64 "), "
		       " rkey(0x%x), len(%d)\n", raddr, rkey, rlen);

	buf->indir_sge = calloc(1, rlen);
	if (!buf->indir_sge) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	if (mr_lookup(obj_to_ni(buf), buf->indir_sge, rlen,
		      &buf->indir_mr)) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	/*
	 * Post RDMA read
	 */
	sge.addr = (uintptr_t)buf->indir_sge;
	sge.lkey = buf->indir_mr->ibmr->lkey;
	sge.length = rlen;

	atomic_set(&buf->rdma.rdma_comp, 1);

	rdma_buf = tgt_alloc_rdma_buf(buf);
	if (!rdma_buf) {
		WARN();
		next = STATE_TGT_ERROR;
		goto done;
	}

	err = post_rdma(buf, rdma_buf, DATA_DIR_IN, raddr, rkey, &sge, 1, 1);
	if (err) {
		WARN();
		buf_put(rdma_buf);
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	next = STATE_TGT_RDMA_WAIT_DESC;

done:
	return next;
}

/*
 * tgt_rdma_wait_desc
 *	indirect descriptor RDMA has completed, initialize for common RDMA code
 */
static int tgt_rdma_wait_desc(buf_t *buf)
{
	data_t *data;

	data = buf->rdma_dir == DATA_DIR_IN ? buf->data_in : buf->data_out;

	buf->rdma.cur_rem_sge = buf->indir_sge;
	buf->rdma.cur_rem_off = 0;
	buf->rdma.num_rem_sge = (le32_to_cpu(data->rdma.sge_list[0].length)) /
			  sizeof(struct ibv_sge);

	if (tgt_rdma_init_loc_off(buf))
		return STATE_TGT_ERROR;

	return STATE_TGT_RDMA;
}

#ifdef WITH_TRANSPORT_SHMEM
/*
 * tgt_shmem_desc
 *	initiate read of indirect descriptors for initiator IOV.
 * Equivalent of tgt_rdma_desc() and tgt_rdma_wait_desc() combined.
 */
static int tgt_shmem_desc(buf_t *buf)
{
	data_t *data;
	int err;
	int next;
	size_t len;
	ni_t *ni = obj_to_ni(buf);

	data = buf->rdma_dir == DATA_DIR_IN ? buf->data_in : buf->data_out;
	len = data->shmem.knem_iovec[0].length;

	/*
	 * Allocate and map indirect buffer and setup to read
	 * descriptor list from initiator memory.
	 */
	/* TODO: alloc + double memory registration is overkill. May be we don't need that for SHMEM. Same with IB. */
	buf->indir_sge = calloc(1, len);
	if (!buf->indir_sge) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	if (mr_lookup(obj_to_ni(buf), buf->indir_sge, len, &buf->indir_mr)) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	err = knem_copy(ni, data->shmem.knem_iovec[0].cookie,
			data->shmem.knem_iovec[0].offset,
			buf->indir_mr->knem_cookie,
			buf->indir_sge -
			buf->indir_mr->ibmr->addr, len);
	if (err != len) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	buf->shmem.cur_rem_iovec = buf->indir_sge;
	buf->shmem.cur_rem_off = 0;
	buf->shmem.num_rem_iovecs = len / sizeof(struct shmem_iovec);

	if (tgt_rdma_init_loc_off(buf)) {
		WARN();
		next = STATE_TGT_ERROR;
		goto done;
	}

	next = STATE_TGT_RDMA;

done:
	return next;
}
#else
static inline int tgt_shmem_desc(buf_t *buf)
{
	/* This state is not reachable when SHMEM is not enabled. */
	abort();
	return STATE_TGT_ERROR;
}
#endif

/*
 * tgt_data_in
 *	handle request for data from initiator to target
 */
static int tgt_data_in(buf_t *buf)
{
	int err;
	me_t *me = buf->me;
	data_t *data = buf->data_in;
	int next;

	switch (data->data_fmt) {
	case DATA_FMT_IMMEDIATE:
		err = tgt_copy_in(buf, me, data->immediate.data);
		if (err)
			return STATE_TGT_ERROR;

		next = STATE_TGT_COMM_EVENT;
		break;
	case DATA_FMT_RDMA_DMA:
		/* Read from SG list provided directly in request */
		buf->rdma.cur_rem_sge = &data->rdma.sge_list[0];
		buf->rdma.cur_rem_off = 0;
		buf->rdma.num_rem_sge = le32_to_cpu(data->rdma.num_sge);

		if (debug)
			printf("cur_rem_sge(%p), num_rem_sge(%d)\n",
				buf->rdma.cur_rem_sge, (int)buf->rdma.num_rem_sge);

		if (tgt_rdma_init_loc_off(buf))
			return STATE_TGT_ERROR;

		buf->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_RDMA;
		break;

	case DATA_FMT_SHMEM_DMA:
		buf->shmem.cur_rem_iovec = &data->shmem.knem_iovec[0];
		buf->shmem.num_rem_iovecs = data->shmem.num_knem_iovecs;
		buf->shmem.cur_rem_off = 0;

		if (tgt_rdma_init_loc_off(buf))
			return STATE_TGT_ERROR;

		buf->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_RDMA;
		break;

	case DATA_FMT_RDMA_INDIRECT:
		buf->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_RDMA_DESC;
		break;

	case DATA_FMT_SHMEM_INDIRECT:
		buf->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_SHMEM_DESC;
		break;
		
	default:
		assert(0);
		WARN();
		next = STATE_TGT_ERROR;
	}

	if (buf->in_atomic) {
		ni_t *ni = obj_to_ni(buf);
		
		pthread_mutex_unlock(&ni->atomic_mutex);
		buf->in_atomic = 0;
	}

	return next;
}

/*
 * tgt_atomic_data_in
 *	handle atomic operation
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
 */
static int tgt_swap_data_in(buf_t *buf)
{
	int err;
	me_t *me = buf->me; /* can be LE or ME */
	data_t *data = buf->data_in;
	uint8_t copy[16]; /* big enough to hold double complex */
	void *dst;
	ni_t *ni;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	uint64_t operand = le64_to_cpu(hdr->operand);

	assert(data->data_fmt == DATA_FMT_IMMEDIATE);

	if (unlikely(me->num_iov)) {
		err = copy_out(buf, me, copy);
		if (err)
			return STATE_TGT_ERROR;

		dst = copy;
	} else {
		dst = me->start + buf->moffset;
	}

	err = swap_data_in(hdr->atom_op, hdr->atom_type, dst,
			   data->immediate.data, &operand);
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

static int tgt_comm_event(buf_t *buf)
{
	int err = PTL_OK;

	if (buf->event_mask & XT_COMM_EVENT)
		err = make_comm_event(buf);
		if (err) {
			WARN();
			return STATE_TGT_ERROR;
		}

	if (buf->event_mask & XT_CT_COMM_EVENT)
		make_ct_comm_event(buf);

	if (buf->event_mask & XT_REPLY_EVENT)
		return STATE_TGT_SEND_REPLY;

	if (buf->event_mask & XT_ACK_EVENT)
		return STATE_TGT_SEND_ACK;

	return STATE_TGT_CLEANUP;
}

static int tgt_send_ack(buf_t *buf)
{
	int err;
	buf_t *send_buf;
	hdr_t *send_hdr;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;

	send_buf = buf->send_buf;
	send_hdr = (hdr_t *)send_buf->data;

	xport_hdr_from_buf(send_hdr, buf);
	base_hdr_from_buf(send_hdr, buf);

	switch (hdr->ack_req) {
	case PTL_NO_ACK_REQ:
		WARN();
		return STATE_TGT_ERROR;
	case PTL_ACK_REQ:
		send_hdr->operation = OP_ACK;
		break;
	case PTL_CT_ACK_REQ:
		send_hdr->operation = OP_CT_ACK;
		break;
	case PTL_OC_ACK_REQ:
		send_hdr->operation = OP_OC_ACK;
		break;
	default:
		WARN();
		return STATE_TGT_ERROR;
	}

	if (buf->le && buf->le->options & PTL_LE_ACK_DISABLE)
		send_hdr->operation = OP_NO_ACK;

	if (buf->le && buf->le->ptl_list == PTL_PRIORITY_LIST) {
		/* The LE must be released before we sent the ack. */
		le_put(buf->le);
		buf->le = NULL;
	}

	send_buf->dest = buf->dest;
	err = buf->conn->transport.send_message(send_buf, 1);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	return STATE_TGT_CLEANUP;
}

static int tgt_send_reply(buf_t *buf)
{
	int err;
	buf_t *send_buf;
	hdr_t *send_hdr;

	send_buf = buf->send_buf;
	send_hdr = (hdr_t *)send_buf->data;

	xport_hdr_from_buf(send_hdr, buf);
	base_hdr_from_buf(send_hdr, buf);

	send_hdr->operation = OP_REPLY;

	if (buf->le && buf->le->ptl_list == PTL_PRIORITY_LIST) {
		/* The LE must be released before we sent the ack. */
		le_put(buf->le);
		buf->le = NULL;
	}

	send_buf->dest = buf->dest;
	err = buf->conn->transport.send_message(send_buf, 1);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	return STATE_TGT_CLEANUP;
}

static int tgt_cleanup(buf_t *buf)
{
	int state;
	pt_t *pt;

	if (buf->matching.le) {
		/* On the overflow list, and was already matched by an
		 * ME/LE. */
		assert(buf->le->ptl_list == PTL_OVERFLOW_LIST);
		state = STATE_TGT_OVERFLOW_EVENT;
	} else if (buf->le && buf->le->ptl_list == PTL_OVERFLOW_LIST)
		state = STATE_TGT_WAIT_APPEND;
	else
		state = STATE_TGT_CLEANUP_2;

	if (buf->indir_sge) {
		if (buf->indir_mr) {
			mr_put(buf->indir_mr);
			buf->indir_mr = NULL;
		}
		free(buf->indir_sge);
		buf->indir_sge = NULL;
	}

#ifndef NO_ARG_VALIDATION
	pthread_spin_lock(&buf->rdma_list_lock);
	while(!list_empty(&buf->rdma_list)) {
		buf_t *rdma_buf = list_first_entry(&buf->rdma_list,
						   buf_t, list);
		list_del(&rdma_buf->list);
		buf_put(rdma_buf);
		abort();	/* this should not happen */
	}
	pthread_spin_unlock(&buf->rdma_list_lock);
#endif

	if (buf->send_buf) {
		buf_put(buf->send_buf);
		buf->send_buf = NULL;
	}

	pt = buf->pt;

	pthread_spin_lock(&pt->lock);
	pt->num_tgt_active--;
	if ((pt->disable & PT_AUTO_DISABLE) && !pt->num_tgt_active) {
		pt->enabled = 0;
		pt->disable &= ~PT_AUTO_DISABLE;
		pthread_spin_unlock(&pt->lock);

		// TODO: don't send if PTL_LE_EVENT_FLOWCTRL_DISABLE ?
		make_target_event(buf, pt->eq,
				  PTL_EVENT_PT_DISABLED,
				  buf->matching.le ? buf->matching.le->user_ptr
						  : NULL, NULL);
	} else
		pthread_spin_unlock(&pt->lock);

	return state;
}

static void tgt_cleanup_2(buf_t *buf)
{
	/* tgt must release reference to any LE/ME */
	if (buf->le) {
		le_put(buf->le);
		buf->le = NULL;
	}
}

static int tgt_overflow_event(buf_t *buf)
{
	le_t *le = buf->matching.le;
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;
	unsigned operation = hdr->operation;

	assert(le);

	if (!(le->options & PTL_LE_EVENT_OVER_DISABLE)) {
		switch (operation) {
		case OP_PUT:
			make_target_event(buf, buf->pt->eq, PTL_EVENT_PUT_OVERFLOW,
					  le->user_ptr, buf->start);
			break;

		case OP_ATOMIC:
			make_target_event(buf, buf->pt->eq, PTL_EVENT_ATOMIC_OVERFLOW,
					  le->user_ptr, buf->start);
			break;

		case OP_FETCH:
		case OP_SWAP:
			make_target_event(buf, buf->pt->eq, PTL_EVENT_FETCH_ATOMIC_OVERFLOW,
					  le->user_ptr, buf->start);
			break;

		case OP_GET:
			make_target_event(buf, buf->pt->eq, PTL_EVENT_GET_OVERFLOW,
					  le->user_ptr, buf->start);
			break;

		default:
			/* Not possible. */
			abort();
			break;
		}

		/* Update the counter if we can. If LE comes from PtlLESearch,
		 * then ct is NULL. */
		if ((le->options & PTL_LE_EVENT_CT_OVERFLOW) && le->ct)
			make_ct_event(le->ct, buf, CT_MBYTES);
	}

	le_put(le);
	buf->matching.le = NULL;

	return STATE_TGT_CLEANUP_2;
}

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

/*
 * process_tgt
 *	process incoming request message
 */
int process_tgt(buf_t *buf)
{
	int err = PTL_OK;
	enum tgt_state state;

	if(debug)
		printf("process_tgt: called buf = %p\n", buf);

	pthread_mutex_lock(&buf->mutex);

	state = buf->tgt_state;

	while(1) {
		if (debug)
			printf("%p: tgt state = %s\n",
				   buf, tgt_state_name[state]);

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
		case STATE_TGT_DATA_IN:
			state = tgt_data_in(buf);
			break;
		case STATE_TGT_RDMA_DESC:
			state = tgt_rdma_desc(buf);
			if (state == STATE_TGT_RDMA_DESC)
				goto exit;
			if (state == STATE_TGT_RDMA_WAIT_DESC)
				goto exit;
			break;
		case STATE_TGT_RDMA_WAIT_DESC:
			state = tgt_rdma_wait_desc(buf);
			break;
		case STATE_TGT_SHMEM_DESC:
			state = tgt_shmem_desc(buf);
			break;
		case STATE_TGT_RDMA:
			state = tgt_rdma(buf);
			if (state == STATE_TGT_RDMA)
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
			if (state == STATE_TGT_SEND_ACK)
				goto exit;
			break;
		case STATE_TGT_SEND_REPLY:
			state = tgt_send_reply(buf);
			if (state == STATE_TGT_SEND_REPLY)
				goto exit;
			break;
		case STATE_TGT_DROP:
			state = request_drop(buf);
			break;
		case STATE_TGT_OVERFLOW_EVENT:
			state = tgt_overflow_event(buf);
			break;
		case STATE_TGT_WAIT_APPEND:
			state = tgt_wait_append(buf);
			if (state == STATE_TGT_WAIT_APPEND)
				goto exit;
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
			buf_put(buf);		/* match buf_alloc */
			return err;
			break;

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
