/*
 * ptl_init.c - initiator side processing
 */
#include "ptl_loc.h"

/*
 * send_message
 *	send a message to remote end
 */
int send_message(buf_t *buf, int signaled)
{
	int err;
	xi_t *xi = buf->xi;
	struct ibv_send_wr *bad_wr;

	if (debug)
		printf("send_message\n");

	buf->ib.send_wr.opcode = IBV_WR_SEND;
	if (signaled)
		buf->ib.send_wr.send_flags = IBV_SEND_SIGNALED;
	buf->ib.sg_list[0].length = buf->length;
	buf->type = BUF_SEND;
#ifdef USE_XRC
	buf->send_wr.xrc_remote_srq_num = buf->dest->xrc_remote_srq_num;
#endif

	buf->ib.comp = signaled;

	pthread_spin_lock(&xi->send_list_lock);

	err = ibv_post_send(buf->dest->qp, &buf->ib.send_wr, &bad_wr);
	if (err) {
		pthread_spin_unlock(&xi->send_list_lock);

		WARN();

		return PTL_FAIL;
	}

	if (signaled) {
		assert(buf->num_mr == 0);
		list_add_tail(&buf->list, &xi->send_list);
	} else {
		list_add_tail(&buf->list, &xi->ack_list);
	}

	pthread_spin_unlock(&xi->send_list_lock);

	return PTL_OK;
}

/*
 * iov_copy_in
 *	copy length bytes to io vector starting at offset offset
 *	from src to an array of io vectors of length num_iov
 */
int iov_copy_in(void *src, ptl_iovec_t *iov, ptl_size_t num_iov,
		ptl_size_t offset, ptl_size_t length)
{
	ptl_size_t i;
	ptl_size_t iov_offset = 0;
	ptl_size_t src_offset = 0;
	ptl_size_t dst_offset = 0;
	ptl_size_t bytes;

	for (i = 0; i < num_iov && dst_offset < offset; i++, iov++) {
		iov_offset = offset - dst_offset;
		if (iov_offset > iov->iov_len)
			iov_offset = iov->iov_len;
		dst_offset += iov_offset;
	}

	if (dst_offset < offset) {
		WARN();
		return PTL_FAIL;
	}

	for( ; i < num_iov && src_offset < length; i++, iov++) {
		bytes = iov->iov_len - iov_offset;
		if (bytes == 0)
			continue;
		if (src_offset + bytes > length)
			bytes = length - src_offset;

		memcpy(iov->iov_base + iov_offset, src + src_offset, bytes);

		iov_offset = 0;
		src_offset += bytes;
	}

	if (src_offset < length) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

/*
 * iov_copy_out
 *	copy length bytes from io vector starting at offset offset
 *	to dst from an array of io vectors of length num_iov
 */
int iov_copy_out(void *dst, ptl_iovec_t *iov, ptl_size_t num_iov,
		 ptl_size_t offset, ptl_size_t length)
{
	ptl_size_t i;
	ptl_size_t iov_offset = 0;
	ptl_size_t src_offset = 0;
	ptl_size_t dst_offset = 0;
	ptl_size_t bytes;

	for (i = 0; i < num_iov && src_offset < offset; i++, iov++) {
		iov_offset = offset - src_offset;
		if (iov_offset > iov->iov_len)
			iov_offset = iov->iov_len;
		src_offset += iov_offset;
	}

	if (src_offset < offset) {
		WARN();
		return PTL_FAIL;
	}

	for( ; i < num_iov && dst_offset < length; i++, iov++) {
		bytes = iov->iov_len - iov_offset;
		if (bytes == 0)
			continue;
		if (dst_offset + bytes > length)
			bytes = length - dst_offset;

		memcpy(dst + dst_offset, iov->iov_base + iov_offset, bytes);

		iov_offset = 0;
		dst_offset += bytes;
	}

	if (dst_offset < length) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

/*
 * iov_atomic_in
 *	like iov_copy_in except apply atomic op
 *
 *	TODO this implementation assumes that iovec's are
 *	properly aligned with the size of atomic_datatypes
 *	this may not be adequate in all cases which would
 *	require copying at the boundaries to handle datatypes
 *	that straddled two iovecs.
 */
int iov_atomic_in(atom_op_t op, void *src, ptl_iovec_t *iov,
		  ptl_size_t num_iov, ptl_size_t offset, ptl_size_t length)
{
	ptl_size_t i;
	ptl_size_t iov_offset = 0;
	ptl_size_t src_offset = 0;
	ptl_size_t dst_offset = 0;
	ptl_size_t bytes;

	for (i = 0; i < num_iov && dst_offset < offset; i++, iov++) {
		iov_offset = offset - dst_offset;
		if (iov_offset > iov->iov_len)
			iov_offset = iov->iov_len;
		dst_offset += iov_offset;
	}

	if (dst_offset < offset) {
		WARN();
		return PTL_FAIL;
	}

	for( ; i < num_iov && src_offset < length; i++, iov++) {
		bytes = iov->iov_len - iov_offset;
		if (bytes == 0)
			continue;
		if (src_offset + bytes > length)
			bytes = length - src_offset;

		op(iov->iov_base + iov_offset, src + src_offset, bytes);

		iov_offset = 0;
		src_offset += bytes;
	}

	if (src_offset < length) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}
