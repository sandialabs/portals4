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
static int make_comm_event(xt_t *xt)
{
	if (xt->ni_fail || !(xt->le->options & PTL_LE_EVENT_SUCCESS_DISABLE)) {

		ptl_event_kind_t type;

		if (xt->operation == OP_PUT)
			type = PTL_EVENT_PUT;
		else if (xt->operation == OP_GET)
			type = PTL_EVENT_GET;
		else if (xt->operation == OP_ATOMIC || xt->operation == OP_FETCH ||
				 xt->operation == OP_SWAP)
			type = PTL_EVENT_ATOMIC;
		else {
			WARN();
			return STATE_TGT_ERROR;
		}

		make_target_event(xt, xt->pt->eq, type, xt->le->user_ptr, xt->le->start+xt->moffset);
	}

	xt->event_mask &= ~XT_COMM_EVENT;

	return PTL_OK;
}

/*
 * make_ct_comm_event
 */
static void make_ct_comm_event(xt_t *xt)
{
	le_t *le = xt->le;
	int bytes = le->options & PTL_LE_EVENT_CT_BYTES;

	make_ct_event(le->ct, xt->ni_fail, xt->mlength, bytes);
	xt->event_mask &= ~XT_CT_COMM_EVENT;
}

/*
 * init_events
 *	decide whether comm eq/ct events will happen
 *	for this message
 */
static void init_events(xt_t *xt)
{
	if (xt->pt->eq && !(xt->le->options & PTL_LE_EVENT_COMM_DISABLE)) {
		xt->event_mask |= XT_COMM_EVENT;
	}

	if (xt->le->ct && (xt->le->options & PTL_LE_EVENT_CT_COMM)) {
		xt->event_mask |= XT_CT_COMM_EVENT;
	}
}

/*
 * copy_in
 *	copy data from data segment into le/me
 */
static int copy_in(xt_t *xt, me_t *me, void *data)
{
	int err;
	ptl_size_t offset = xt->moffset;
	ptl_size_t length = xt->mlength;

	if (me->num_iov) {
		void *dst_start;
		err = iov_copy_in(data, (ptl_iovec_t *)me->start,
						  me->num_iov, offset, length, &dst_start);
		if (err) {
			WARN();
			return STATE_TGT_ERROR;
		}

		/* this is silly and meaningless but it is what the spec demands */
		xt->start = dst_start;
	} else {
		xt->start = me->start + offset;
		memcpy(xt->start, data, length);
	}

	return PTL_OK;
}

/*
 * atomic_in
 */
static int atomic_in(xt_t *xt, me_t *me, void *data)
{
	int err;
	ptl_size_t offset = xt->moffset;
	ptl_size_t length = xt->mlength;
	atom_op_t op;

	op = atom_op[xt->atom_op][xt->atom_type];
	assert(op);

	/* this implementation assumes that the architecture can support
	 * misaligned arithmetic operations. This is OK for x86
	 * and x86 variants and generally most modern architectures */
	if (me->num_iov) {
		err = iov_atomic_in(op, atom_type_size[xt->atom_type],
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
static int copy_out(xt_t *xt, me_t *me, void *data)
{
	int err;
	ptl_size_t offset = xt->moffset;
	ptl_size_t length = xt->mlength;

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
static int prepare_send_buf(xt_t *xt)
{
	buf_t *buf;
	int err;

	err = buf_alloc(obj_to_ni(xt), &buf);
	if (err) {
		WARN();
		return PTL_FAIL;
	}
	buf->type = BUF_SEND;
	buf->xt = xt;
	xt_get(xt);
	buf->dest = &xt->dest;
	xt->send_buf = buf;

	return PTL_OK;
}

/*
 * tgt_start
 *	get portals table entry from request
 */
static int tgt_start(xt_t *xt)
{
	ni_t *ni = obj_to_ni(xt);

	switch (xt->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (xt->ack_req != PTL_NO_ACK_REQ)
			xt->event_mask |= XT_ACK_EVENT;
		break;
	case OP_GET:
	case OP_FETCH:
	case OP_SWAP:
		if (xt->ack_req != PTL_NO_ACK_REQ) {
			xt->event_mask |= XT_REPLY_EVENT;
		}
		break;
	}

	/* Allocate the ack/reply buffer */
	if ((xt->event_mask & (XT_ACK_EVENT | XT_REPLY_EVENT)) &&
		(prepare_send_buf(xt) != PTL_OK))
		return STATE_TGT_ERROR;

	if (xt->pt_index >= ni->limits.max_pt_index) {
		WARN();
		xt->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}

	xt->pt = &ni->pt[xt->pt_index];
	if (!xt->pt->in_use) {
		WARN();
		xt->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}

	/* Serialize between progress and API */
	pthread_spin_lock(&xt->pt->lock);
	if (!xt->pt->enabled || xt->pt->disable) {
		pthread_spin_unlock(&xt->pt->lock);
		xt->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}
	xt->pt->num_xt_active++;
	pthread_spin_unlock(&xt->pt->lock);

	return STATE_TGT_GET_MATCH;
}

/*
 * request_drop
 *	drop a request
 */
static int request_drop(xt_t *xt)
{
	/* logging ? */

	return STATE_TGT_WAIT_CONN;
}

/*
 * check_match
 *	determine if ME matches XT request info.
 */
static int check_match(const xt_t *xt, const me_t *me)
{
	const ni_t *ni = obj_to_ni(xt);
	ptl_size_t offset;
	ptl_size_t length;

	if (ni->options & PTL_NI_LOGICAL) {
		if (!(me->id.rank == PTL_RANK_ANY ||
		     (me->id.rank == xt->initiator.rank)))
			return 0;
	} else {
		if (!(me->id.phys.nid == PTL_NID_ANY ||
		     (me->id.phys.nid == xt->initiator.phys.nid)))
			return 0;
		if (!(me->id.phys.pid == PTL_PID_ANY ||
		     (me->id.phys.pid == xt->initiator.phys.pid)))
			return 0;
	}

	length = xt->rlength;
	offset = (me->options & PTL_ME_MANAGE_LOCAL) ?
			me->offset : xt->roffset;

	if ((me->options & PTL_ME_NO_TRUNCATE) &&
	    ((offset + length) > me->length))
			return 0;

	return (xt->match_bits | me->ignore_bits) ==
		(me->match_bits | me->ignore_bits);
}

/*
 * check_perm
 *	check permission on incoming request packet against ME/LE
 * Returns 0 for permission granted, else an error code
 */
static int check_perm(const xt_t *xt, const le_t *le)
{
	int ret = PTL_NI_OK;

	if (!(le->uid == PTL_UID_ANY || (le->uid == xt->uid))) {
		WARN();
		ret = PTL_NI_PERM_VIOLATION;
	} else {
		switch (xt->operation) {
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
static int tgt_get_match(xt_t *xt)
{
	ni_t *ni = obj_to_ni(xt);
	struct list_head *l;
	int perm_ret;
	pt_t *pt = xt->pt;

	/* have to protect against a race with le/me append/search
	 * which change the pt lists */
	pthread_spin_lock(&pt->lock);

	if (pt->options & PTL_PT_FLOWCTRL) {
		if (list_empty(&pt->priority_list) &&
		    list_empty(&pt->overflow_list)) {
			WARN();
			pt->disable |= PT_AUTO_DISABLE;
			pthread_spin_unlock(&pt->lock);
			xt->ni_fail = PTL_NI_FLOW_CTRL;
			xt->le = NULL;
			return STATE_TGT_DROP;
		}
	}

	list_for_each(l, &pt->priority_list) {
		xt->le = list_entry(l, le_t, list);
		if (ni->options & PTL_NI_NO_MATCHING) {
			le_get(xt->le);
			goto done;
		}

		if (check_match(xt, xt->me)) {
			me_get(xt->me);
			goto done;
		}
	}

	list_for_each(l, &pt->overflow_list) {
		xt->le = list_entry(l, le_t, list);
		if (ni->options & PTL_NI_NO_MATCHING) {
			le_get(xt->le);
			goto done;
		}

		if (check_match(xt, xt->me)) {
			me_get(xt->me);
			goto done;
		}
	}

	pthread_spin_unlock(&pt->lock);
	WARN();
	xt->le = NULL;
	xt->ni_fail = PTL_NI_DROPPED;
	return STATE_TGT_DROP;

done:
	if ((perm_ret = check_perm(xt, xt->le))) {
		pthread_spin_unlock(&pt->lock);
		le_put(xt->le);
		xt->le = NULL;

		xt->ni_fail = perm_ret;
		return STATE_TGT_DROP;
	}

	if (xt->le->ptl_list == PTL_OVERFLOW_LIST) {
		xt_get(xt);
		list_add_tail(&xt->unexpected_list, &xt->le->pt->unexpected_list);
	}

	pthread_spin_unlock(&pt->lock);
	return STATE_TGT_GET_LENGTH;
}

/*
 * tgt_get_length
 *	determine the data in/out transfer lengths
 */
static int tgt_get_length(xt_t *xt)
{
	const ni_t *ni = obj_to_ni(xt);
	me_t *me = xt->me;
	ptl_size_t offset;
	ptl_size_t length;

	/* note le->options & PTL_ME_MANAGE_LOCAL is always zero */
	offset = (me->options & PTL_ME_MANAGE_LOCAL) ? me->offset : xt->roffset;

	if (offset > me->length) {
		/* Messages that are outside the bounds of the ME are
		 * truncated to zero bytes. */
		length = 0;
	} else {
		ptl_size_t room;

		room = me->length - offset;
		length = (room >= xt->rlength) ? xt->rlength : room;
	}

	switch (xt->operation) {
	case OP_PUT:
		if (length > ni->limits.max_msg_size)
			length = ni->limits.max_msg_size;
		xt->put_resid = length;
		break;

	case OP_GET:
		if (length > ni->limits.max_msg_size)
		length = ni->limits.max_msg_size;
		xt->get_resid = length;
		break;

	case OP_ATOMIC:
		if (length > ni->limits.max_atomic_size)
			length = ni->limits.max_atomic_size;
		xt->put_resid = length;
		break;

	case OP_FETCH:
		if (length > ni->limits.max_atomic_size)
			length = ni->limits.max_atomic_size;
		xt->put_resid = length;
		xt->get_resid = length;
		break;

	case OP_SWAP:
		if (xt->atom_op == PTL_SWAP) {
			if (length > ni->limits.max_atomic_size)
				length = ni->limits.max_atomic_size;
		} else {
			if (length > atom_type_size[xt->atom_type])
				length = atom_type_size[xt->atom_type];
		}
		xt->put_resid = length;
		xt->get_resid = length;
		break;
	}

	xt->mlength = length;
	xt->moffset = offset;

	init_events(xt);

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
		le_unlink(xt->le, !(me->options & PTL_ME_EVENT_UNLINK_DISABLE));
	}

	return STATE_TGT_WAIT_CONN;
}

/*
 * tgt_wait_conn
 *	check whether we need a connection to init
 *	and if so wait until we are connected
 */
static int tgt_wait_conn(xt_t *xt)
{
	ni_t *ni = obj_to_ni(xt);
	conn_t *conn = xt->conn;

	/* we need a connection if we are sending an ack/reply
	 * or doing an RDMA operation */
	if (!(xt->event_mask & (XT_ACK_EVENT | XT_REPLY_EVENT)) &&
	    !(xt->data_out || (xt->data_in && (xt->data_in->data_fmt
						!= DATA_FMT_IMMEDIATE))))
		goto out1;

	/* get per conn info */
	if (!conn) {
		conn = xt->conn = get_conn(ni, &xt->initiator);
		if (unlikely(!conn)) {
			WARN();
			return STATE_TGT_ERROR;
		}
	}

	if (conn->state >= CONN_STATE_CONNECTED)
		goto out2;

	/* if not connected. Add the xt on the pending list. It will be
	 * retried once connected/disconnected. */
	pthread_mutex_lock(&conn->mutex);
	if (conn->state < CONN_STATE_CONNECTED) {
		pthread_spin_lock(&conn->wait_list_lock);
		list_add_tail(&xt->list, &conn->xt_list);
		pthread_spin_unlock(&conn->wait_list_lock);

		if (conn->state == CONN_STATE_DISCONNECTED) {
			/* Initiate connection. */
			if (init_connect(ni, conn)) {
				pthread_mutex_unlock(&conn->mutex);
				pthread_spin_lock(&conn->wait_list_lock);
				list_del(&xt->list);
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
		set_xt_dest(xt, conn->main_connect);
	else
#endif
		set_xt_dest(xt, conn);

out1:
	if (xt->get_resid)
		return STATE_TGT_DATA_OUT;

	if (xt->put_resid)
		return (xt->operation == OP_ATOMIC) ? STATE_TGT_ATOMIC_DATA_IN
						    : STATE_TGT_DATA_IN;

	return STATE_TGT_COMM_EVENT;
}

buf_t *tgt_alloc_rdma_buf(xt_t *xt)
{
	buf_t *buf;
	int err;

	if (debug)
		printf("tgt_alloc_rdma_buf\n");

	err = buf_alloc(obj_to_ni(xt), &buf);
	if (err) {
		WARN();
		return NULL;
	}
	buf->type = BUF_RDMA;
	buf->xt = xt;
	xt_get(xt);
	buf->dest = &xt->dest;

	return buf;
}

/*
 * tgt_rdma_init_loc_off
 *	initialize local offsets into ME/LE for RDMA
 */
static int tgt_rdma_init_loc_off(xt_t *xt)
{
	me_t *me = xt->me;

	if (debug)
		printf("me->num_iov(%d), xt->moffset(%d)\n",
			me->num_iov, (int)xt->moffset);

	/* Determine starting vector and vector offset for local le/me */
	xt->cur_loc_iov_index = 0;
	xt->cur_loc_iov_off = 0;

	if (me->num_iov) {
		ptl_iovec_t *iov = (ptl_iovec_t *)me->start;
		ptl_size_t i = 0;
		ptl_size_t loc_offset = 0;
		ptl_size_t iov_offset = 0;

		if (debug)
			printf("*iov(%p)\n", (void *)iov);

		for (i = 0; i < me->num_iov && loc_offset < xt->moffset;
			i++, iov++) {
			iov_offset = xt->moffset - loc_offset;
			if (iov_offset > iov->iov_len)
				iov_offset = iov->iov_len;
			loc_offset += iov_offset;
			if (debug)
				printf("In loop: loc_offset(%d) moffset(%d)\n",
					(int)loc_offset, (int)xt->moffset);
		}
		if (loc_offset < xt->moffset) {
			WARN();
			return PTL_FAIL;
		}

		xt->cur_loc_iov_index = i;
		xt->cur_loc_iov_off = iov_offset;

		xt->start = iov->iov_base + iov_offset;
	} else {
		xt->cur_loc_iov_off = xt->moffset;
		xt->start = me->start + xt->moffset;
	}

	if (debug)
		printf("cur_loc_iov_index(%d), cur_loc_iov_off(%d)\n",
			(int)xt->cur_loc_iov_index,
			(int)xt->cur_loc_iov_off);
	return PTL_OK;
}

static int tgt_data_out(xt_t *xt)
{
	data_t *data = xt->data_out;
	int next;

	if (!data) {
		WARN();
		return STATE_TGT_ERROR;
	}

	xt->rdma_dir = DATA_DIR_OUT;

	switch (data->data_fmt) {
	case DATA_FMT_RDMA_DMA:
		xt->rdma.cur_rem_sge = &data->rdma.sge_list[0];
		xt->rdma.cur_rem_off = 0;
		xt->rdma.num_rem_sge = be32_to_cpu(data->rdma.num_sge);

		if (tgt_rdma_init_loc_off(xt))
			return STATE_TGT_ERROR;

		next = STATE_TGT_RDMA;
		break;

	case DATA_FMT_SHMEM_DMA:
		xt->shmem.cur_rem_iovec = &data->shmem.knem_iovec[0];
		xt->shmem.num_rem_iovecs = data->shmem.num_knem_iovecs;
		xt->shmem.cur_rem_off = 0;

		if (tgt_rdma_init_loc_off(xt))
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
static int tgt_rdma(xt_t *xt)
{
	int err;
	ptl_size_t *resid = xt->rdma_dir == DATA_DIR_IN ?
				&xt->put_resid : &xt->get_resid;

	/* post one or more RDMA operations */
	err = xt->conn->transport.post_tgt_dma(xt);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	/* more work to do */
	if (*resid || atomic_read(&xt->rdma.rdma_comp))
		return STATE_TGT_RDMA;

	/* check to see if we still need data in phase */
	if (xt->put_resid) {
		if (xt->operation == OP_FETCH)
			return STATE_TGT_ATOMIC_DATA_IN;

		if (xt->operation == OP_SWAP)
			return (xt->atom_op == PTL_SWAP) ? STATE_TGT_DATA_IN
							 : STATE_TGT_SWAP_DATA_IN;

		return  STATE_TGT_DATA_IN;
	}

	return STATE_TGT_COMM_EVENT;
}

/*
 * tgt_rdma_desc
 *	initiate read of indirect descriptors for initiator IOV
 */
static int tgt_rdma_desc(xt_t *xt)
{
	data_t *data;
	uint64_t raddr;
	uint32_t rkey;
	uint32_t rlen;
	struct ibv_sge sge;
	int err;
	int next;
	buf_t *rdma_buf;

	data = xt->rdma_dir == DATA_DIR_IN ? xt->data_in : xt->data_out;

	/*
	 * Allocate and map indirect buffer and setup to read
	 * descriptor list from initiator memory.
	 */
	raddr = be64_to_cpu(data->rdma.sge_list[0].addr);
	rkey = be32_to_cpu(data->rdma.sge_list[0].lkey);
	rlen = be32_to_cpu(data->rdma.sge_list[0].length);

	if (debug)
		printf("RDMA indirect descriptors:radd(0x%" PRIx64 "), "
		       " rkey(0x%x), len(%d)\n", raddr, rkey, rlen);

	xt->indir_sge = calloc(1, rlen);
	if (!xt->indir_sge) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	if (mr_lookup(obj_to_ni(xt), xt->indir_sge, rlen,
		      &xt->indir_mr)) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	/*
	 * Post RDMA read
	 */
	sge.addr = (uintptr_t)xt->indir_sge;
	sge.lkey = xt->indir_mr->ibmr->lkey;
	sge.length = rlen;

	atomic_set(&xt->rdma.rdma_comp, 1);

	rdma_buf = tgt_alloc_rdma_buf(xt);
	if (!rdma_buf) {
		WARN();
		next = STATE_TGT_ERROR;
		goto done;
	}

	err = rdma_read(rdma_buf, raddr, rkey, &sge, 1, 1);
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
static int tgt_rdma_wait_desc(xt_t *xt)
{
	data_t *data;

	data = xt->rdma_dir == DATA_DIR_IN ? xt->data_in : xt->data_out;

	xt->rdma.cur_rem_sge = xt->indir_sge;
	xt->rdma.cur_rem_off = 0;
	xt->rdma.num_rem_sge = (be32_to_cpu(data->rdma.sge_list[0].length)) /
			  sizeof(struct ibv_sge);

	if (tgt_rdma_init_loc_off(xt))
		return STATE_TGT_ERROR;

	return STATE_TGT_RDMA;
}

#ifdef WITH_TRANSPORT_SHMEM
/*
 * tgt_shmem_desc
 *	initiate read of indirect descriptors for initiator IOV.
 * Equivalent of tgt_rdma_desc() and tgt_rdma_wait_desc() combined.
 */
static int tgt_shmem_desc(xt_t *xt)
{
	data_t *data;
	int err;
	int next;
	size_t len;

	data = xt->rdma_dir == DATA_DIR_IN ? xt->data_in : xt->data_out;
	len = data->shmem.knem_iovec[0].length;

	/*
	 * Allocate and map indirect buffer and setup to read
	 * descriptor list from initiator memory.
	 */
	/* TODO: alloc + double memory registration is overkill. May be we don't need that for SHMEM. Same with IB. */
	xt->indir_sge = calloc(1, len);
	if (!xt->indir_sge) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	if (mr_lookup(obj_to_ni(xt), xt->indir_sge, len, &xt->indir_mr)) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	err = knem_copy(xt->obj.obj_ni,
					data->shmem.knem_iovec[0].cookie,
					data->shmem.knem_iovec[0].offset,
					xt->indir_mr->knem_cookie,
					xt->indir_sge - xt->indir_mr->ibmr->addr,
					len);
	if (err != len) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	xt->shmem.cur_rem_iovec = xt->indir_sge;
	xt->shmem.cur_rem_off = 0;
	xt->shmem.num_rem_iovecs = len / sizeof(struct shmem_iovec);

	if (tgt_rdma_init_loc_off(xt)) {
		WARN();
		next = STATE_TGT_ERROR;
		goto done;
	}

	next = STATE_TGT_RDMA;

done:
	return next;
}
#else
static inline int tgt_shmem_desc(xt_t *xt)
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
static int tgt_data_in(xt_t *xt)
{
	int err;
	me_t *me = xt->me;
	data_t *data = xt->data_in;
	int next;

	switch (data->data_fmt) {
	case DATA_FMT_IMMEDIATE:
		err = copy_in(xt, me, data->immediate.data);
		if (err)
			return STATE_TGT_ERROR;

		next = STATE_TGT_COMM_EVENT;
		break;
	case DATA_FMT_RDMA_DMA:
		/* Read from SG list provided directly in request */
		xt->rdma.cur_rem_sge = &data->rdma.sge_list[0];
		xt->rdma.cur_rem_off = 0;
		xt->rdma.num_rem_sge = be32_to_cpu(data->rdma.num_sge);

		if (debug)
			printf("cur_rem_sge(%p), num_rem_sge(%d)\n",
				xt->rdma.cur_rem_sge, (int)xt->rdma.num_rem_sge);

		if (tgt_rdma_init_loc_off(xt))
			return STATE_TGT_ERROR;

		xt->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_RDMA;
		break;

	case DATA_FMT_SHMEM_DMA:
		xt->shmem.cur_rem_iovec = &data->shmem.knem_iovec[0];
		xt->shmem.num_rem_iovecs = data->shmem.num_knem_iovecs;
		xt->shmem.cur_rem_off = 0;

		if (tgt_rdma_init_loc_off(xt))
			return STATE_TGT_ERROR;

		xt->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_RDMA;
		break;

	case DATA_FMT_RDMA_INDIRECT:
		xt->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_RDMA_DESC;
		break;

	case DATA_FMT_SHMEM_INDIRECT:
		xt->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_SHMEM_DESC;
		break;
		
	default:
		assert(0);
		WARN();
		next = STATE_TGT_ERROR;
	}

	return next;
}

/*
 * tgt_atomic_data_in
 *	handle atomic operation
 */
static int tgt_atomic_data_in(xt_t *xt)
{
	int err;
	data_t *data = xt->data_in;
	me_t *me = xt->me;

	/* assumes that max_atomic_size is <= PTL_MAX_INLINE_DATA */
	if (data->data_fmt != DATA_FMT_IMMEDIATE) {
		WARN();
		return STATE_TGT_ERROR;
	}

	// TODO should we return an ni fail??
	if (xt->atom_op > PTL_BXOR || xt->atom_type >= PTL_DATATYPE_LAST) {
		WARN();
		return STATE_TGT_ERROR;
	}

	err = atomic_in(xt, me, data->immediate.data);
	if (err)
		return STATE_TGT_ERROR;

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
static int tgt_swap_data_in(xt_t *xt)
{
	int err;
	me_t *me = xt->me; /* can be LE or ME */
	data_t *data = xt->data_in;
	uint8_t copy[16]; /* big enough to hold double complex */
	void *dst;

	assert(data->data_fmt == DATA_FMT_IMMEDIATE);

	if (unlikely(me->num_iov)) {
		err = copy_out(xt, me, copy);
		if (err)
			return STATE_TGT_ERROR;

		dst = copy;
	} else {
		dst = me->start + xt->moffset;
	}

	err = swap_data_in(xt->atom_op, xt->atom_type, dst,
			   data->immediate.data, &xt->operand);
	if (err)
		return STATE_TGT_ERROR;

	if (unlikely(me->num_iov)) {
		err = copy_in(xt, xt->me, copy);
		if (err)
			return STATE_TGT_ERROR;
	}

	return STATE_TGT_COMM_EVENT;
}

static int tgt_comm_event(xt_t *xt)
{
	int err = PTL_OK;

	if (xt->event_mask & XT_COMM_EVENT)
		err = make_comm_event(xt);
		if (err) {
			WARN();
			return STATE_TGT_ERROR;
		}

	if (xt->event_mask & XT_CT_COMM_EVENT)
		make_ct_comm_event(xt);

	if (xt->event_mask & XT_REPLY_EVENT)
		return STATE_TGT_SEND_REPLY;

	if (xt->event_mask & XT_ACK_EVENT)
		return STATE_TGT_SEND_ACK;

	return STATE_TGT_CLEANUP;
}

static int tgt_send_ack(xt_t *xt)
{
	int err;
	ni_t *ni = obj_to_ni(xt);
	buf_t *buf;
	hdr_t *hdr;

	xt->event_mask &= ~XT_ACK_EVENT;

	buf = xt->send_buf;
	hdr = (hdr_t *)buf->data;

	memset(hdr, 0, sizeof(*hdr));

	xport_hdr_from_xt(hdr, xt);
	base_hdr_from_xt(hdr, xt);

	switch (xt->ack_req) {
	case PTL_NO_ACK_REQ:
		WARN();
		buf_put(buf);
		return STATE_TGT_ERROR;
	case PTL_ACK_REQ:
		hdr->operation = OP_ACK;
		break;
	case PTL_CT_ACK_REQ:
		hdr->operation = OP_CT_ACK;
		break;
	case PTL_OC_ACK_REQ:
		hdr->operation = OP_OC_ACK;
		break;
	default:
		WARN();
		buf_put(buf);
		return STATE_TGT_ERROR;
	}

	if (xt->le && xt->le->options & PTL_LE_ACK_DISABLE)
		hdr->operation = OP_NO_ACK;

	buf->length = sizeof(*hdr);

	err = xt->conn->transport.send_message(buf, 1);
	if (err) {
		WARN();
		buf_put(buf);
		return STATE_TGT_ERROR;
	}

	xt->send_buf = NULL;

	return STATE_TGT_CLEANUP;
}

static int tgt_send_reply(xt_t *xt)
{
	int err;
	ni_t *ni = obj_to_ni(xt);
	buf_t *buf;
	hdr_t *hdr;

	xt->event_mask &= ~XT_REPLY_EVENT;

	buf = xt->send_buf;
	hdr = (hdr_t *)buf->data;

	memset(hdr, 0, sizeof(*hdr));

	xport_hdr_from_xt(hdr, xt);
	base_hdr_from_xt(hdr, xt);

	hdr->operation = OP_REPLY;
	buf->length = sizeof(*hdr);

	err = xt->conn->transport.send_message(buf, 1);
	if (err) {
		WARN();
		buf_put(buf);
		return STATE_TGT_ERROR;
	}

	xt->send_buf = NULL;

	return STATE_TGT_CLEANUP;
}

static int tgt_cleanup(xt_t *xt)
{
	int state;
	pt_t *pt;

	if (xt->matching.le) {
		/* On the overflow list, and was already matched by an
		 * ME/LE. */
		assert(xt->le->ptl_list == PTL_OVERFLOW_LIST);
		state = STATE_TGT_OVERFLOW_EVENT;
	} else if (xt->le && xt->le->ptl_list == PTL_OVERFLOW_LIST)
		state = STATE_TGT_WAIT_APPEND;
	else
		state = STATE_TGT_CLEANUP_2;

	/* tgt must release reference to any LE/ME */
	if (xt->le) {
		le_put(xt->le);
		xt->le = NULL;
	}

	if (xt->indir_sge) {
		if (xt->indir_mr) {
			mr_put(xt->indir_mr);
			xt->indir_mr = NULL;
		}
		free(xt->indir_sge);
		xt->indir_sge = NULL;
	}

#ifndef NO_ARG_VALIDATION
	pthread_spin_lock(&xt->rdma_list_lock);
	while(!list_empty(&xt->rdma_list)) {
		buf_t *buf = list_first_entry(&xt->rdma_list, buf_t, list);
		list_del(&buf->list);

		buf_put(buf);

		abort();				/* this should not happen */
	}
	pthread_spin_unlock(&xt->rdma_list_lock);
#endif

	if (xt->recv_buf) {
		buf_put(xt->recv_buf);
		xt->recv_buf = NULL;
	}

	if (xt->send_buf) {
		buf_put(xt->send_buf);
		xt->send_buf = NULL;
	}

	pt = xt->pt;

	pthread_spin_lock(&pt->lock);
	pt->num_xt_active--;
	if ((pt->disable & PT_AUTO_DISABLE) && !pt->num_xt_active) {
		pt->enabled = 0;
		pt->disable &= ~PT_AUTO_DISABLE;
		pthread_spin_unlock(&pt->lock);

		// TODO: don't send if PTL_LE_EVENT_FLOWCTRL_DISABLE ?
		make_target_event(xt, pt->eq, PTL_EVENT_PT_DISABLED,
						  xt->matching.le ? xt->matching.le->user_ptr : NULL, NULL);
	} else
		pthread_spin_unlock(&pt->lock);

	return state;
}

static int tgt_cleanup_2(xt_t *xt)
{
	xt_put(xt);

	return STATE_TGT_DONE;
}


static int tgt_overflow_event(xt_t *xt)
{
	le_t *le = xt->matching.le;

	assert(xt->le == NULL);
	assert(xt->matching.le);

	if (!(le->options & PTL_LE_EVENT_OVER_DISABLE)) {
		switch (xt->operation) {
		case OP_PUT:
			make_target_event(xt, xt->pt->eq, PTL_EVENT_PUT_OVERFLOW, xt->matching.le->user_ptr, xt->start);
			break;

		case OP_ATOMIC:
			make_target_event(xt, xt->pt->eq, PTL_EVENT_ATOMIC_OVERFLOW, xt->matching.le->user_ptr, xt->start);
			break;

		case OP_FETCH:
		case OP_SWAP:
			make_target_event(xt, xt->pt->eq, PTL_EVENT_FETCH_ATOMIC_OVERFLOW, xt->matching.le->user_ptr, xt->start);
			break;

		case OP_GET:
			make_target_event(xt, xt->pt->eq, PTL_EVENT_GET_OVERFLOW, xt->matching.le->user_ptr, xt->start);
			break;

		default:
			/* Not possible. */
			abort();
			return STATE_TGT_ERROR;
			break;
		}

		/* Update the counter if we can. If LE comes from PtlLESearch,
		 * then ct is NULL. */
		if ((le->options & PTL_LE_EVENT_CT_OVERFLOW) && le->ct)
			make_ct_event(le->ct, xt->ni_fail, xt->mlength, 1);
	}

	le_put(le);
	xt->matching.le = NULL;

	return STATE_TGT_CLEANUP_2;
}

/* The XT is on the overflow list and waiting for a ME/LE search/append. */
static int tgt_wait_append(xt_t *xt)
{
	int state;

	if (xt->matching.le)
		state = STATE_TGT_OVERFLOW_EVENT;
	else
		state = STATE_TGT_WAIT_APPEND;

	return state;
}

/*
 * process_tgt
 *	process incoming request message
 */
int process_tgt(xt_t *xt)
{
	int err = PTL_OK;
	int state;

	if(debug)
		printf("process_tgt: called xt = %p\n", xt);

	pthread_spin_lock(&xt->obj.obj_lock);

	state = xt->state;

	while(1) {
		if (debug)
			printf("%p: tgt state = %s\n",
				   xt, tgt_state_name[state]);

		switch (state) {
		case STATE_TGT_START:
			state = tgt_start(xt);
			break;
		case STATE_TGT_GET_MATCH:
			state = tgt_get_match(xt);
			break;
		case STATE_TGT_GET_LENGTH:
			state = tgt_get_length(xt);
			break;
		case STATE_TGT_WAIT_CONN:
			state = tgt_wait_conn(xt);
			if (state == STATE_TGT_WAIT_CONN)
				goto exit;
			break;
		case STATE_TGT_DATA_IN:
			state = tgt_data_in(xt);
			break;
		case STATE_TGT_RDMA_DESC:
			state = tgt_rdma_desc(xt);
			if (state == STATE_TGT_RDMA_DESC)
				goto exit;
			if (state == STATE_TGT_RDMA_WAIT_DESC)
				goto exit;
			break;
		case STATE_TGT_RDMA_WAIT_DESC:
			state = tgt_rdma_wait_desc(xt);
			break;
		case STATE_TGT_SHMEM_DESC:
			state = tgt_shmem_desc(xt);
			break;
		case STATE_TGT_RDMA:
			state = tgt_rdma(xt);
			if (state == STATE_TGT_RDMA)
				goto exit;
			break;
		case STATE_TGT_ATOMIC_DATA_IN:
			state = tgt_atomic_data_in(xt);
			break;
		case STATE_TGT_SWAP_DATA_IN:
			state = tgt_swap_data_in(xt);
			break;
		case STATE_TGT_DATA_OUT:
			state = tgt_data_out(xt);
			break;
		case STATE_TGT_COMM_EVENT:
			state = tgt_comm_event(xt);
			break;
		case STATE_TGT_SEND_ACK:
			state = tgt_send_ack(xt);
			if (state == STATE_TGT_SEND_ACK)
				goto exit;
			break;
		case STATE_TGT_SEND_REPLY:
			state = tgt_send_reply(xt);
			if (state == STATE_TGT_SEND_REPLY)
				goto exit;
			break;
		case STATE_TGT_DROP:
			state = request_drop(xt);
			break;
		case STATE_TGT_OVERFLOW_EVENT:
			state = tgt_overflow_event(xt);
			break;
		case STATE_TGT_WAIT_APPEND:
			state = tgt_wait_append(xt);
			if (state == STATE_TGT_WAIT_APPEND)
				goto exit;
			break;
		case STATE_TGT_CLEANUP:
			state = tgt_cleanup(xt);
			break;
		case STATE_TGT_CLEANUP_2:
			state = tgt_cleanup_2(xt);
			break;
		case STATE_TGT_ERROR:
			WARN();
			tgt_cleanup(xt);
			err = PTL_FAIL;
			goto exit;
		case STATE_TGT_DONE:
			/* xt isn't valid anymore. */
			goto done;
		}
	}

 exit:
	xt->state = state;

 done:
	pthread_spin_unlock(&xt->obj.obj_lock);

	return err;
}

/* Matches an ME/LE against entries in the unexpected list. 
 * PT lock must be taken.
 */
static void match_le_unexpected(const le_t *le, int dont_check_perm, 
								struct list_head *xt_list)
{
	xt_t *xt;
	xt_t *n;
	pt_t *pt = &le->obj.obj_ni->pt[le->pt_index];
	int no_matching = le->obj.obj_ni->options & PTL_NI_NO_MATCHING;

	assert(pthread_spin_trylock(&pt->lock) != 0);

	INIT_LIST_HEAD(xt_list);

	/* Check this new LE against the overflowlist. */
	list_for_each_entry_safe(xt, n, &pt->unexpected_list, unexpected_list) {

		if ((no_matching || check_match(xt, (me_t *)le)) &&
			(dont_check_perm || !check_perm(xt, le))) {
			list_del(&xt->unexpected_list);
			list_add_tail(&xt->unexpected_list, xt_list);

			if (le->options & PTL_LE_USE_ONCE)
				break;
		}
	}

	return;
}


/* Check whether that LE/ME matches one or more XT on the unexpected
 * list. Return true is at least one XT was processed.
 * PT lock must be taken.
 */
int check_overflow(le_t *le)
{
	xt_t *xt;
	xt_t *n;
	pt_t *pt = &le->obj.obj_ni->pt[le->pt_index];
	struct list_head xt_list;
	int ret;

	assert(pthread_spin_trylock(&pt->lock) != 0);

	match_le_unexpected(le, 1, &xt_list);

	ret = !list_empty(&xt_list);

	if (ret) {
		/* Process the elements of the list. */
		pthread_spin_unlock(&pt->lock);

		list_for_each_entry_safe(xt, n, &xt_list, unexpected_list) {
			int err;
			int state;

			pthread_spin_lock(&xt->obj.obj_lock);

			assert(xt->matching.le == NULL);
			xt->matching.le = le;
			le_get(le);

			list_del(&xt->unexpected_list);

			state = xt->state;

			pthread_spin_unlock(&xt->obj.obj_lock);

			if (state == STATE_TGT_WAIT_APPEND) {
				err = process_tgt(xt);
				if (err)
					WARN();
			}

			/* From get_match(). */
			xt_put(xt);
		}

		pthread_spin_lock(&pt->lock);
	}

	return ret;
}

/* Check whether that LE/ME matches one or more XT on the unexpected
 * list.
 */
int check_overflow_search_only(le_t *le)
{
	xt_t *xt;
	xt_t xt_dup;
	xt_t *n;
	pt_t *pt = &le->obj.obj_ni->pt[le->pt_index];
	int no_matching = le->obj.obj_ni->options & PTL_NI_NO_MATCHING;
	int found = 0;

	/* Check this new LE against the overflowlist. */
	pthread_spin_lock(&pt->lock);

	list_for_each_entry_safe(xt, n, &pt->unexpected_list, unexpected_list) {

		if ((no_matching || check_match(xt, (me_t *)le))) {
			found = 1;

			/* Work on a copy of XT because it might be disposed
			 * before we can post the event and because we have to
			 * set the ni_fail field. */
			if (le->eq)
				xt_dup = *xt;
			break;
		}
	}

	pthread_spin_unlock(&pt->lock);

	if (le->eq && !(le->options & PTL_LE_EVENT_COMM_DISABLE)) {
		if (found) {
			xt_dup.ni_fail = PTL_NI_OK;
			make_target_event(&xt_dup, le->eq, PTL_EVENT_SEARCH, le->user_ptr, NULL);
		}
		else
			make_le_event(le, le->eq, PTL_EVENT_SEARCH, PTL_NI_NO_MATCH);
	}

	return PTL_OK;
}

/* Search for matching entries in the unexpected and delete them.
 */
int check_overflow_search_delete(le_t *le)
{
	xt_t *xt;
	xt_t *n;
	pt_t *pt = &le->obj.obj_ni->pt[le->pt_index];
	struct list_head xt_list;

	pthread_spin_lock(&pt->lock);

	match_le_unexpected(le, 0, &xt_list);

	pthread_spin_unlock(&pt->lock);

	if (list_empty(&xt_list)) {
			make_le_event(le, le->eq, PTL_EVENT_SEARCH, PTL_NI_NO_MATCH);
	} else {

		list_for_each_entry_safe(xt, n, &xt_list, unexpected_list) {
			int err;
			int state;

			pthread_spin_lock(&xt->obj.obj_lock);

			assert(xt->matching.le == NULL);
			xt->matching.le = le;
			le_get(le);

			list_del(&xt->unexpected_list);

			state = xt->state;

			/* tgt must release reference to any LE/ME */
			if (xt->le) {
				le_put(xt->le);
				xt->le = NULL;
			}

			pthread_spin_unlock(&xt->obj.obj_lock);

			if (state == STATE_TGT_WAIT_APPEND) {
				err = process_tgt(xt);
				if (err)
					WARN();
			}

			/* From get_match. */
			xt_put(xt);
		}
	}

	return PTL_NI_OK;
}
