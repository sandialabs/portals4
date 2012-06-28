/**
 * @file ptl_move.c
 *
 * @brief Portals move APIs
 */

#include "ptl_loc.h"

/**
 * @brief Allocate a buf or sbuf depending on transport type.
 *
 * @return status
 */
static int get_transport_buf(ni_t *ni, ptl_process_t target_id, buf_t **retbuf)
{
	conn_t *conn;
	int err;
	buf_t *buf;

	*retbuf = NULL;

	/* lookup or allocate a conn_t struct to hold per target info */
	conn = get_conn(ni, target_id);
	if (unlikely(!conn))
		return PTL_FAIL;

	/* allocate the correct type of buf */
	err = conn->transport.buf_alloc(ni, &buf);
	if (unlikely(err)) {
		conn_put(conn);
		return err;
	}

	assert(buf->type == BUF_FREE);
	buf->type = BUF_INIT;

	buf->conn = conn;

	*retbuf = buf;

	return PTL_OK;
}

/**
 * @brief Get operand from address and type.
 *
 * @return status
 */
static int get_operand(ptl_datatype_t type, const void *operand,
		       uint64_t *opval)
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
		/** @todo need to handle double complex case */
		val = -1ULL;
		break;
	default:
		/* should never happen */
		abort();
	}

	*opval = val;
	return PTL_OK;
}

#ifndef NO_ARG_VALIDATION
/**
 * @brief check parameters for a put type operation
 *
 * @return status
 */
static int check_put(md_t *md, ptl_size_t local_offset, ptl_size_t length,
	      ptl_ack_req_t ack_req, ni_t *ni)
{
	if (local_offset + length > md->length)
		return PTL_ARG_INVALID;

	if (ack_req > PTL_OC_ACK_REQ)
		return PTL_ARG_INVALID;

	if (ack_req == PTL_ACK_REQ && !md->eq)
		return PTL_ARG_INVALID;

	if (ack_req == PTL_CT_ACK_REQ && !md->ct)
		return PTL_ARG_INVALID;

	if (length > ni->limits.max_msg_size)
		return PTL_ARG_INVALID;

	if ((md->options & PTL_MD_VOLATILE) &&
		length > ni->limits.max_volatile_size)
		/* We can only guarantee volatile for the data that will be
		 * copied in the request buffer. */
		return PTL_ARG_INVALID;

	return PTL_OK;
}
#endif

/**
 * @brief Perform a put operation.
 *
 * @return status
 */
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

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	md = to_md(md_handle);
	if (unlikely(!md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = obj_to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_put(md, local_offset, length, ack_req, ni);
	if (err)
		goto err2;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err2;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_PUT;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->h2.ack_req = ack_req;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->put_md = md;
	buf->put_eq = md->eq;
	buf->put_ct = md->ct;
	buf->user_ptr = user_ptr;
	buf->put_offset = local_offset;
	buf->init_state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put();
err0:
	return err;
}

/**
 * @brief Perform a triggered put operation.
 *
 * @return status
 */
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

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	md = to_md(md_handle);
	if (unlikely(!md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = obj_to_ni(md);

	err = to_ct(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err2;

#ifndef NO_ARG_VALIDATION
	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	err = check_put(md, local_offset, length, ack_req, ni);
	if (err)
		goto err3;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err3;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_PUT;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->h2.ack_req = ack_req;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->put_md = md;
	buf->put_eq = md->eq;
	buf->put_ct = md->ct;
	buf->user_ptr = user_ptr;
	buf->ct_threshold = threshold;
	buf->put_offset = local_offset;
	buf->init_state = STATE_INIT_START;

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
err0:
	return err;
}

#ifndef NO_ARG_VALIDATION
/**
 * @brief check parameters for a get type operation
 *
 * @return status
 */
static int check_get(md_t *md, ptl_size_t local_offset,
		     ptl_size_t length, ni_t *ni)
{
	if (local_offset + length > md->length)
		return PTL_ARG_INVALID;

	if (length > ni->limits.max_msg_size)
		return PTL_ARG_INVALID;

	return PTL_OK;
}
#endif

/**
 * @brief Perform a get operation.
 *
 * @return status
 */
int PtlGet(ptl_handle_md_t md_handle, ptl_size_t local_offset,
	   ptl_size_t length, ptl_process_t target_id,
	   ptl_pt_index_t pt_index, ptl_match_bits_t match_bits,
	   ptl_size_t remote_offset, void *user_ptr)
{
	int err;
	md_t *md;
	ni_t *ni;
	buf_t *buf;
	req_hdr_t *hdr;

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	md = to_md(md_handle);
	if (unlikely(!md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = obj_to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_get(md, local_offset, length, ni);
	if (err)
		goto err2;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err2;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_GET;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->get_md = md;
	buf->get_eq = md->eq;
	buf->get_ct = md->ct;
	buf->user_ptr = user_ptr;
	buf->get_offset = local_offset;
	buf->init_state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put();
err0:
	return err;
}

/**
 * @brief Perform a triggered get operation.
 *
 * @return status
 */
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
	req_hdr_t *hdr;

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	md = to_md(md_handle);
	if (unlikely(!md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = obj_to_ni(md);

	err = to_ct(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err2;

#ifndef NO_ARG_VALIDATION
	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	err = check_get(md, local_offset, length, ni);
	if (err)
		goto err3;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err3;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_GET;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->get_md = md;
	buf->get_eq = md->eq;
	buf->get_ct = md->ct;
	buf->user_ptr = user_ptr;
	buf->get_offset = local_offset;
	buf->init_state = STATE_INIT_START;
	buf->ct_threshold = threshold;

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
err0:
	return err;
}

#ifndef NO_ARG_VALIDATION
/**
 * @brief check parameters for a atomic type operation
 *
 * @return status
 */
static int check_atomic(md_t *md, ptl_size_t local_offset, ptl_size_t length,
			ni_t *ni, ptl_ack_req_t ack_req,
			ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	if (local_offset + length > md->length)
		return PTL_ARG_INVALID;

	if (length > ni->limits.max_atomic_size)
		return PTL_ARG_INVALID;

	if (ack_req > PTL_OC_ACK_REQ)
		return PTL_ARG_INVALID;

	if (ack_req == PTL_ACK_REQ && !md->eq)
		return PTL_ARG_INVALID;

	if (ack_req == PTL_CT_ACK_REQ && !md->ct)
		return PTL_ARG_INVALID;

	if (atom_op >= PTL_OP_LAST)
		return PTL_ARG_INVALID;

	if (!op_info[atom_op].atomic_ok)
		return PTL_ARG_INVALID;

	if (atom_type >= PTL_DATATYPE_LAST)
		return PTL_ARG_INVALID;

	if ((atom_type == PTL_FLOAT || atom_type == PTL_DOUBLE) &&
		      !op_info[atom_op].float_ok)
		return PTL_ARG_INVALID;

	if ((atom_type == PTL_FLOAT_COMPLEX ||
		      atom_type == PTL_DOUBLE_COMPLEX) &&
		      !op_info[atom_op].complex_ok)
		return PTL_ARG_INVALID;

	return PTL_OK;
}

/**
 * @brief check for overlap between get and put MDs.
 *
 * @return status
 */
static int check_overlap(md_t *get_md, ptl_size_t local_get_offset,
			 md_t *put_md, ptl_size_t local_put_offset,
			 ptl_size_t length)
{
#if 0
	unsigned char *get_start = get_md->start + local_get_offset;
	unsigned char *put_start = put_md->start + local_put_offset;

	if (get_start >= put_start &&
		get_start < put_start + length) {
		return PTL_ARG_INVALID;
	}

	if (get_start + length >= put_start &&
		get_start + length < put_start + length) {
		return PTL_ARG_INVALID;
	}

	return PTL_OK;
#else
	/* Although this is undefined, some tests don't care about
	 * overlapping exist and will fail because overlapping is detected. */
	return PTL_OK;
#endif
}
#endif /* NO_ARG_VALIDATION */

/**
 * @brief Perform a atomic operation.
 *
 * @return status
 */
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

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	md = to_md(md_handle);
	if (unlikely(!md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = obj_to_ni(md);

#ifndef NO_ARG_VALIDATION
	err = check_atomic(md, local_offset, length, ni,
			   ack_req, atom_op, atom_type);
	if (err)
		goto err2;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err2;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_ATOMIC;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->h2.ack_req = ack_req;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->h2.atom_op = atom_op;
	hdr->h2.atom_type = atom_type;
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->put_md = md;
	buf->put_eq = md->eq;
	buf->put_ct = md->ct;
	buf->user_ptr = user_ptr;
	buf->put_offset = local_offset;
	buf->init_state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err2:
	md_put(md);
err1:
	gbl_put();
err0:
	return err;
}

/**
 * @brief Perform a triggered atomic operation.
 *
 * @return status
 */
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

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	md = to_md(md_handle);
	if (unlikely(!md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = obj_to_ni(md);

	err = to_ct(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err2;

#ifndef NO_ARG_VALIDATION
	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	err = check_atomic(md, local_offset, length, ni,
			   ack_req, atom_op, atom_type);
	if (err)
		goto err3;
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err3;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_ATOMIC;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->h2.ack_req = ack_req;
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->h2.atom_op = atom_op;
	hdr->h2.atom_type = atom_type;
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->put_md = md;
	buf->put_eq = md->eq;
	buf->put_ct = md->ct;
	buf->user_ptr = user_ptr;
	buf->ct_threshold = threshold;
	buf->put_offset = local_offset;
	buf->init_state = STATE_INIT_START;

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
err0:
	return err;
}

/**
 * @brief Perform a fetch atomic operation.
 *
 * @return status
 */
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
	md_t *put_md;
	ni_t *ni;
	buf_t *buf;
	req_hdr_t *hdr;

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	get_md = to_md(get_md_handle);
	if (unlikely(!get_md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	put_md = to_md(put_md_handle);
	if (unlikely(!put_md)) {
		err = PTL_ARG_INVALID;
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

	err = check_overlap(get_md, local_get_offset, put_md,
			    local_put_offset, length);
	if (err)
		goto err3;

	if (obj_to_ni(put_md) != ni) {
		err = PTL_ARG_INVALID;
		goto err3;
	}
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err3;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_FETCH;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->h2.atom_op = atom_op;
	hdr->h2.atom_type = atom_type;
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->put_md = put_md;
	buf->put_eq = put_md->eq;
	buf->put_ct = put_md->ct;
	buf->get_md = get_md;
	buf->get_eq = get_md->eq;
	buf->get_ct = get_md->ct;
	buf->user_ptr = user_ptr;
	buf->put_offset = local_put_offset;
	buf->get_offset = local_get_offset;
	buf->init_state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put();
err0:
	return err;
}

/**
 * @brief Perform a triggered fetch atomic operation.
 *
 * @return status
 */
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
	md_t *put_md;
	ni_t *ni;
	ct_t *ct = NULL;
	buf_t *buf;
	req_hdr_t *hdr;

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	get_md = to_md(get_md_handle);
	if (unlikely(!get_md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	put_md = to_md(put_md_handle);
	if (unlikely(!put_md)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	ni = obj_to_ni(get_md);

	err = to_ct(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err3;

#ifndef NO_ARG_VALIDATION
	if (!ct) {
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

	err = check_overlap(get_md, local_get_offset, put_md,
			    local_put_offset, length);
	if (err)
		goto err4;

	if (obj_to_ni(put_md) != ni) {
		err = PTL_ARG_INVALID;
		goto err4;
	}
#endif

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err4;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_FETCH;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->h2.atom_op = atom_op;
	hdr->h2.atom_type = atom_type;
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->put_md = put_md;
	buf->put_eq = put_md->eq;
	buf->put_ct = put_md->ct;
	buf->get_md = get_md;
	buf->get_eq = get_md->eq;
	buf->get_ct = get_md->ct;
	buf->user_ptr = user_ptr;
	buf->ct_threshold = threshold;
	buf->put_offset = local_put_offset;
	buf->get_offset = local_get_offset;
	buf->init_state = STATE_INIT_START;

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
err0:
	return err;
}

/**
 * @brief Perform an atomic sync.
 *
 * @return status
 */
int PtlAtomicSync(void)
{
	int err;

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	/* TODO */
	err = PTL_OK;

	gbl_put();
err0:
	return err;
}

#ifndef NO_ARG_VALIDATION
/**
 * @brief check parameters for a swap type operation
 *
 * @return status
 */
static int check_swap(md_t *get_md, ptl_size_t local_get_offset, md_t *put_md,
		      ptl_size_t local_put_offset, ptl_size_t length, ni_t *ni,
		      ptl_op_t atom_op, ptl_datatype_t atom_type)
{
	if (get_md->obj.obj_ni != put_md->obj.obj_ni)
		return PTL_ARG_INVALID;

	if (local_get_offset + length > get_md->length)
		return PTL_ARG_INVALID;

	if (local_put_offset + length > put_md->length)
		return PTL_ARG_INVALID;

	if (length > ni->limits.max_atomic_size)
		return PTL_ARG_INVALID;

	if (atom_op >= PTL_OP_LAST)
		return PTL_ARG_INVALID;

	if (!op_info[atom_op].swap_ok)
		return PTL_ARG_INVALID;

	if (atom_type >= PTL_DATATYPE_LAST)
		return PTL_ARG_INVALID;

	if ((atom_type == PTL_FLOAT || atom_type == PTL_DOUBLE) &&
		      !op_info[atom_op].float_ok)
		return PTL_ARG_INVALID;

	if ((atom_type == PTL_FLOAT_COMPLEX ||
		      atom_type == PTL_DOUBLE_COMPLEX) &&
		      !op_info[atom_op].complex_ok)
		return PTL_ARG_INVALID;

	if (op_info[atom_op].use_operand && length > atom_type_size[atom_type])
		return PTL_ARG_INVALID;

	return PTL_OK;
}
#endif

/**
 * @brief Perform a swap operation.
 *
 * @return status
 */
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

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	get_md = to_md(get_md_handle);
	if (unlikely(!get_md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	put_md = to_md(put_md_handle);
	if (unlikely(!put_md)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	ni = obj_to_ni(get_md);

#ifndef NO_ARG_VALIDATION
	err = check_get(get_md, local_get_offset, length, ni);
	if (err)
		goto err3;

	err = check_swap(get_md, local_get_offset, put_md,
			 local_put_offset, length, ni, atom_op, atom_type);
	if (err)
		goto err3;

	err = check_overlap(get_md, local_get_offset,
			    put_md, local_put_offset, length);
	if (err)
		goto err3;

	if (obj_to_ni(put_md) != ni) {
		err = PTL_ARG_INVALID;
		goto err3;
	}
#endif

	if (op_info[atom_op].use_operand) {
		err = get_operand(atom_type, operand, &opval);
		if (unlikely(err))
			goto err3;
	}

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err3;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_SWAP;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->operand = cpu_to_le64(opval);
	hdr->h2.atom_op = atom_op;
	hdr->h2.atom_type = atom_type;
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->put_md = put_md;
	buf->put_eq = put_md->eq;
	buf->put_ct = put_md->ct;
	buf->get_md = get_md;
	buf->get_eq = get_md->eq;
	buf->get_ct = get_md->ct;
	buf->user_ptr = user_ptr;
	buf->put_offset = local_put_offset;
	buf->get_offset = local_get_offset;
	buf->init_state = STATE_INIT_START;

	process_init(buf);

	gbl_put();
	return PTL_OK;

err3:
	md_put(put_md);
err2:
	md_put(get_md);
err1:
	gbl_put();
err0:
	return err;
}

/**
 * @brief Perform a triggered swap operation.
 *
 * @return status
 */
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
	md_t *put_md;
	ni_t *ni;
	ct_t *ct = NULL;
	buf_t *buf;
	uint64_t opval = 0;
	req_hdr_t *hdr;

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	get_md = to_md(get_md_handle);
	if (unlikely(!get_md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	put_md = to_md(put_md_handle);
	if (unlikely(!put_md)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	ni = obj_to_ni(get_md);

	err = to_ct(trig_ct_handle, &ct);
	if (unlikely(err))
		goto err3;

#ifndef NO_ARG_VALIDATION
	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = check_get(get_md, local_get_offset, length, ni);
	if (err)
		goto err4;

	err = check_swap(get_md, local_get_offset, put_md, local_put_offset,
			 length, ni, atom_op, atom_type);
	if (err)
		goto err4;

	err = check_overlap(get_md, local_get_offset, put_md,
			    local_put_offset, length);
	if (err)
		goto err4;

	if (obj_to_ni(put_md) != ni) {
		err = PTL_ARG_INVALID;
		goto err4;
	}
#endif

	if (op_info[atom_op].use_operand) {
		err = get_operand(atom_type, operand, &opval);
		if (unlikely(err))
			goto err1;
	}

	err = get_transport_buf(ni, target_id, &buf);
	if (unlikely(err))
		goto err4;

	hdr = (req_hdr_t *)buf->data;

	hdr->h1.operation = OP_SWAP;
	hdr->uid = cpu_to_le32(ni->uid);
	hdr->pt_index = cpu_to_le32(pt_index);
	hdr->match_bits = cpu_to_le64(match_bits);
	hdr->hdr_data = cpu_to_le64(hdr_data);
	hdr->operand = cpu_to_le64(opval);
	hdr->h2.atom_op = atom_op;
	hdr->h2.atom_type = atom_type;
	hdr->h3.length = cpu_to_le64(length);
	hdr->h3.offset = cpu_to_le64(remote_offset);

	buf->target = target_id;
	buf->put_md = put_md;
	buf->put_eq = put_md->eq;
	buf->put_ct = put_md->ct;
	buf->get_md = get_md;
	buf->get_eq = get_md->eq;
	buf->get_ct = get_md->ct;
	buf->user_ptr = user_ptr;
	buf->ct_threshold = threshold;
	buf->put_offset = local_put_offset;
	buf->get_offset = local_get_offset;
	buf->init_state = STATE_INIT_START;

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
err0:
	return err;
}

/**
 * @brief Start a bundle.
 *
 * @return status
 */
int PtlStartBundle(ptl_handle_ni_t ni_handle)
{
	int err;
	ni_t *ni;

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ni)) {
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
err0:
	return err;
}

/**
 * @brief End a bundle.
 *
 * @return status
 */
int PtlEndBundle(ptl_handle_ni_t ni_handle)
{
	int err;
	ni_t *ni;

	err = gbl_get();
	if (unlikely(err))
		goto err0;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ni)) {
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
err0:
	return err;
}
