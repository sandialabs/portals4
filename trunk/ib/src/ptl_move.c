/*
 * ptl_move.c - get/put/atomic/fetch_atomic/swap APIs
 */

#include "ptl_loc.h"

static struct atom_op_info {
	int		float_ok;
	int		complex_ok;
	int		atomic_ok;
	int		swap_ok;
	int		use_operand;
} op_info[] = {
	[PTL_MIN]	= {	1,	0,	1,	0,	0, },
	[PTL_MAX]	= {	1,	0,	1,	0,	0, },
	[PTL_SUM]	= {	1,	1,	1,	0,	0, },
	[PTL_PROD]	= {	1,	1,	1,	0,	0, },
	[PTL_LOR]	= {	0,	0,	1,	0,	0, },
	[PTL_LAND]	= {	0,	0,	1,	0,	0, },
	[PTL_BOR]	= {	0,	0,	1,	0,	0, },
	[PTL_BAND]	= {	0,	0,	1,	0,	0, },
	[PTL_LXOR]	= {	0,	0,	1,	0,	0, },
	[PTL_BXOR]	= {	0,	0,	1,	0,	0, },
	[PTL_SWAP]	= {	1,	1,	0,	1,	0, },
	[PTL_CSWAP]	= {	1,	1,	0,	1,	1, },
	[PTL_CSWAP_NE]	= {	1,	1,	0,	1,	1, },
	[PTL_CSWAP_LE]	= {	1,	0,	0,	1,	1, },
	[PTL_CSWAP_LT]	= {	1,	0,	0,	1,	1, },
	[PTL_CSWAP_GE]	= {	1,	0,	0,	1,	1, },
	[PTL_CSWAP_GT]	= {	1,	0,	0,	1,	1, },
	[PTL_MSWAP]	= {	0,	0,	0,	1,	1, },
};

int atom_type_size[] = 
{
	[PTL_INT8_T]		= 1,
	[PTL_UINT8_T]		= 1,
	[PTL_INT16_T]		= 2,
	[PTL_UINT16_T]		= 2,
	[PTL_INT32_T]		= 4,
	[PTL_UINT32_T]		= 4,
	[PTL_INT64_T]		= 8,
	[PTL_UINT64_T]		= 8,
	[PTL_FLOAT]		= 4,
	[PTL_FLOAT_COMPLEX]	= 8,
	[PTL_DOUBLE]		= 8,
	[PTL_DOUBLE_COMPLEX]	= 16,
};

static int get_operand(ptl_datatype_t type, void *operand, uint64_t *opval)
{
	uint64_t val;
	int len = atom_type_size[type];

	switch(len) {
	case 1:
		val = *(uint8_t *)operand;
		break;
	case 2:
		val = *(uint16_t *)operand;
		break;
	case 4:
		val = *(uint32_t *)operand;
		break;
	case 8:
		val = *(uint64_t *)operand;
		break;
	case 16:
		/* TODO need to handle double complex case */
		WARN();
		val = -1ULL;
		break;
	default:
		ptl_error("invalid datatype = %d\n", type);
		val = -1ULL;
		break;
	}

	*opval = val;
	return PTL_OK;
}

#ifndef NO_ARG_VALIDATION
static int check_put(md_t *md, ptl_size_t local_offset, ptl_size_t length,
	      ptl_ack_req_t ack_req, ni_t *ni)
{
	if (unlikely(!md)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(local_offset + length > md->length)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(ack_req < PTL_NO_ACK_REQ || ack_req > PTL_OC_ACK_REQ)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(ack_req == PTL_ACK_REQ && !md->eq)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(ack_req == PTL_CT_ACK_REQ && !md->ct)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(length > ni->limits.max_msg_size)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	return PTL_OK;
}
#endif

int PtlPut(ptl_handle_md_t md_handle, ptl_size_t local_offset,
	   ptl_size_t length, ptl_ack_req_t ack_req, ptl_process_t target_id,
	   ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
	   ptl_size_t remote_offset, void *user_ptr, ptl_hdr_data_t hdr_data)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = md_get(md_handle, &md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	ni = to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_put(md, local_offset, length, ack_req, ni);
	if (err)
		goto err2;
#endif

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	xi->operation = OP_PUT;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->ack_req = ack_req;
	xi->put_md = md;
	xi->put_eq = md->eq;
	xi->put_ct = md->ct;
	xi->hdr_data = hdr_data;
	xi->user_ptr = user_ptr;
	xi->threshold = 0;

	xi->rlength = length;
	xi->put_offset = local_offset;
	xi->put_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	process_init(xi);

	gbl_put(gbl);
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlTriggeredPut(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		    ptl_size_t length, ptl_ack_req_t ack_req,
		    ptl_process_t target_id, ptl_pt_index_t pt_index,
		    ptl_match_bits_t match_bits, ptl_size_t remote_offset,
		    void *user_ptr, ptl_hdr_data_t hdr_data,
		    ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	ct_t *ct = NULL;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = md_get(md_handle, &md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	ni = to_ni(md);

	err = ct_get(trig_ct_handle, &ct);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

#ifndef NO_ARG_VALIDATION
	if (unlikely(!ct)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	err = check_put(md, local_offset, length, ack_req, ni);
	if (err)
		goto err3;
#endif

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	xi->operation = OP_PUT;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->ack_req = ack_req;
	xi->put_md = md;
	xi->put_eq = md->eq;
	xi->put_ct = md->ct;
	xi->hdr_data = hdr_data;
	xi->user_ptr = user_ptr;
	xi->threshold = threshold;

	xi->rlength = length;
	xi->put_offset = local_offset;
	xi->put_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	post_ct(xi, ct);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err3:
	ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

#ifndef NO_ARG_VALIDATION
static int check_get(md_t *md, ptl_size_t local_offset, ptl_size_t length,
	      ni_t *ni)
{
	if (unlikely(!md)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(local_offset + length > md->length)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(length > ni->limits.max_msg_size)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	return PTL_OK;
}
#endif

static inline void preparePtlGet(xi_t *xi, ni_t *ni, md_t *md,
								 ptl_size_t local_offset,
								 ptl_size_t length, ptl_process_t target_id,
								 ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
								 ptl_size_t remote_offset, void *user_ptr)
{
	xi->operation = OP_GET;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->get_md = md;
	xi->get_eq = md->eq;
	xi->get_ct = md->ct;
	xi->user_ptr = user_ptr;

	xi->rlength = length;
	xi->get_offset = local_offset;
	xi->get_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;
}

int PtlGet(ptl_handle_md_t md_handle, ptl_size_t local_offset,
	   ptl_size_t length, ptl_process_t target_id,
	   ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
	   ptl_size_t remote_offset, void *user_ptr)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = md_get(md_handle, &md);
	if (unlikely(err))
		goto err1;

	ni = to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_get(md, local_offset, length, ni);
	if (err)
		goto err2;
#endif

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	preparePtlGet(xi, ni, md, local_offset,
				  length, target_id,
				  pt_index, match_bits,
				  remote_offset, user_ptr);

	process_init(xi);

	gbl_put(gbl);
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlTriggeredGet(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		    ptl_size_t length, ptl_process_t target_id,
		    ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
		    ptl_size_t remote_offset, void *user_ptr,
		    ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	ct_t *ct = NULL;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = md_get(md_handle, &md);
	if (unlikely(err))
		goto err1;

	ni = to_ni(md);

	err = ct_get(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err2;

#ifndef NO_ARG_VALIDATION
	if (unlikely(!ct)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	err = check_get(md, local_offset, length, ni);
	if (err)
		goto err3;
#endif

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	preparePtlGet(xi, ni, md, local_offset,
				  length, target_id,
				  pt_index, match_bits,
				  remote_offset, user_ptr);

	xi->threshold = threshold;

	post_ct(xi, ct);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err3:
	ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

#ifndef NO_ARG_VALIDATION
static int check_atomic(md_t *md, ptl_size_t local_offset, ptl_size_t length,
			ni_t *ni, ptl_ack_req_t ack_req,
			ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	if (unlikely(!md)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(local_offset + length > md->length)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(length > ni->limits.max_atomic_size)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(ack_req < PTL_NO_ACK_REQ || ack_req > PTL_OC_ACK_REQ)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(ack_req == PTL_ACK_REQ && !md->eq)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(ack_req == PTL_CT_ACK_REQ && !md->ct)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(atom_op < PTL_MIN || atom_op >= PTL_OP_LAST)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(!op_info[atom_op].atomic_ok)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(atom_type < PTL_INT8_T || atom_type >= PTL_DATATYPE_LAST)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely((atom_type == PTL_FLOAT ||
		      atom_type == PTL_DOUBLE) &&
		      !op_info[atom_op].float_ok)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely((atom_type == PTL_FLOAT_COMPLEX ||
		      atom_type == PTL_DOUBLE_COMPLEX) &&
		      !op_info[atom_op].complex_ok)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	return PTL_OK;
}

static int check_overlap(md_t *get_md, ptl_size_t local_get_offset,
						 md_t *put_md, ptl_size_t local_put_offset,
						 ptl_size_t length)
{
	unsigned char *get_start = get_md->start + local_get_offset;
	unsigned char *put_start = put_md->start + local_put_offset;

	if (get_start >= put_start &&
		get_start < put_start + length) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (get_start + length >= put_start &&
		get_start + length < put_start + length) {
		WARN();
		return PTL_ARG_INVALID;
	}

	return PTL_OK;
}
#endif

int PtlAtomic(ptl_handle_md_t md_handle, ptl_size_t local_offset,
	      ptl_size_t length, ptl_ack_req_t ack_req,
	      ptl_process_t target_id, ptl_pt_index_t pt_index,
	      ptl_match_bits_t match_bits, ptl_size_t remote_offset,
	      void *user_ptr, ptl_hdr_data_t hdr_data,
	      ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = md_get(md_handle, &md);
	if (unlikely(err))
		goto err1;

	ni = to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_atomic(md, local_offset, length, ni, ack_req, atom_op, atom_type);
	if (err)
		goto err2;
#endif

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	xi->operation = OP_ATOMIC;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->ack_req = ack_req;
	xi->put_md = md;
	xi->put_eq = md->eq;
	xi->put_ct = md->ct;
	xi->hdr_data = hdr_data;
	xi->user_ptr = user_ptr;
	xi->atom_op = atom_op;
	xi->atom_type = atom_type;

	xi->rlength = length;
	xi->put_offset = local_offset;
	xi->put_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	process_init(xi);

	gbl_put(gbl);
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlTriggeredAtomic(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		       ptl_size_t length, ptl_ack_req_t ack_req,
		       ptl_process_t target_id, ptl_pt_index_t pt_index,
		       ptl_match_bits_t match_bits, ptl_size_t remote_offset,
		       void *user_ptr, ptl_hdr_data_t hdr_data,
		       ptl_op_t atom_op, ptl_datatype_t atom_type,
		       ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	ct_t *ct = NULL;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = md_get(md_handle, &md);
	if (unlikely(err))
		goto err1;

	ni = to_ni(md);

	err = ct_get(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err2;

#ifndef NO_ARG_VALIDATION
	if (unlikely(!ct)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	err = check_atomic(md, local_offset, length, ni, ack_req, atom_op, atom_type);
	if (err)
		goto err3;
#endif

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	xi->operation = OP_ATOMIC;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->ack_req = ack_req;
	xi->put_md = md;
	xi->put_eq = md->eq;
	xi->put_ct = md->ct;
	xi->hdr_data = hdr_data;
	xi->user_ptr = user_ptr;
	xi->atom_op = atom_op;
	xi->atom_type = atom_type;
	xi->threshold = threshold;

	xi->rlength = length;
	xi->put_offset = local_offset;
	xi->put_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	post_ct(xi, ct);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err3:
	ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlFetchAtomic(ptl_handle_md_t get_md_handle, ptl_size_t local_get_offset,
		   ptl_handle_md_t put_md_handle, ptl_size_t local_put_offset,
		   ptl_size_t length, ptl_process_t target_id,
		   ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
		   ptl_size_t remote_offset, void *user_ptr,
		   ptl_hdr_data_t hdr_data, ptl_op_t atom_op,
		   ptl_datatype_t atom_type)
{
	int err;
	gbl_t *gbl;
	md_t *get_md;
	md_t *put_md = NULL;
	ni_t *ni;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = md_get(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	err = md_get(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	ni = to_ni(get_md);

#ifndef NO_ARG_VALIDATION
	err = check_get(get_md, local_get_offset, length, ni);
	if (err)
		goto err3;

	err = check_atomic(put_md, local_put_offset, length, ni,
			   PTL_NO_ACK_REQ, atom_op, atom_type);
	if (err)
		goto err3;

	err = check_overlap(get_md, local_get_offset,
						put_md, local_put_offset, length);
	if (err)
		goto err3;

	if (unlikely(to_ni(put_md) != ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}
#endif

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	xi->operation = OP_FETCH;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->put_md = put_md;
	xi->put_eq = put_md->eq;
	xi->put_ct = put_md->ct;
	xi->get_md = get_md;
	xi->get_eq = get_md->eq;
	xi->get_ct = get_md->ct;
	xi->rlength = length;
	xi->hdr_data = hdr_data;
	xi->user_ptr = user_ptr;
	xi->atom_op = atom_op;
	xi->atom_type = atom_type;

	xi->rlength = length;
	xi->put_offset = local_put_offset;
	xi->put_resid = length;
	xi->get_offset = local_get_offset;
	xi->get_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	process_init(xi);

	gbl_put(gbl);
	return PTL_OK;

err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlTriggeredFetchAtomic(ptl_handle_md_t get_md_handle,
			    ptl_size_t local_get_offset,
			    ptl_handle_md_t put_md_handle,
			    ptl_size_t local_put_offset, ptl_size_t length,
			    ptl_process_t target_id, ptl_pt_index_t pt_index,
			    ptl_match_bits_t match_bits,
			    ptl_size_t remote_offset, void *user_ptr,
			    ptl_hdr_data_t hdr_data, ptl_op_t atom_op,
			    ptl_datatype_t atom_type,
			    ptl_handle_ct_t trig_ct_handle,
			    ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *get_md;
	md_t *put_md = NULL;
	ni_t *ni;
	ct_t *ct = NULL;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = md_get(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	err = md_get(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	ni = to_ni(get_md);

	err = ct_get(trig_ct_handle, &ct);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

#ifndef NO_ARG_VALIDATION
	if (unlikely(!ct)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = check_get(get_md, local_get_offset, length, ni);
	if (err)
		goto err4;

	err = check_atomic(put_md, local_put_offset, length, ni,
			   PTL_NO_ACK_REQ, atom_op, atom_type);
	if (err)
		goto err4;

	err = check_overlap(get_md, local_get_offset,
						put_md, local_put_offset, length);
	if (err)
		goto err4;

	if (unlikely(to_ni(put_md) != ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err4;
	}
#endif

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err4;
	}

	xi->operation = OP_FETCH;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->put_md = put_md;
	xi->put_eq = put_md->eq;
	xi->put_ct = put_md->ct;
	xi->get_md = get_md;
	xi->get_eq = get_md->eq;
	xi->get_ct = get_md->ct;
	xi->rlength = length;
	xi->hdr_data = hdr_data;
	xi->user_ptr = user_ptr;
	xi->atom_op = atom_op;
	xi->atom_type = atom_type;
	xi->threshold = threshold;

	xi->rlength = length;
	xi->put_offset = local_put_offset;
	xi->put_resid = length;
	xi->get_offset = local_get_offset;
	xi->get_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	post_ct(xi, ct);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err4:
	ct_put(ct);
err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlAtomicSync(void)
{
	int err;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	/* TODO */
	err = PTL_OK;

	gbl_put(gbl);
	return err;
}

#ifndef NO_ARG_VALIDATION
static int check_swap(md_t *get_md, ptl_size_t local_get_offset,
					  md_t *put_md, ptl_size_t local_put_offset,
					  ptl_size_t length, ni_t *ni,
					  ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	if (unlikely(!get_md)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(!put_md)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(get_md->obj.obj_ni != put_md->obj.obj_ni)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(local_get_offset + length > get_md->length)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(local_put_offset + length > put_md->length)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(length > ni->limits.max_atomic_size)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(atom_op < PTL_MIN || atom_op >= PTL_OP_LAST)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(!op_info[atom_op].swap_ok)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(atom_type < PTL_INT8_T || atom_type >= PTL_DATATYPE_LAST)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely((atom_type == PTL_FLOAT ||
		      atom_type == PTL_DOUBLE) &&
		      !op_info[atom_op].float_ok)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely((atom_type == PTL_FLOAT_COMPLEX ||
		      atom_type == PTL_DOUBLE_COMPLEX) &&
		      !op_info[atom_op].complex_ok)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (unlikely(op_info[atom_op].use_operand && 
	    length > atom_type_size[atom_type])) {
		WARN();
		return PTL_ARG_INVALID;
	}

	return PTL_OK;
}
#endif

int PtlSwap(ptl_handle_md_t get_md_handle, ptl_size_t local_get_offset,
	    ptl_handle_md_t put_md_handle, ptl_size_t local_put_offset,
	    ptl_size_t length, ptl_process_t target_id,
	    ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
	    ptl_size_t remote_offset, void *user_ptr,
	    ptl_hdr_data_t hdr_data, void *operand,
	    ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	int err;
	gbl_t *gbl;
	md_t *get_md;
	md_t *put_md = NULL;
	ni_t *ni;
	xi_t *xi;
	uint64_t opval = 0;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = md_get(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	err = md_get(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	ni = to_ni(get_md);

#ifndef NO_ARG_VALIDATION
	err = check_get(get_md, local_get_offset, length, ni);
	if (err)
		goto err3;

	err = check_swap(get_md, local_get_offset,
					 put_md, local_put_offset, 
					 length, ni, atom_op, atom_type);
	if (err)
		goto err3;

	err = check_overlap(get_md, local_get_offset,
						put_md, local_put_offset, length);
	if (err)
		goto err3;

	if (unlikely(to_ni(put_md) != ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}
#endif

	if (op_info[atom_op].use_operand) {
		err = get_operand(atom_type, operand, &opval);
		if (unlikely(err)) {
			WARN();
			goto err3;
		}
	}

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	xi->operation = OP_SWAP;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->put_md = put_md;
	xi->put_eq = put_md->eq;
	xi->put_ct = put_md->ct;
	xi->get_md = get_md;
	xi->get_eq = get_md->eq;
	xi->get_ct = get_md->ct;
	xi->hdr_data = hdr_data;
	xi->operand = opval;
	xi->user_ptr = user_ptr;
	xi->atom_op = atom_op;
	xi->atom_type = atom_type;

	xi->rlength = length;
	xi->put_offset = local_put_offset;
	xi->put_resid = length;
	xi->get_offset = local_get_offset;
	xi->get_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	process_init(xi);

	gbl_put(gbl);
	return PTL_OK;

err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlTriggeredSwap(ptl_handle_md_t get_md_handle, ptl_size_t local_get_offset,
		     ptl_handle_md_t put_md_handle, ptl_size_t local_put_offset,
		     ptl_size_t length, ptl_process_t target_id,
		     ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
		     ptl_size_t remote_offset, void *user_ptr,
		     ptl_hdr_data_t hdr_data, void *operand,
		     ptl_op_t atom_op, ptl_datatype_t atom_type,
		     ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *get_md;
	md_t *put_md = NULL;
	ni_t *ni;
	ct_t *ct = NULL;
	xi_t *xi;
	uint64_t opval = 0;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = md_get(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	err = md_get(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	ni = to_ni(get_md);

	err = ct_get(trig_ct_handle, &ct);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

#ifndef NO_ARG_VALIDATION
	if (unlikely(!ct)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = check_get(get_md, local_get_offset, length, ni);
	if (err)
		goto err4;

	err = check_swap(get_md, local_get_offset,
					 put_md, local_put_offset, length, ni,
		  	 atom_op, atom_type);
	if (err)
		goto err4;

	err = check_overlap(get_md, local_get_offset,
						put_md, local_put_offset, length);
	if (err)
		goto err4;

	if (unlikely(to_ni(put_md) != ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err4;
	}
#endif

	if (op_info[atom_op].use_operand) {
		err = get_operand(atom_type, operand, &opval);
		if (unlikely(err)) {
			WARN();
			goto err1;
		}
	}

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err4;
	}

	xi->operation = OP_SWAP;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->rt.jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->put_md = put_md;
	xi->put_eq = put_md->eq;
	xi->put_ct = put_md->ct;
	xi->get_md = get_md;
	xi->get_eq = get_md->eq;
	xi->get_ct = get_md->ct;
	xi->hdr_data = hdr_data;
	xi->operand = opval;
	xi->user_ptr = user_ptr;
	xi->atom_op = atom_op;
	xi->atom_type = atom_type;
	xi->threshold = threshold;

	xi->rlength = length;
	xi->put_offset = local_put_offset;
	xi->put_resid = length;
	xi->get_offset = local_get_offset;
	xi->get_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	post_ct(xi, ct);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err4:
	ct_put(ct);
err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlTriggeredCTSet(ptl_handle_ct_t ct_handle,
                      ptl_ct_event_t new_ct,
                      ptl_handle_ct_t trig_ct_handle,
                      ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;
	xl_t *xl;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ct_get(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	xl = xl_alloc();
	if (!xl)
		goto err2;

	xl->op = TRIGGERED_CTSET;
	xl->ct_handle = ct_handle;
	xl->value = new_ct;
	xl->threshold = threshold;

	post_ct_local(xl, ct);

	err = PTL_OK;

 err2:
	ct_put(ct);

 err1:
	gbl_put(gbl);
	return err;
}
	
int PtlTriggeredCTInc(ptl_handle_ct_t ct_handle,
                      ptl_ct_event_t increment,
                      ptl_handle_ct_t trig_ct_handle,
                      ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;
	xl_t *xl;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ct_get(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	xl = xl_alloc();
	if (!xl)
		goto err2;

	xl->op = TRIGGERED_CTINC;
	xl->ct_handle = ct_handle;
	xl->value = increment;
	xl->threshold = threshold;

	post_ct_local(xl, ct);

	err = PTL_OK;

 err2:
	ct_put(ct);
	
 err1:
 	gbl_put(gbl);
 	return err;
}

/*
 * PtlStartBundle
 * returns:
 *	PTL_OK
 *	PTL_NO_INIT
 *	PTL_ARG_INVALID
 */
int PtlStartBundle(ptl_handle_ni_t ni_handle)
{
	int err;
	gbl_t *gbl;
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = ni_get(ni_handle, &ni);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	if (unlikely(!ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* TODO implement start bundle */

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}

/*
 * PtlEndBundle
 * returns:
 *	PTL_OK
 *	PTL_NO_INIT
 *	PTL_ARG_INVALID
 */
int PtlEndBundle(ptl_handle_ni_t ni_handle)
{
	int err;
	gbl_t *gbl;
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = ni_get(ni_handle, &ni);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	if (unlikely(!ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* TODO implement end bundle */

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}
