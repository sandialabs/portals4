/*
 * ptl_move.c - get/put/atomic/fetch_atomic/swap APIs
 */

#include "ptl_loc.h"

struct atom_op_info {
	int		float_ok;
	int		atomic_ok;
	int		swap_ok;
	int		use_operand;
} op_info[] = {
	[PTL_MIN]	= {	1,	1,	0,	0, },
	[PTL_MAX]	= {	1,	1,	0,	0, },
	[PTL_SUM]	= {	1,	1,	0,	0, },
	[PTL_PROD]	= {	1,	1,	0,	0, },
	[PTL_LOR]	= {	0,	1,	0,	0, },
	[PTL_LAND]	= {	0,	1,	0,	0, },
	[PTL_BOR]	= {	0,	1,	0,	0, },
	[PTL_BAND]	= {	0,	1,	0,	0, },
	[PTL_LXOR]	= {	0,	1,	0,	0, },
	[PTL_BXOR]	= {	0,	1,	0,	0, },
	[PTL_SWAP]	= {	1,	0,	1,	0, },
	[PTL_CSWAP]	= {	1,	0,	1,	1, },
	[PTL_CSWAP_NE]	= {	1,	0,	1,	1, },
	[PTL_CSWAP_LE]	= {	1,	0,	1,	1, },
	[PTL_CSWAP_LT]	= {	1,	0,	1,	1, },
	[PTL_CSWAP_GE]	= {	1,	0,	1,	1, },
	[PTL_CSWAP_GT]	= {	1,	0,	1,	1, },
	[PTL_MSWAP]	= {	0,	0,	1,	1, },
};

int atom_type_size[] = 
{
	[PTL_CHAR]	= 1,
	[PTL_UCHAR]	= 1,
	[PTL_SHORT]	= 2,
	[PTL_USHORT]	= 2,
	[PTL_INT]	= 4,
	[PTL_UINT]	= 4,
	[PTL_LONG]	= 8,
	[PTL_ULONG]	= 8,
	[PTL_FLOAT]	= 4,
	[PTL_DOUBLE]	= 8,
};

static int get_operand(ptl_datatype_t type, void *operand, uint64_t *opval)
{
	int err;
	uint64_t val;

	switch(atom_type_size[type]) {
	case 1:
		if (CHECK_POINTER(operand, uint8_t))
			return err = PTL_ARG_INVALID;
		val = *(uint8_t *)operand;
		break;
	case 2:
		if (CHECK_POINTER(operand, uint16_t))
			return err = PTL_ARG_INVALID;
		val = *(uint16_t *)operand;
		break;
	case 4:
		if (CHECK_POINTER(operand, uint32_t))
			return err = PTL_ARG_INVALID;
		val = *(uint32_t *)operand;
		break;
	case 8:
		if (CHECK_POINTER(operand, uint64_t))
			return err = PTL_ARG_INVALID;
		val = *(uint64_t *)operand;
		break;
	default:
		ptl_error("invalid datatype = %d\n", type);
		val = -1ULL;
		break;
	}

	*opval = val;
	return PTL_OK;
}

static int put_common(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		      ptl_size_t length, ptl_ack_req_t ack_req,
		      ptl_process_t target_id, ptl_pt_index_t pt_index,
		      ptl_match_bits_t match_bits, ptl_size_t remote_offset,
		      void *user_ptr, ptl_hdr_data_t hdr_data, int trig,
		      ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	ct_t *ct;
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

	if (unlikely(!md)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(local_offset + length > md->length)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (unlikely(ack_req < PTL_ACK_REQ || ack_req > PTL_OC_ACK_REQ)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (unlikely(ack_req == PTL_ACK_REQ && !md->eq)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (unlikely(ack_req == PTL_CT_ACK_REQ && !md->ct)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (trig) {
		err = ct_get(trig_ct_handle, &ct);
		if (unlikely(err)) {
			WARN();
			goto err2;
		}

		if (unlikely(!ct)) {
			WARN();
			err = PTL_ARG_INVALID;
			goto err2;
		}
	}

	ni = to_ni(md);

	if (unlikely(length > ni->limits.max_msg_size)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	xi->operation = OP_PUT;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->gbl->jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->ack_req = ack_req;
	xi->put_md = md;
	xi->hdr_data = hdr_data;
	xi->user_ptr = user_ptr;
	xi->threshold = threshold;

	xi->rlength = length;
	xi->put_offset = local_offset;
	xi->put_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	if (trig) {
		post_ct(xi, ct);
		ct_put(ct);
	} else {
		process_init(xi);
	}

	gbl_put(gbl);
	return PTL_OK;

err3:
	if (trig)
		ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlPut(ptl_handle_md_t md_handle, ptl_size_t local_offset,
	   ptl_size_t length, ptl_ack_req_t ack_req, ptl_process_t target_id,
	   ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
	   ptl_size_t remote_offset, void *user_ptr, ptl_hdr_data_t hdr_data)
{
	return put_common(md_handle, local_offset, length, ack_req, target_id,
			  pt_index, match_bits, remote_offset, user_ptr,
			  hdr_data,
			  0, 0, 0);
}

int PtlTriggeredPut(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		    ptl_size_t length, ptl_ack_req_t ack_req,
		    ptl_process_t target_id, ptl_pt_index_t pt_index,
		    ptl_match_bits_t match_bits, ptl_size_t remote_offset,
		    void *user_ptr, ptl_hdr_data_t hdr_data,
		    ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	return put_common(md_handle, local_offset, length, ack_req, target_id,
			  pt_index, match_bits, remote_offset, user_ptr,
			  hdr_data,
			  1, trig_ct_handle, threshold);
}

static int get_common(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		      ptl_size_t length, ptl_process_t target_id,
		      ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
		      void *user_ptr, ptl_size_t remote_offset, int trig,
		      ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	ct_t *ct;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = md_get(md_handle, &md);
	if (unlikely(err))
		goto err1;

	if (unlikely(!md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(local_offset + length > md->length)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (trig) {
		err = ct_get(trig_ct_handle, &ct);
		if (unlikely(err))
			goto err2;

		if (unlikely(!ct)) {
			err = PTL_ARG_INVALID;
			goto err2;
		}
	}

	ni = to_ni(md);

	if (unlikely(length > ni->limits.max_msg_size)) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = xi_alloc(ni, &xi);
	if (unlikely(err))
		goto err3;

	xi->operation = OP_GET;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->gbl->jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->get_md = md;
	xi->user_ptr = user_ptr;
	xi->threshold = threshold;

	xi->rlength = length;
	xi->get_offset = local_offset;
	xi->get_resid = length;
	xi->roffset = remote_offset;

	xi->pkt_len = sizeof(req_hdr_t);
	xi->state = STATE_INIT_START;

	if (trig) {
		post_ct(xi, ct);
		ct_put(ct);
	} else {
		process_init(xi);
	}

	gbl_put(gbl);
	return PTL_OK;

err3:
	if (trig)
		ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlGet(ptl_handle_md_t md_handle, ptl_size_t local_offset,
	   ptl_size_t length, ptl_process_t target_id,
	   ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
	   void *user_ptr, ptl_size_t remote_offset)
{
	return get_common(md_handle, local_offset, length, target_id, pt_index,
			  match_bits, user_ptr, remote_offset,
			  0, 0, 0);
}

int PtlTriggeredGet(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		    ptl_size_t length, ptl_process_t target_id,
		    ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
		    void *user_ptr, ptl_size_t remote_offset,
		    ptl_handle_ct_t ct_handle, ptl_size_t threshold)
{
	return get_common(md_handle, local_offset, length, target_id, pt_index,
			  match_bits, user_ptr, remote_offset,
			  1, ct_handle,threshold);
}

static int atomic_common(ptl_handle_md_t md_handle, ptl_size_t local_offset,
			 ptl_size_t length, ptl_ack_req_t ack_req,
			 ptl_process_t target_id, ptl_pt_index_t pt_index,
			 ptl_match_bits_t match_bits, ptl_size_t remote_offset,
			 void *user_ptr, ptl_hdr_data_t hdr_data,
			 ptl_op_t atom_op, ptl_datatype_t atom_type, int trig,
		         ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *md;
	ni_t *ni;
	ct_t *ct;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(atom_op < PTL_MIN || atom_op > PTL_MSWAP)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!op_info[atom_op].atomic_ok)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(atom_type < PTL_CHAR || atom_type > PTL_DOUBLE)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(atom_type >= PTL_FLOAT && !op_info[atom_op].float_ok)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = md_get(md_handle, &md);
	if (unlikely(err))
		goto err1;

	if (unlikely(!md)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(local_offset + length > md->length)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (unlikely(ack_req < PTL_ACK_REQ || ack_req > PTL_OC_ACK_REQ)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (unlikely(ack_req == PTL_ACK_REQ && !md->eq)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (unlikely(ack_req == PTL_CT_ACK_REQ && !md->ct)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (trig) {
		err = ct_get(trig_ct_handle, &ct);
		if (unlikely(err))
			goto err2;

		if (unlikely(!ct)) {
			WARN();
			err = PTL_ARG_INVALID;
			goto err2;
		}
	}

	ni = to_ni(md);

	if (unlikely(length > ni->limits.max_atomic_size)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = xi_alloc(ni, &xi);
	if (unlikely(err))
		goto err3;

	xi->operation = OP_ATOMIC;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->gbl->jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->ack_req = ack_req;
	xi->put_md = md;
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

	if (trig) {
		post_ct(xi, ct);
		ct_put(ct);
	} else {
		process_init(xi);
	}

	gbl_put(gbl);
	return PTL_OK;

err3:
	if (trig)
		ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlAtomic(ptl_handle_md_t md_handle, ptl_size_t local_offset,
	      ptl_size_t length, ptl_ack_req_t ack_req,
	      ptl_process_t target_id, ptl_pt_index_t pt_index,
	      ptl_match_bits_t match_bits, ptl_size_t remote_offset,
	      void *user_ptr, ptl_hdr_data_t hdr_data,
	      ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	return atomic_common(md_handle, local_offset, length, ack_req,
			     target_id, pt_index, match_bits, remote_offset,
			     user_ptr, hdr_data, atom_op, atom_type,
			     0, 0, 0);
}

int PtlTriggeredAtomic(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		       ptl_size_t length, ptl_ack_req_t ack_req,
		       ptl_process_t target_id, ptl_pt_index_t pt_index,
		       ptl_match_bits_t match_bits, ptl_size_t remote_offset,
		       void *user_ptr, ptl_hdr_data_t hdr_data,
		       ptl_op_t atom_op, ptl_datatype_t atom_type,
		       ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	return atomic_common(md_handle, local_offset, length, ack_req,
			     target_id, pt_index, match_bits, remote_offset,
			     user_ptr, hdr_data, atom_op, atom_type,
			     1, trig_ct_handle, threshold);
}

static int fetch_common(ptl_handle_md_t get_md_handle,
			ptl_size_t local_get_offset,
			ptl_handle_md_t put_md_handle,
			ptl_size_t local_put_offset,
			ptl_size_t length, ptl_process_t target_id,
			ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
			ptl_size_t remote_offset, void *user_ptr,
			ptl_hdr_data_t hdr_data, ptl_op_t atom_op,
			ptl_datatype_t atom_type, int trig,
			ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *get_md;
	md_t *put_md;
	ni_t *ni;
	ct_t *ct;
	xi_t *xi;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(atom_op < PTL_MIN || atom_op > PTL_MSWAP)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!op_info[atom_op].atomic_ok)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(atom_type < PTL_CHAR || atom_type > PTL_DOUBLE)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(atom_type >= PTL_FLOAT && !op_info[atom_op].float_ok)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = md_get(get_md_handle, &get_md);
	if (unlikely(err))
		goto err1;

	if (unlikely(!get_md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(local_get_offset + length > get_md->length)) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = md_get(put_md_handle, &put_md);
	if (unlikely(err))
		goto err2;

	if (unlikely(!put_md)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (unlikely(local_put_offset + length > put_md->length)) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	if (trig) {
		err = ct_get(trig_ct_handle, &ct);
		if (unlikely(err))
			goto err3;

		if (unlikely(!ct)) {
			err = PTL_ARG_INVALID;
			goto err3;
		}
	}

	ni = to_ni(get_md);

	if (unlikely(to_ni(put_md) != ni)) {
		err = PTL_ARG_INVALID;
		goto err4;
	}

	if (unlikely(length > ni->limits.max_atomic_size)) {
		err = PTL_ARG_INVALID;
		goto err4;
	}

	err = xi_alloc(ni, &xi);
	if (unlikely(err))
		goto err4;

	xi->operation = OP_FETCH;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->gbl->jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->put_md = put_md;
	xi->get_md = get_md;
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

	if (trig) {
		post_ct(xi, ct);
		ct_put(ct);
	} else {
		process_init(xi);
	}

	gbl_put(gbl);
	return PTL_OK;

err4:
	if (trig)
		ct_put(ct);
err3:
	md_put(put_md);
err2:
	md_put(get_md);
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
	return fetch_common(get_md_handle, local_get_offset, put_md_handle,
			    local_put_offset, length, target_id, pt_index,
			    match_bits, remote_offset, user_ptr, hdr_data,
			    atom_op, atom_type,
			    0, 0, 0);
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
	return fetch_common(get_md_handle, local_get_offset, put_md_handle,
			    local_put_offset, length, target_id, pt_index,
			    match_bits, remote_offset, user_ptr, hdr_data,
			    atom_op, atom_type,
			    1, trig_ct_handle, threshold);
}

static int swap_common(ptl_handle_md_t get_md_handle,
		       ptl_size_t local_get_offset,
		       ptl_handle_md_t put_md_handle,
		       ptl_size_t local_put_offset,
		       ptl_size_t length, ptl_process_t target_id,
		       ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
		       ptl_size_t remote_offset, void *user_ptr,
		       ptl_hdr_data_t hdr_data, void *operand,
		       ptl_op_t atom_op, ptl_datatype_t atom_type, int trig,
		       ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	md_t *get_md;
	md_t *put_md;
	ni_t *ni;
	ct_t *ct;
	xi_t *xi;
	uint64_t opval = 0;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	if (unlikely(atom_op < PTL_MIN || atom_op > PTL_MSWAP)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(!op_info[atom_op].swap_ok)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(atom_type < PTL_CHAR || atom_type > PTL_DOUBLE)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(atom_type >= PTL_FLOAT && !op_info[atom_op].float_ok)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(op_info[atom_op].use_operand && 
	    length > atom_type_size[atom_type])) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (op_info[atom_op].use_operand) {
		err = get_operand(atom_type, operand, &opval);
		if (unlikely(err)) {
			WARN();
			goto err1;
		}
	}

	err = md_get(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	if (unlikely(!get_md)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(local_get_offset + length > get_md->length)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = md_get(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	if (unlikely(!put_md)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (unlikely(local_put_offset + length > put_md->length)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	if (trig) {
		err = ct_get(trig_ct_handle, &ct);
		if (unlikely(err)) {
			WARN();
			goto err3;
		}

		if (unlikely(!ct)) {
			WARN();
			err = PTL_ARG_INVALID;
			goto err3;
		}
	}

	ni = to_ni(get_md);

	if (unlikely(to_ni(put_md) != ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err4;
	}

	if (unlikely(length > ni->limits.max_atomic_size)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err4;
	}

	err = xi_alloc(ni, &xi);
	if (unlikely(err)) {
		WARN();
		goto err4;
	}

	xi->operation = OP_SWAP;
	xi->target = target_id;
	xi->uid = ni->uid;
	xi->jid = ni->gbl->jid;
	xi->pt_index = pt_index;
	xi->match_bits = match_bits,
	xi->put_md = put_md;
	xi->get_md = get_md;
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

	if (trig) {
		post_ct(xi, ct);
		ct_put(ct);
	} else {
		process_init(xi);
	}

	gbl_put(gbl);
	return PTL_OK;

err4:
	if (trig)
		ct_put(ct);
err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put(gbl);
	return err;
}

int PtlSwap(ptl_handle_md_t get_md_handle, ptl_size_t local_get_offset,
	    ptl_handle_md_t put_md_handle, ptl_size_t local_put_offset,
	    ptl_size_t length, ptl_process_t target_id,
	    ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
	    ptl_size_t remote_offset, void *user_ptr,
	    ptl_hdr_data_t hdr_data, void *operand,
	    ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	return swap_common(get_md_handle, local_get_offset, put_md_handle,
			   local_put_offset, length, target_id, pt_index,
			   match_bits, remote_offset, user_ptr, hdr_data,
			   operand, atom_op, atom_type,
			   0, 0, 0);
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
	return swap_common(get_md_handle, local_get_offset, put_md_handle,
			   local_put_offset, length, target_id, pt_index,
			   match_bits, remote_offset, user_ptr, hdr_data,
			   operand, atom_op, atom_type,
			   1, trig_ct_handle, threshold);
}

int PtlTriggeredCTSet(ptl_handle_ct_t ct_handle,
                      ptl_ct_event_t new_ct,
                      ptl_handle_ct_t trig_ct_handle,
                      ptl_size_t threshold)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ct_get(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	/* TODO see PtlCTSet */
	ct->event = new_ct;

	ni = to_ni(ct);
	pthread_mutex_lock(&ni->ct_wait_mutex);
	if (ni->ct_waiting)
		pthread_cond_broadcast(&ni->ct_wait_cond);
	pthread_mutex_unlock(&ni->ct_wait_mutex);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

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
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ct_get(ct_handle, &ct);
	if (unlikely(err))
		goto err1;
	(void)__sync_fetch_and_add(&ct->event.success, increment.success);
	(void)__sync_fetch_and_add(&ct->event.failure, increment.failure);

	/* TODO see PtlCTSet */
	ni = to_ni(ct);
	pthread_mutex_lock(&ni->ct_wait_mutex);
	if (ni->ct_waiting)
		pthread_cond_broadcast(&ni->ct_wait_cond);
	pthread_mutex_unlock(&ni->ct_wait_mutex);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

int PtlStartBundle(ptl_handle_ni_t ni_handle)
{
	int ret;
	gbl_t *gbl;
	ni_t *ni;

	ret = get_gbl(&gbl);
	if (unlikely(ret)) {
		WARN();
		return ret;
	}

	ret = ni_get(ni_handle, &ni);
	if (unlikely(ret)) {
		WARN();
		goto done;
	}

	ni_put(ni);
done:
	gbl_put(gbl);
	return ret;
}

int PtlEndBundle(ptl_handle_ni_t ni_handle)
{
	int ret;
	gbl_t *gbl;
	ni_t *ni;

	ret = get_gbl(&gbl);
	if (unlikely(ret)) {
		WARN();
		return ret;
	}

	ret = ni_get(ni_handle, &ni);
	if (unlikely(ret)) {
		WARN();
		goto done;
	}

	ni_put(ni);
done:
	gbl_put(gbl);
	return ret;
}
