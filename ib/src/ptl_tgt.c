#include "ptl_loc.h"

/*
 * tgt_state_name
 *	for debugging output
 */
static char *tgt_state_name[] = {
	[STATE_TGT_START]		= "tgt_start",
	[STATE_TGT_DROP]		= "tgt_drop",
	[STATE_TGT_GET_MATCH]		= "tgt_get_match",
	[STATE_TGT_GET_PERM]		= "tgt_get_perm",
	[STATE_TGT_GET_LENGTH]		= "tgt_get_length",
	[STATE_TGT_DATA_IN]		= "tgt_data_in",
	[STATE_TGT_RDMA]		= "tgt_rdma",
	[STATE_TGT_ATOMIC_DATA_IN]	= "tgt_atomic_data_in",
	[STATE_TGT_SWAP_DATA_IN]	= "tgt_swap_data_in",
	[STATE_TGT_DATA_OUT]		= "tgt_data_out",
	[STATE_TGT_RDMA_DESC]		= "tgt_rdma_desc",
	[STATE_TGT_UNLINK]	        = "tgt_unlink",
	[STATE_TGT_SEND_ACK]		= "tgt_send_ack",
	[STATE_TGT_SEND_REPLY]		= "tgt_send_reply",
	[STATE_TGT_COMM_EVENT]		= "tgt_comm_event",
	[STATE_TGT_CLEANUP]		= "tgt_cleanup",
	[STATE_TGT_ERROR]		= "tgt_error",
	[STATE_TGT_DONE]		= "tgt_done",
};

/*
 * make_comm_event
 */
int make_comm_event(xt_t *xt)
{
	int err = PTL_OK;
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

	if (xt->ni_fail || !(xt->le->options & PTL_LE_EVENT_SUCCESS_DISABLE)) {
		err = make_target_event(xt, xt->pt->eq, type, NULL);
		if (err)
			WARN();
	}

	xt->event_mask &= ~XT_COMM_EVENT;

	return err;
}

/*
 * make_ct_comm_event
 */
static void make_ct_comm_event(xt_t *xt)
{
	le_t *le = xt->le;
	int bytes = le->options & PTL_LE_EVENT_CT_BYTES;

	make_ct_event(le->ct, xt->ni_fail, xt->mlength, bytes);
}

/*
 * init_events
 *	decide whether comm eq/ct events will happen
 *	for this message
 */
static void init_events(xt_t *xt)
{
	if (xt->pt->eq && !(xt->le->options &
			    PTL_LE_EVENT_COMM_DISABLE))
		xt->event_mask |= XT_COMM_EVENT;

	if (xt->le->ct && (xt->le->options &
			   PTL_LE_EVENT_CT_COMM))
		xt->event_mask |= XT_CT_COMM_EVENT;

	switch (xt->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (xt->ack_req != PTL_NO_ACK_REQ)
			xt->event_mask |= XT_ACK_EVENT;
		break;
	case OP_GET:
	case OP_FETCH:
	case OP_SWAP:
		if (xt->ack_req != PTL_NO_ACK_REQ)
			xt->event_mask |= XT_REPLY_EVENT;
		break;
	}
}

/*
 * init_drop_events
 *	Set events for messages that are dropped.  xt->ni_fail
 * should be set to failure type.
 */
static void init_drop_events(xt_t *xt)
{
	switch (xt->operation) {
	case OP_PUT:
	case OP_ATOMIC:
		if (xt->ack_req != PTL_NO_ACK_REQ)
			xt->event_mask |= XT_ACK_EVENT;
		break;
	case OP_GET:
	case OP_FETCH:
	case OP_SWAP:
		if (xt->ack_req != PTL_NO_ACK_REQ)
			xt->event_mask |= XT_REPLY_EVENT;
		break;
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
		err = iov_copy_in(data, (ptl_iovec_t *)me->start,
				  me->num_iov, offset, length);
		if (err) {
			WARN();
			return STATE_TGT_ERROR;
		}
	} else
		memcpy(me->start + offset, data, length);

	return PTL_OK;
}

/*
 * atomic_in
 *	TODO have to do better on IOVEC boundaries
 */
static int atomic_in(xt_t *xt, me_t *me, void *data)
{
	int err;
	ptl_size_t offset = xt->moffset;
	ptl_size_t length = xt->mlength;
	atom_op_t op;

	op = atom_op[xt->atom_op][xt->atom_type];
	if (!op) {
		WARN();
		return STATE_TGT_ERROR;
	}

	if (me->num_iov) {
		err = iov_atomic_in(op, data, (ptl_iovec_t *)me->start,
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

/*
 * tgt_start
 *	get portals table entry from request
 */
static int tgt_start(xt_t *xt)
{
	ni_t *ni = to_ni(xt);

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
	pthread_spin_lock(&xt->pt->obj_lock);
	if (!xt->pt->enabled || xt->pt->disable) {
		pthread_spin_unlock(&xt->pt->obj_lock);
		xt->ni_fail = PTL_NI_DROPPED;
		return STATE_TGT_DROP;
	}
	xt->pt->num_xt_active++;
	pthread_spin_unlock(&xt->pt->obj_lock);

	return STATE_TGT_GET_MATCH;
}

/*
 * request_drop
 *	drop a request
 */
static int request_drop(xt_t *xt)
{
	/* logging ? */

	init_drop_events(xt);
	return STATE_TGT_COMM_EVENT;
}

static int check_match(xt_t *xt)
{
	ni_t *ni = to_ni(xt);
	ptl_size_t offset;
	ptl_size_t length;

	if (ni->options & PTL_NI_LOGICAL) {
		if (!(xt->me->id.rank == PTL_RANK_ANY ||
		     (xt->me->id.rank == xt->initiator.rank)))
			return 0;
	} else {
		if (!(xt->me->id.phys.nid == PTL_NID_ANY ||
		     (xt->me->id.phys.nid == xt->initiator.phys.nid)))
			return 0;
		if (!(xt->me->id.phys.pid == PTL_PID_ANY ||
		     (xt->me->id.phys.pid == xt->initiator.phys.pid)))
			return 0;
	}

	length = xt->rlength;
	offset = (xt->me->options & PTL_ME_MANAGE_LOCAL) ?
			xt->me->offset : xt->roffset;

	if ((xt->me->options & PTL_ME_NO_TRUNCATE) &&
	    ((offset + length) > xt->me->length))
			return 0;

	return (xt->match_bits | xt->me->ignore_bits) ==
		(xt->me->match_bits | xt->me->ignore_bits);
}

/*
 * tgt_get_match
 *	get matching entry from PT
 */
static int tgt_get_match(xt_t *xt)
{
	ni_t *ni = to_ni(xt);
	struct list_head *l;

	if (xt->pt->options & PTL_PT_FLOWCTRL) {
		if (list_empty(&xt->pt->priority_list) &&
		    list_empty(&xt->pt->overflow_list)) {
			WARN();
			pthread_spin_lock(&xt->pt->obj_lock);
			xt->pt->disable |= PT_AUTO_DISABLE;
			pthread_spin_unlock(&xt->pt->obj_lock);
			xt->ni_fail = PTL_NI_FLOW_CTRL;
			xt->le = NULL;
			return STATE_TGT_DROP;
		}
	}

	list_for_each(l, &xt->pt->priority_list) {
		xt->le = list_entry(l, le_t, list);
		if (ni->options & PTL_NI_NO_MATCHING) {
			le_ref(xt->le);
			goto done;
		}

		if (check_match(xt)) {
			me_ref((me_t *)xt->le);
			goto done;
		}
	}

	list_for_each(l, &xt->pt->overflow_list) {
		xt->le = list_entry(l, le_t, list);
		if (ni->options & PTL_NI_NO_MATCHING) {
			le_ref(xt->le);
			goto done;
		}

		if (check_match(xt)) {
			me_ref((me_t *)xt->le);
			goto done;
		}
	}

	WARN();
	xt->le = NULL;
	xt->ni_fail = PTL_NI_DROPPED;
	return STATE_TGT_DROP;

done:
	return STATE_TGT_GET_PERM;
}

/*
 * tgt_get_perm
 *	check permission on incoming request packet
 */
static int tgt_get_perm(xt_t *xt)
{
	/* just a handy place to do this
	 * nothing to do with permissions */
	init_events(xt);

	if (xt->le->options & PTL_ME_AUTH_USE_JID) {
		if (!(xt->le->jid == PTL_JID_ANY || (xt->le->jid == xt->jid))) {
			WARN();
			goto no_perm;
		}
		if (!(xt->le->uid == PTL_UID_ANY || (xt->le->uid == xt->uid))) {
			WARN();
			goto no_perm;
		}
	}

	switch (xt->operation) {
	case OP_ATOMIC:
	case OP_PUT:
		if (!(xt->le->options & PTL_ME_OP_PUT)) {
			WARN();
			goto no_perm;
		}
		break;

	case OP_GET:
		if (!(xt->le->options & PTL_ME_OP_GET)) {
			WARN();
			goto no_perm;
		}
		break;

	case OP_FETCH:
	case OP_SWAP:
		if ((xt->le->options & (PTL_ME_OP_PUT | PTL_ME_OP_GET))
		    != (PTL_ME_OP_PUT | PTL_ME_OP_GET)) {
			WARN();
			goto no_perm;
		}
		break;

	default:
		return STATE_TGT_ERROR;
	}

	return STATE_TGT_GET_LENGTH;

no_perm:
	xt->ni_fail = PTL_NI_PERM_VIOLATION;
	return STATE_TGT_DROP;
}

/*
 * tgt_get_length
 *	determine the data in/out transfer lengths
 */
static int tgt_get_length(xt_t *xt)
{
	ni_t *ni = to_ni(xt);
	me_t *me = xt->me;
	ptl_size_t room;
	ptl_size_t offset;
	ptl_size_t length;

	/* note le->options & PTL_ME_MANAGE_LOCAL is always zero */
	offset = (me->options & PTL_ME_MANAGE_LOCAL) ? me->offset : xt->roffset;
	room = me->length - offset;
	length = (room >= xt->rlength) ? xt->rlength : room;

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

	switch (xt->operation) {
	case OP_PUT:
		return STATE_TGT_DATA_IN;

	case OP_ATOMIC:
		return STATE_TGT_ATOMIC_DATA_IN;

	case OP_GET:
	case OP_FETCH:
	case OP_SWAP:
		return STATE_TGT_DATA_OUT;
		
	default:
		return STATE_TGT_ERROR;
	}
}

static void tgt_unlink(xt_t *xt)
{
	if (xt->me->type == TYPE_ME)
		me_unlink(xt->me);
	else
		le_unlink(xt->le);
}

static int tgt_alloc_rdma_buf(xt_t *xt)
{
	buf_t *buf;
	int err;

	if (debug)
		printf("tgt_alloc_rdma_buf\n");

	err = buf_alloc(to_ni(xt), &buf);
	if (err) {
		WARN();
		return err;
	}
	buf->type = BUF_RDMA;
	buf->xt = xt;
	buf->dest = &xt->dest;
	xt->rdma_buf = buf;

	return 0;
}

static void tgt_free_rdma_buf(xt_t *xt)
{
	if (xt->rdma_buf) {
		buf_put(xt->rdma_buf);
		xt->rdma_buf = NULL;
	}
}

static int tgt_data_out(xt_t *xt)
{
	me_t *me = xt->me;
	data_t *data = xt->data_out;
	int next;

	if (!data) {
		WARN();
		return STATE_TGT_ERROR;
	}

	switch (data->data_fmt) {
	case DATA_FMT_DMA:
		if (tgt_alloc_rdma_buf(xt))
			return STATE_TGT_ERROR;

		/* Write to SG list provided directly in request */
		xt->cur_rem_sge = &data->sge_list[0];
		xt->cur_rem_off = 0;
		xt->num_rem_sge = be32_to_cpu(data->num_sge);

		if (debug)
			printf("cur_rem_sge(%p), num_rem_sge(%d)\n",
				xt->cur_rem_sge, (int)xt->num_rem_sge);
		/*
		 * RDMA data from le/me  memory, determine starting vector
		 * and vector offset for le/me.
		 */
		xt->cur_loc_iov_index = 0;
		xt->cur_loc_iov_off = 0;

		if (debug)
			printf("me->num_iov(%d), xt->moffset(%d)\n",
				me->num_iov, (int)xt->moffset);

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
					printf("In loop: loc_offset(%d),"
 						"moffset(%d)\n",
						(int)loc_offset,
						(int)xt->moffset);
			}
			if (loc_offset < xt->moffset) {
				WARN();
				return STATE_TGT_ERROR;
			}

			xt->cur_loc_iov_index = i;
			xt->cur_loc_iov_off = iov_offset;
		} else {
			xt->cur_loc_iov_off = xt->moffset;
		}
		if (debug)
			printf("cur_loc_iov_index(%d), cur_loc_iov_off(%d)\n",
				(int)xt->cur_loc_iov_index,
				(int)xt->cur_loc_iov_off);

		xt->rdma_dir = DATA_DIR_OUT;
		next = STATE_TGT_RDMA;
		break;
	case DATA_FMT_INDIRECT:
		if (tgt_alloc_rdma_buf(xt))
			return STATE_TGT_ERROR;

		xt->rdma_dir = DATA_DIR_OUT;
		next = STATE_TGT_RDMA_DESC;
		break;
	default:
		WARN();
		return STATE_TGT_ERROR;
		break;
	}

	if (xt->operation == OP_FETCH) {
		if (xt->atom_op >= PTL_MIN && xt->atom_op <= PTL_BXOR) {
			return STATE_TGT_ATOMIC_DATA_IN;
		} else {
			WARN();
			return STATE_TGT_ERROR;
		}
	}

	if (xt->operation == OP_SWAP) {
		if (xt->atom_op == PTL_SWAP) {
			return STATE_TGT_DATA_IN;
		} else if (xt->atom_op >= PTL_CSWAP && xt->atom_op <= PTL_MSWAP) {
			return STATE_TGT_SWAP_DATA_IN;
		} else {
			WARN();
			return STATE_TGT_ERROR;
	
		}
	}

	/*
	 * If locally managed update to reserve space for the
	 * associated RDMA data.
	 */
	if (me->options & PTL_ME_MANAGE_LOCAL)
		me->offset += xt->mlength;

	/*
	 * Unlink if required to prevent further use of this 
	 * ME/LE.
	 */
	if (me->options & PTL_ME_USE_ONCE)
		tgt_unlink(xt);
	else if  ((me->options & PTL_ME_MANAGE_LOCAL) &&
	    ((me->length - me->offset) < me->min_free))
		tgt_unlink(xt);

	return next;
}

/* Start a connection if not connected already. Return -1 on error, 1
   if the connection is pending, and 0 if connected. */
static int check_conn(xt_t *xt)
{
	struct nid_connect *connect;
	int ret;
	ni_t *ni = to_ni(xt);

	/* Ensure we are already connected. */
	connect = get_connect_for_id(ni, &xt->initiator);
	if (unlikely(!connect)) {
		ptl_warn("Invalid destination\n");
		return -1;
	}

	pthread_mutex_lock(&connect->mutex);
	if (unlikely(connect->state != GBLN_CONNECTED)) {
		/* Not connected. Add the xi on the pending list. It will be
		 * flushed once connected/disconnected. */

		list_add_tail(&xt->connect_pending_list, &connect->xt_list);

		if (connect->state == GBLN_DISCONNECTED) {
			/* Initiate connection. */
			if (init_connect(ni, connect)) {
				list_del(&xt->connect_pending_list);
				ret = -1;
			} else
				ret = 1;
		} else {
			/* Connection in already in progress. */
			ret = 1;
		}

	} else {
		set_xt_dest(xt, connect);
		ret = 0;
	}

	pthread_mutex_unlock(&connect->mutex);

	return ret;
}


static int tgt_rdma(xt_t *xt)
{
	int err;
	ptl_size_t *resid = xt->rdma_dir == DATA_DIR_IN ?
		&xt->put_resid : &xt->get_resid;

	if (debug) printf("tgt_rdma - data_dir(%d), resid(%d), "
		"interim_rdma(%d), rdma_comp(%d)\n",
		(int) xt->rdma_dir, (int) xt->put_resid,
		 (int) xt->interim_rdma, (int) xt->rdma_comp);

	err = check_conn(xt);
	if (err == -1)
		return STATE_TGT_ERROR;
	else if (err == 1)
		/* Not connected yet. */
		return STATE_TGT_RDMA;

	/*
	 * Issue as many RDMA requests as we can.  We may have to take
	 * intermediate completions if we run out send queue resources;
	 * We will always take at least one on the last work request
	 * associated with a portals transfer.
	 */
	if (*resid) {
		if (debug)
			printf("tgt_rdma - post rdma\n");

		err = post_tgt_rdma(xt, xt->rdma_dir);
		if (err) {
			WARN();
			return STATE_TGT_ERROR;
		}
	}

	/*
	 * If all RDMA have been issued and there is not a completion
	 * outstanding advance state.
	 */
	if (!*resid && !xt->rdma_comp)
		return STATE_TGT_COMM_EVENT;

	return STATE_TGT_RDMA;
}

static int tgt_rdma_desc(xt_t *xt)
{
	data_t *data;
	uint64_t raddr;
	uint32_t rkey;
	uint32_t rlen;
	struct ibv_sge sge;
	int err;
	int next;

	err = check_conn(xt);
	if (err == -1)
		return STATE_TGT_ERROR;
	else if (err == 1)
		/* Not connected yet. */
		return STATE_TGT_RDMA_DESC;

	data = xt->rdma_dir == DATA_DIR_IN ? xt->data_in : xt->data_out;

	/*
	 * Allocate and map indirect buffer and setup to read
	 * descriptor list from initiator memory.
	 */
	raddr = be64_to_cpu(data->sge_list[0].addr);
	rkey = be32_to_cpu(data->sge_list[0].lkey);
	rlen = be32_to_cpu(data->sge_list[0].length);

	if (debug)
		printf("RDMA indirect descriptors:radd(0x%" PRIx64 "), "
		       " rkey(0x%x), len(%d)\n", raddr, rkey, rlen);

	xt->indir_sge = ptl_calloc(1, rlen);
	if (!xt->indir_sge) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}

	if (mr_lookup(to_ni(xt), xt->indir_sge, rlen,
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

	xt->rdma_comp++;
	err = rdma_read(xt->rdma_buf, raddr, rkey, &sge, 1, 1);
	if (err) {
		WARN();
		next = STATE_TGT_COMM_EVENT;
		goto done;
	}
	next = STATE_TGT_RDMA_WAIT_DESC;
done:
	return next;
}

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
	} else {
		xt->cur_loc_iov_off = xt->moffset;
	}

	if (debug)
		printf("cur_loc_iov_index(%d), cur_loc_iov_off(%d)\n",
			(int)xt->cur_loc_iov_index,
			(int)xt->cur_loc_iov_off);
	return PTL_OK;
}

static int tgt_rdma_wait_desc(xt_t *xt)
{
	data_t *data;

	data = xt->rdma_dir == DATA_DIR_IN ? xt->data_in : xt->data_out;

	xt->cur_rem_sge = xt->indir_sge;
	xt->cur_rem_off = 0;
	xt->num_rem_sge = (be32_to_cpu(data->sge_list[0].length)) /
			  sizeof(struct ibv_sge);

	if (debug)
		printf("Wait Desc:cur_rem_sge(%p), num_rem_sge(%d)\n",
			xt->cur_rem_sge, (int)xt->num_rem_sge);

	if (tgt_rdma_init_loc_off(xt)) 
		return STATE_TGT_ERROR;

	return STATE_TGT_RDMA;
}

static int tgt_data_in(xt_t *xt)
{
	int err;
	me_t *me = xt->me;
	data_t *data = xt->data_in;
	int next;

	switch (data->data_fmt) {
	case DATA_FMT_IMMEDIATE:
		err = copy_in(xt, me, data->data);
		if (err)
			return STATE_TGT_ERROR;
		next = STATE_TGT_COMM_EVENT;
		break;
	case DATA_FMT_DMA:
		if (tgt_alloc_rdma_buf(xt))
			return STATE_TGT_ERROR;

		/* Read from SG list provided directly in request */
		xt->cur_rem_sge = &data->sge_list[0];
		xt->cur_rem_off = 0;
		xt->num_rem_sge = be32_to_cpu(data->num_sge);

		if (debug)
			printf("cur_rem_sge(%p), num_rem_sge(%d)\n",
				xt->cur_rem_sge, (int)xt->num_rem_sge);

		if (tgt_rdma_init_loc_off(xt)) 
			return STATE_TGT_ERROR;

		xt->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_RDMA;
		break;
	case DATA_FMT_INDIRECT:
		if (tgt_alloc_rdma_buf(xt))
			return STATE_TGT_ERROR;

		xt->rdma_dir = DATA_DIR_IN;
		next = STATE_TGT_RDMA_DESC;
		break;
	default:
		WARN();
		next = STATE_TGT_ERROR;
	}

	/*
	 * If locally managed update to reserve space for the
	 * associated RDMA data.
	 */
	if (me->options & PTL_ME_MANAGE_LOCAL)
		me->offset += xt->mlength;

	/*
	 * Unlink if required to prevent further use of this 
	 * ME/LE.
	 */
	if (me->options & PTL_ME_USE_ONCE)
		tgt_unlink(xt);
	else if  ((me->options & PTL_ME_MANAGE_LOCAL) &&
	    ((me->length - me->offset) < me->min_free))
		tgt_unlink(xt);

	return next;
}

static int tgt_atomic_data_in(xt_t *xt)
{
	int err;
	data_t *data = xt->data_in;
	me_t *me = xt->me;

	/* assumes that max_atomic_size is <= MAX_INLINE_BYTES */
	if (data->data_fmt != DATA_FMT_IMMEDIATE) {
		WARN();
		return STATE_TGT_ERROR;
	}

	err = atomic_in(xt, me, data->data);
	if (err)
		return STATE_TGT_ERROR;

	if (me->options & PTL_ME_MANAGE_LOCAL)
		me->offset += xt->mlength;

	if (me->options & PTL_ME_USE_ONCE)
		return STATE_TGT_UNLINK;

	if ((me->options & PTL_ME_MANAGE_LOCAL) &&
	    ((me->length - me->offset) < me->min_free))
		return STATE_TGT_UNLINK;

	return STATE_TGT_COMM_EVENT;
}

static int tgt_swap_data_in(xt_t *xt)
{
	int err;
	data_t *data = xt->data_in;
	me_t *me = xt->me;
	datatype_t opr, src, dst, *d;

	opr.u64 = xt->operand;
	dst.u64 = 0;
	d = (union datatype *)data->data;

	/* assumes that max_atomic_size is <= MAX_INLINE_BYTES */
	if (data->data_fmt != DATA_FMT_IMMEDIATE) {
		WARN();
		return STATE_TGT_ERROR;
	}

	err = copy_out(xt, me, &src);
	if (err)
		return STATE_TGT_ERROR;

	switch (xt->atom_op) {
	case PTL_CSWAP:
		switch (xt->atom_type) {
		case PTL_CHAR:
			dst.s8 = (opr.s8 == src.s8) ? d->s8 : src.s8;
			break;
		case PTL_UCHAR:
			dst.u8 = (opr.u8 == src.u8) ? d->u8 : src.u8;
			break;
		case PTL_SHORT:
			dst.s16 = (opr.s16 == src.s16) ? d->s16 : src.s16;
			break;
		case PTL_USHORT:
			dst.u16 = (opr.u16 == src.u16) ? d->u16 : src.u16;
			break;
		case PTL_INT:
			dst.s32 = (opr.s32 == src.s32) ? d->s32 : src.s32;
			break;
		case PTL_UINT:
			dst.u32 = (opr.u32 == src.u32) ? d->u32 : src.u32;
			break;
		case PTL_LONG:
			dst.s64 = (opr.s64 == src.s64) ? d->s64 : src.s64;
			break;
		case PTL_ULONG:
			dst.u64 = (opr.u64 == src.u64) ? d->u64 : src.u64;
			break;
		case PTL_FLOAT:
			dst.f = (opr.f == src.f) ? d->f : src.f;
			break;
		case PTL_DOUBLE:
			dst.d = (opr.d == src.d) ? d->d : src.d;
			break;
		default:
			return STATE_TGT_ERROR;
		}
		break;
	case PTL_CSWAP_NE:
		switch (xt->atom_type) {
		case PTL_CHAR:
			dst.s8 = (opr.s8 != src.s8) ? d->s8 : src.s8;
			break;
		case PTL_UCHAR:
			dst.u8 = (opr.u8 != src.u8) ? d->u8 : src.u8;
			break;
		case PTL_SHORT:
			dst.s16 = (opr.s16 != src.s16) ? d->s16 : src.s16;
			break;
		case PTL_USHORT:
			dst.u16 = (opr.u16 != src.u16) ? d->u16 : src.u16;
			break;
		case PTL_INT:
			dst.s32 = (opr.s32 != src.s32) ? d->s32 : src.s32;
			break;
		case PTL_UINT:
			dst.u32 = (opr.u32 != src.u32) ? d->u32 : src.u32;
			break;
		case PTL_LONG:
			dst.s64 = (opr.s64 != src.s64) ? d->s64 : src.s64;
			break;
		case PTL_ULONG:
			dst.u64 = (opr.u64 != src.u64) ? d->u64 : src.u64;
			break;
		case PTL_FLOAT:
			dst.f = (opr.f != src.f) ? d->f : src.f;
			break;
		case PTL_DOUBLE:
			dst.d = (opr.d != src.d) ? d->d : src.d;
			break;
		default:
			return STATE_TGT_ERROR;
		}
		break;
	case PTL_CSWAP_LE:
		switch (xt->atom_type) {
		case PTL_CHAR:
			dst.s8 = (opr.s8 <= src.s8) ? d->s8 : src.s8;
			break;
		case PTL_UCHAR:
			dst.u8 = (opr.u8 <= src.u8) ? d->u8 : src.u8;
			break;
		case PTL_SHORT:
			dst.s16 = (opr.s16 <= src.s16) ? d->s16 : src.s16;
			break;
		case PTL_USHORT:
			dst.u16 = (opr.u16 <= src.u16) ? d->u16 : src.u16;
			break;
		case PTL_INT:
			dst.s32 = (opr.s32 <= src.s32) ? d->s32 : src.s32;
			break;
		case PTL_UINT:
			dst.u32 = (opr.u32 <= src.u32) ? d->u32 : src.u32;
			break;
		case PTL_LONG:
			dst.s64 = (opr.s64 <= src.s64) ? d->s64 : src.s64;
			break;
		case PTL_ULONG:
			dst.u64 = (opr.u64 <= src.u64) ? d->u64 : src.u64;
			break;
		case PTL_FLOAT:
			dst.f = (opr.f <= src.f) ? d->f : src.f;
			break;
		case PTL_DOUBLE:
			dst.d = (opr.d <= src.d) ? d->d : src.d;
			break;
		default:
			return STATE_TGT_ERROR;
		}
		break;
	case PTL_CSWAP_LT:
		switch (xt->atom_type) {
		case PTL_CHAR:
			dst.s8 = (opr.s8 < src.s8) ? d->s8 : src.s8;
			break;
		case PTL_UCHAR:
			dst.u8 = (opr.u8 < src.u8) ? d->u8 : src.u8;
			break;
		case PTL_SHORT:
			dst.s16 = (opr.s16 < src.s16) ? d->s16 : src.s16;
			break;
		case PTL_USHORT:
			dst.u16 = (opr.u16 < src.u16) ? d->u16 : src.u16;
			break;
		case PTL_INT:
			dst.s32 = (opr.s32 < src.s32) ? d->s32 : src.s32;
			break;
		case PTL_UINT:
			dst.u32 = (opr.u32 < src.u32) ? d->u32 : src.u32;
			break;
		case PTL_LONG:
			dst.s64 = (opr.s64 < src.s64) ? d->s64 : src.s64;
			break;
		case PTL_ULONG:
			dst.u64 = (opr.u64 < src.u64) ? d->u64 : src.u64;
			break;
		case PTL_FLOAT:
			dst.f = (opr.f < src.f) ? d->f : src.f;
			break;
		case PTL_DOUBLE:
			dst.d = (opr.d < src.d) ? d->d : src.d;
			break;
		default:
			return STATE_TGT_ERROR;
		}
		break;
	case PTL_CSWAP_GE:
		switch (xt->atom_type) {
		case PTL_CHAR:
			dst.s8 = (opr.s8 >= src.s8) ? d->s8 : src.s8;
			break;
		case PTL_UCHAR:
			dst.u8 = (opr.u8 >= src.u8) ? d->u8 : src.u8;
			break;
		case PTL_SHORT:
			dst.s16 = (opr.s16 >= src.s16) ? d->s16 : src.s16;
			break;
		case PTL_USHORT:
			dst.u16 = (opr.u16 >= src.u16) ? d->u16 : src.u16;
			break;
		case PTL_INT:
			dst.s32 = (opr.s32 >= src.s32) ? d->s32 : src.s32;
			break;
		case PTL_UINT:
			dst.u32 = (opr.u32 >= src.u32) ? d->u32 : src.u32;
			break;
		case PTL_LONG:
			dst.s64 = (opr.s64 >= src.s64) ? d->s64 : src.s64;
			break;
		case PTL_ULONG:
			dst.u64 = (opr.u64 >= src.u64) ? d->u64 : src.u64;
			break;
		case PTL_FLOAT:
			dst.f = (opr.f >= src.f) ? d->f : src.f;
			break;
		case PTL_DOUBLE:
			dst.d = (opr.d >= src.d) ? d->d : src.d;
			break;
		default:
			return STATE_TGT_ERROR;
		}
		break;
	case PTL_CSWAP_GT:
		switch (xt->atom_type) {
		case PTL_CHAR:
			dst.s8 = (opr.s8 > src.s8) ? d->s8 : src.s8;
			break;
		case PTL_UCHAR:
			dst.u8 = (opr.u8 > src.u8) ? d->u8 : src.u8;
			break;
		case PTL_SHORT:
			dst.s16 = (opr.s16 > src.s16) ? d->s16 : src.s16;
			break;
		case PTL_USHORT:
			dst.u16 = (opr.u16 > src.u16) ? d->u16 : src.u16;
			break;
		case PTL_INT:
			dst.s32 = (opr.s32 > src.s32) ? d->s32 : src.s32;
			break;
		case PTL_UINT:
			dst.u32 = (opr.u32 > src.u32) ? d->u32 : src.u32;
			break;
		case PTL_LONG:
			dst.s64 = (opr.s64 > src.s64) ? d->s64 : src.s64;
			break;
		case PTL_ULONG:
			dst.u64 = (opr.u64 > src.u64) ? d->u64 : src.u64;
			break;
		case PTL_FLOAT:
			dst.f = (opr.f > src.f) ? d->f : src.f;
			break;
		case PTL_DOUBLE:
			dst.d = (opr.d > src.d) ? d->d : src.d;
			break;
		default:
			return STATE_TGT_ERROR;
		}
		break;
	case PTL_MSWAP:
		switch (xt->atom_type) {
		case PTL_CHAR:
		case PTL_UCHAR:
			dst.u8 = (opr.u8 & d->u8) | (~opr.u8 & src.u8);
			break;
		case PTL_SHORT:
		case PTL_USHORT:
			dst.u16 = (opr.u16 & d->u16) | (~opr.u16 & src.u16);
			break;
		case PTL_INT:
		case PTL_UINT:
		case PTL_FLOAT:
			dst.u32 = (opr.u32 & d->u32) | (~opr.u32 & src.u32);
			break;
		case PTL_LONG:
		case PTL_ULONG:
		case PTL_DOUBLE:
			dst.u64 = (opr.u64 & d->u64) | (~opr.u64 & src.u64);
			break;
		default:
			return STATE_TGT_ERROR;
		}
		break;
	default:
		return STATE_TGT_ERROR;
	}

	err = copy_in(xt, me, &dst);
	if (err)
		return STATE_TGT_ERROR;

	if (me->options & PTL_ME_MANAGE_LOCAL)
		me->offset += xt->mlength;

	if (me->options & PTL_ME_USE_ONCE)
		return STATE_TGT_UNLINK;

	if ((me->options & PTL_ME_MANAGE_LOCAL) &&
	    ((me->length - me->offset) < me->min_free))
		return STATE_TGT_UNLINK;

	return STATE_TGT_COMM_EVENT;
}

int tgt_comm_event(xt_t *xt)
{
	int err = PTL_OK;

	if (debug)
		printf("tgt_comm_event\n");

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
	ni_t *ni = to_ni(xt);
	buf_t *buf;
	hdr_t *hdr;

	err = check_conn(xt);
	if (err == -1)
		return STATE_TGT_ERROR;
	else if (err == 1)
		/* Not connected yet. */
		return STATE_TGT_SEND_ACK;

	xt->event_mask &= ~XT_ACK_EVENT;

	err = buf_alloc(ni, &buf);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	buf->xt = xt;
	buf->dest = &xt->dest;
	xt->send_buf = buf;

	hdr = (hdr_t *)buf->data;

	xport_hdr_from_xt(hdr, xt);
	base_hdr_from_xt(hdr, xt);

	switch (xt->ack_req) {
	case PTL_NO_ACK_REQ:
		WARN();
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
		return STATE_TGT_ERROR;
	}

	buf->length = sizeof(*hdr);

	if (debug) buf_dump(buf);

	err = send_message(buf);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	return STATE_TGT_CLEANUP;
}

static int tgt_send_reply(xt_t *xt)
{
	int err;
	ni_t *ni = to_ni(xt);
	buf_t *buf;
	hdr_t *hdr;

	err = check_conn(xt);
	if (err == -1)
		return STATE_TGT_ERROR;
	else if (err == 1)
		/* Not connected yet. */
		return STATE_TGT_SEND_REPLY;

	xt->event_mask &= ~XT_REPLY_EVENT;

	err = buf_alloc(ni, &buf);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	buf->xt = xt;
	buf->dest = &xt->dest;
	xt->send_buf = buf;

	hdr = (hdr_t *)buf->data;

	xport_hdr_from_xt(hdr, xt);
	base_hdr_from_xt(hdr, xt);

	hdr->operation = OP_REPLY;
	buf->length = sizeof(*hdr);

	err = send_message(buf);
	if (err) {
		WARN();
		return STATE_TGT_ERROR;
	}

	return STATE_TGT_CLEANUP;
}

static int tgt_cleanup(xt_t *xt)
{
	/* tgt must release reference to any LE/ME */
	if (xt->le) {
		if (xt->le->type == TYPE_ME)
			me_put(xt->me);
		else
			le_put(xt->le);
		xt->le = NULL;
	}

	/* tgt must release RDMA acquired resources */
	tgt_free_rdma_buf(xt);

	if (xt->indir_sge) {
		if (xt->indir_mr) {
			mr_put(xt->indir_mr);
			xt->indir_mr = NULL;
		}
		free(xt->indir_sge);
		xt->indir_sge = NULL;
	}
	
	/* tgt responsible to cleanup all received buffers */
	if (xt->recv_buf) {
		if (debug)
			printf("cleanup recv buf %p\n", xt->recv_buf);
		buf_put(xt->recv_buf);
		xt->recv_buf = NULL;
	}

	pthread_spin_lock(&xt->pt->obj_lock);
	xt->pt->num_xt_active--;
	if ((xt->pt->disable & PT_AUTO_DISABLE) && !xt->pt->num_xt_active) {
		xt->pt->enabled = 0;
		xt->pt->disable &= ~PT_AUTO_DISABLE;
		pthread_spin_unlock(&xt->pt->obj_lock);
		if (make_target_event(xt, xt->pt->eq,
				      PTL_EVENT_PT_DISABLED, NULL))
			WARN();
		
	} else
		pthread_spin_unlock(&xt->pt->obj_lock);

	xt_put(xt);
	return STATE_TGT_DONE;
}

/*
 * process_tgt
 *	process incoming request message
 */
int process_tgt(xt_t *xt)
{
	int err = PTL_OK;
	int state;
	ni_t *ni = to_ni(xt);

	if(debug)
		printf("process_tgt: called xt = %p\n", xt);

	xt->state_again = 1;

	do {
		err = pthread_spin_trylock(&xt->state_lock);
		if (err) {
			if (err == EBUSY) {
				return PTL_OK;
			} else {
				WARN();
				return PTL_FAIL;
			}
		}

		xt->state_again = 0;

		if (xt->state_waiting) {
			if (debug)
				printf("remove from xt_wait_list\n");
			pthread_spin_lock(&ni->xt_wait_list_lock);
			list_del(&xt->list);
			pthread_spin_unlock(&ni->xt_wait_list_lock);
			xt->state_waiting = 0;
		}

		state = xt->state;

		while(1) {
			if (debug)
				printf("tgt state = %s\n",
					tgt_state_name[state]);
			switch (state) {
			case STATE_TGT_START:
				state = tgt_start(xt);
				break;
			case STATE_TGT_GET_MATCH:
				state = tgt_get_match(xt);
				break;
			case STATE_TGT_GET_PERM:
				state = tgt_get_perm(xt);
				break;
			case STATE_TGT_GET_LENGTH:
				state = tgt_get_length(xt);
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
			case STATE_TGT_UNLINK:
				tgt_unlink(xt);
				state = STATE_TGT_COMM_EVENT;
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
			case STATE_TGT_CLEANUP:
				state = tgt_cleanup(xt);
				break;
			case STATE_TGT_ERROR:
				tgt_cleanup(xt);
				err = PTL_FAIL;
				goto exit;
			case STATE_TGT_DONE:
				goto exit;
			}
		}

exit:
		xt->state = state;
		pthread_spin_unlock(&xt->state_lock);
	} while(xt->state_again);

	return err;
}
