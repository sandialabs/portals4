/*
 * ptl_move.c - get/put/atomic/fetch_atomic/swap APIs
 */

#include "ptl_loc.h"

static int get_transport_buf(ni_t *ni, ptl_process_t target_id, buf_t **buf)
{
	conn_t *conn;
	int err;

	conn = get_conn(ni, target_id);
	if (unlikely(!conn)) {
		WARN();
		return PTL_FAIL;
	}

	if (conn->transport.type == CONN_TYPE_RDMA)
		err = buf_alloc(ni, buf);
	else
		err = sbuf_alloc(ni, buf);
	if (err) {
		WARN();
		return err;
	}

	if ((*buf)->type != BUF_FREE) abort();

	(*buf)->xi.conn = conn;

	return PTL_OK;
}

static int get_operand(ptl_datatype_t type, const void *operand, uint64_t *opval)
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

	if ((md->options & PTL_MD_VOLATILE) &&
		length > get_param(PTL_MAX_INLINE_DATA)) {
		/* We can only guarantee volatile for the data that will be
		 * copied in the request buffer. */
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
	md_t *md;
	ni_t *ni;
	buf_t *buf;
	req_hdr_t *hdr;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = to_md(md_handle, &md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	ni = obj_to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_put(md, local_offset, length, ack_req, ni);
	if (err)
		goto err2;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_PUT;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->ack_req = ack_req;
	buf->xi.put_md = md;
	buf->xi.put_eq = md->eq;
	buf->xi.put_ct = md->ct;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	buf->xi.user_ptr = user_ptr;

	hdr->length		= cpu_to_le64(length);
	buf->xi.put_offset = local_offset;
	buf->xi.put_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put();
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
	md_t *md;
	ni_t *ni;
	ct_t *ct = NULL;
	buf_t *buf;
	req_hdr_t *hdr;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = to_md(md_handle, &md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	ni = obj_to_ni(md);

	err = to_ct(trig_ct_handle, &ct);
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

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_PUT;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->ack_req = ack_req;
	buf->xi.put_md = md;
	buf->xi.put_eq = md->eq;
	buf->xi.put_ct = md->ct;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	buf->xi.user_ptr = user_ptr;
	buf->xi.threshold = threshold;

	hdr->length		= cpu_to_le64(length);
	buf->xi.put_offset = local_offset;
	buf->xi.put_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;

	post_ct(buf, ct);

	ct_put(ct);
	gbl_put();
	return PTL_OK;

err3:
	ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put();
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

static inline void preparePtlGet(buf_t *buf, ni_t *ni, md_t *md,
								 ptl_size_t local_offset,
								 ptl_size_t length, ptl_process_t target_id,
								 ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
								 ptl_size_t remote_offset, void *user_ptr)
{
	req_hdr_t *hdr;

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_GET;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	buf->xi.get_md = md;
	buf->xi.get_eq = md->eq;
	buf->xi.get_ct = md->ct;
	buf->xi.user_ptr = user_ptr;

	hdr->length		= cpu_to_le64(length);
	buf->xi.get_offset = local_offset;
	buf->xi.get_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;
}

int PtlGet(ptl_handle_md_t md_handle, ptl_size_t local_offset,
	   ptl_size_t length, ptl_process_t target_id,
	   ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
	   ptl_size_t remote_offset, void *user_ptr)
{
	int err;
	md_t *md;
	ni_t *ni;
	buf_t *buf;

	err = get_gbl();
	if (unlikely(err))
		return err;

	err = to_md(md_handle, &md);
	if (unlikely(err))
		goto err1;

	ni = obj_to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_get(md, local_offset, length, ni);
	if (err)
		goto err2;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	preparePtlGet(buf, ni, md, local_offset,
				  length, target_id,
				  pt_index, match_bits,
				  remote_offset, user_ptr);

	process_init(buf);

	gbl_put();
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put();
	return err;
}

int PtlTriggeredGet(ptl_handle_md_t md_handle, ptl_size_t local_offset,
		    ptl_size_t length, ptl_process_t target_id,
		    ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
		    ptl_size_t remote_offset, void *user_ptr,
		    ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	md_t *md;
	ni_t *ni;
	ct_t *ct = NULL;
	buf_t *buf;

	err = get_gbl();
	if (unlikely(err))
		return err;

	err = to_md(md_handle, &md);
	if (unlikely(err))
		goto err1;

	ni = obj_to_ni(md);

	err = to_ct(trig_ct_handle, &ct);
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

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	preparePtlGet(buf, ni, md, local_offset,
				  length, target_id,
				  pt_index, match_bits,
				  remote_offset, user_ptr);

	buf->xi.threshold = threshold;

	post_ct(buf, ct);

	ct_put(ct);
	gbl_put();
	return PTL_OK;

err3:
	ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put();
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
#if 0
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
#else
	/* Although this is undefined, some tests don't care about
	 * overlapping exist and will fail because overlapping is detected. */
	return PTL_OK;
#endif
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
	md_t *md;
	ni_t *ni;
	buf_t *buf;
	req_hdr_t *hdr;

	err = get_gbl();
	if (unlikely(err))
		return err;

	err = to_md(md_handle, &md);
	if (unlikely(err))
		goto err1;

	ni = obj_to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_atomic(md, local_offset, length, ni, ack_req, atom_op, atom_type);
	if (err)
		goto err2;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_ATOMIC;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->ack_req = ack_req;
	buf->xi.put_md = md;
	buf->xi.put_eq = md->eq;
	buf->xi.put_ct = md->ct;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	buf->xi.user_ptr = user_ptr;
	hdr->atom_op = atom_op;
	hdr->atom_type = atom_type;

	hdr->length		= cpu_to_le64(length);
	buf->xi.put_offset = local_offset;
	buf->xi.put_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put();
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
	md_t *md;
	ni_t *ni;
	ct_t *ct = NULL;
	buf_t *buf;
	req_hdr_t *hdr;

	err = get_gbl();
	if (unlikely(err))
		return err;

	err = to_md(md_handle, &md);
	if (unlikely(err))
		goto err1;

	ni = obj_to_ni(md);

	err = to_ct(trig_ct_handle, &ct);
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

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_ATOMIC;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->ack_req = ack_req;
	buf->xi.put_md = md;
	buf->xi.put_eq = md->eq;
	buf->xi.put_ct = md->ct;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	buf->xi.user_ptr = user_ptr;
	hdr->atom_op = atom_op;
	hdr->atom_type = atom_type;
	buf->xi.threshold = threshold;

	hdr->length		= cpu_to_le64(length);
	buf->xi.put_offset = local_offset;
	buf->xi.put_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;

	post_ct(buf, ct);

	ct_put(ct);
	gbl_put();
	return PTL_OK;

err3:
	ct_put(ct);
err2:
	md_put(md);
err1:
	gbl_put();
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
	md_t *get_md;
	md_t *put_md = NULL;
	ni_t *ni;
	buf_t *buf;
	req_hdr_t *hdr;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = to_md(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	err = to_md(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	ni = obj_to_ni(get_md);

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

	if (unlikely(obj_to_ni(put_md) != ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_FETCH;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	buf->xi.put_md = put_md;
	buf->xi.put_eq = put_md->eq;
	buf->xi.put_ct = put_md->ct;
	buf->xi.get_md = get_md;
	buf->xi.get_eq = get_md->eq;
	buf->xi.get_ct = get_md->ct;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	buf->xi.user_ptr = user_ptr;
	hdr->atom_op = atom_op;
	hdr->atom_type = atom_type;

	hdr->length		= cpu_to_le64(length);
	buf->xi.put_offset = local_put_offset;
	buf->xi.put_resid = length;
	buf->xi.get_offset = local_get_offset;
	buf->xi.get_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put();
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
	md_t *get_md;
	md_t *put_md = NULL;
	ni_t *ni;
	ct_t *ct = NULL;
	buf_t *buf;
	req_hdr_t *hdr;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = to_md(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	err = to_md(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	ni = obj_to_ni(get_md);

	err = to_ct(trig_ct_handle, &ct);
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

	if (unlikely(obj_to_ni(put_md) != ni)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err4;
	}
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err4;
	}

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_FETCH;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	buf->xi.put_md = put_md;
	buf->xi.put_eq = put_md->eq;
	buf->xi.put_ct = put_md->ct;
	buf->xi.get_md = get_md;
	buf->xi.get_eq = get_md->eq;
	buf->xi.get_ct = get_md->ct;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	buf->xi.user_ptr = user_ptr;
	hdr->atom_op = atom_op;
	hdr->atom_type = atom_type;
	buf->xi.threshold = threshold;

	hdr->length		= cpu_to_le64(length);
	buf->xi.put_offset = local_put_offset;
	buf->xi.put_resid = length;
	buf->xi.get_offset = local_get_offset;
	buf->xi.get_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;

	post_ct(buf, ct);

	ct_put(ct);
	gbl_put();
	return PTL_OK;

err4:
	ct_put(ct);
err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put();
	return err;
}

int PtlAtomicSync(void)
{
	int err;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	/* TODO */
	err = PTL_OK;

	gbl_put();
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
	    ptl_hdr_data_t hdr_data, const void *operand,
	    ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	int err;
	md_t *get_md;
	md_t *put_md = NULL;
	ni_t *ni;
	buf_t *buf;
	uint64_t opval = 0;
	req_hdr_t *hdr;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = to_md(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	err = to_md(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	ni = obj_to_ni(get_md);

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

	if (unlikely(obj_to_ni(put_md) != ni)) {
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

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_SWAP;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	buf->xi.put_md = put_md;
	buf->xi.put_eq = put_md->eq;
	buf->xi.put_ct = put_md->ct;
	buf->xi.get_md = get_md;
	buf->xi.get_eq = get_md->eq;
	buf->xi.get_ct = get_md->ct;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->operand = cpu_to_le64(opval);

	buf->xi.user_ptr = user_ptr;
	hdr->atom_op = atom_op;
	hdr->atom_type = atom_type;

	hdr->length		= cpu_to_le64(length);
	buf->xi.put_offset = local_put_offset;
	buf->xi.put_resid = length;
	buf->xi.get_offset = local_get_offset;
	buf->xi.get_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put();
	return err;
}

int PtlTriggeredSwap(ptl_handle_md_t get_md_handle, ptl_size_t local_get_offset,
		     ptl_handle_md_t put_md_handle, ptl_size_t local_put_offset,
		     ptl_size_t length, ptl_process_t target_id,
		     ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
		     ptl_size_t remote_offset, void *user_ptr,
		     ptl_hdr_data_t hdr_data, const void *operand,
		     ptl_op_t atom_op, ptl_datatype_t atom_type,
		     ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	md_t *get_md;
	md_t *put_md = NULL;
	ni_t *ni;
	ct_t *ct = NULL;
	buf_t *buf;
	uint64_t opval = 0;
	req_hdr_t *hdr;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = to_md(get_md_handle, &get_md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	err = to_md(put_md_handle, &put_md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	ni = obj_to_ni(get_md);

	err = to_ct(trig_ct_handle, &ct);
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

	if (unlikely(obj_to_ni(put_md) != ni)) {
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

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err)) {
		WARN();
		goto err4;
	}

	hdr = (req_hdr_t *)buf->data;

	hdr->operation = OP_SWAP;
	buf->xi.target = target_id;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	buf->xi.put_md = put_md;
	buf->xi.put_eq = put_md->eq;
	buf->xi.put_ct = put_md->ct;
	buf->xi.get_md = get_md;
	buf->xi.get_eq = get_md->eq;
	buf->xi.get_ct = get_md->ct;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->operand = cpu_to_le64(opval);
	buf->xi.user_ptr = user_ptr;
	hdr->atom_op = atom_op;
	hdr->atom_type = atom_type;
	buf->xi.threshold = threshold;

	hdr->length		= cpu_to_le64(length);
	buf->xi.put_offset = local_put_offset;
	buf->xi.put_resid = length;
	buf->xi.get_offset = local_get_offset;
	buf->xi.get_resid = length;
	hdr->offset		= cpu_to_le64(remote_offset);

	buf->xi.pkt_len = sizeof(req_hdr_t);
	buf->xi.state = STATE_INIT_START;

	post_ct(buf, ct);

	ct_put(ct);
	gbl_put();
	return PTL_OK;

err4:
	ct_put(ct);
err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put();
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
	ni_t *ni;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = to_ni(ni_handle, &ni);
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
	gbl_put();
	return PTL_OK;

	ni_put(ni);
err1:
	gbl_put();
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
	ni_t *ni;

	err = get_gbl();
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = to_ni(ni_handle, &ni);
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
	gbl_put();
	return PTL_OK;

	ni_put(ni);
err1:
	gbl_put();
	return err;
}
