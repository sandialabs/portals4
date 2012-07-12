/**
 * @file ptl_iov.c
 *
 * iovec methods
 */

#include "ptl_loc.h"

/**
 * Copy data from an iovec to linear buffer.
 *
 * copy length bytes from io vector starting at offset offset to dst.
 *
 * @param[in] dst address of destination buffer
 * @param[in] iov address of iovec array
 * @param[in] num_iov number of entries in iovec array
 * @param[in] offset offset into iovec
 * @param[in] length number of bytes to copy
 *
 * @return status
 */
int iov_copy_out(void *dst, ptl_iovec_t *iov, mr_t **mr_list, ptl_size_t num_iov,
				 ptl_size_t offset, ptl_size_t length)
{
	unsigned int i;
	ptl_size_t dst_offset = 0;
	ptl_size_t bytes;

	/* Find starting point in iovec from offset. i is the index of the first iovec. */
	for (i = 0; i < num_iov; i++, iov++) {
		if (offset > iov->iov_len)
			offset -= iov->iov_len;
		else
			break;
	}

	/* check if we ran off the end of the iovec before we started */
	if (i >= num_iov) {
		WARN();
		return PTL_FAIL;
	}

	/* copy each segment. The first one can have a non zero offset */
	for( ; i < num_iov && length; i++, iov++) {
		bytes = iov->iov_len - offset;

		if (bytes > length)
			bytes = length;

		memcpy(dst + dst_offset, addr_to_ppe(iov->iov_base + offset, mr_list[i]), bytes);

		offset = 0;
		length -= bytes;
		dst_offset += bytes;
	}

	/* check if we ran off the end of the iovec after we started */
	if (length) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

/**
 * Copy data from linear buffer to an iovec.
 *
 * copy length bytes from src to an io vector starting at offset
 *
 * @param[in] src address of source buffer
 * @param[in] iov address of iovec array
 * @param[in] num_iov number of entries in iovec array
 * @param[in] offset offset into iovec
 * @param[in] length number of bytes to copy
 *
 * @return status
 */
int iov_copy_in(void *src, ptl_iovec_t *iov, mr_t **mr_list, ptl_size_t num_iov,
		ptl_size_t offset, ptl_size_t length)
{
	unsigned int i;
	ptl_size_t src_offset = 0;
	ptl_size_t bytes;

	for (i = 0; i < num_iov; i++, iov++) {
		if (offset >= iov->iov_len)
			offset -= iov->iov_len;
		else
			break;
	}

	if (i >= num_iov) {
		WARN();
		return PTL_FAIL;
	}

	for( ; i < num_iov && length; i++, iov++) {
		bytes = iov->iov_len - offset;

		if (bytes > length)
			bytes = length;

		memcpy(addr_to_ppe(iov->iov_base + offset, mr_list[i]), src + src_offset, bytes);

		offset = 0;
		length -= bytes;
		src_offset += bytes;
	}

	if (length) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

/**
 * Perform an array of atomic operations between src buffer and iovec.
 *
 * Like iov_copy_in except combine with atomic operation.
 *
 * @param[in] op function performing atomic operation on two arrays
 * @param[in] atom_size the size of each atomic operand
 * @param[in] src address of source buffer
 * @param[in] iov address of iovec array
 * @param[in] num_iov number of entries in iovec array
 * @param[in] offset offset into iovec
 * @param[in] length number of bytes to copy
 *
 * @return status
 */
int iov_atomic_in(atom_op_t op, int atom_size, void *src,
		  ptl_iovec_t *iov, mr_t **mr_list, ptl_size_t num_iov,
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

	/* handle case where atomic data type spans segment boundary */
	if (atom_size > 1) {
		ptl_size_t save_i = i;
		ptl_size_t save_iov_offset = iov_offset;
		ptl_size_t save_src_offset = src_offset;
		ptl_iovec_t *save_iov = iov;

		/* make one pass through the target array to make sure that
		 * each segment contains an even multiple of atom_size
		 * save copies of the initial conditions so we
		 * can start over */
		for( ; i < num_iov && src_offset < length; i++, iov++) {
			bytes = iov->iov_len - iov_offset;
			if (bytes == 0)
				continue;
			if (src_offset + bytes > length)
				bytes = length - src_offset;

			iov_offset = 0;
			src_offset += bytes;
		}

		i = save_i;
		iov_offset = save_iov_offset;
		src_offset = save_src_offset;
		iov = save_iov;
	}

	for( ; i < num_iov && src_offset < length; i++, iov++) {
		bytes = iov->iov_len - iov_offset;
		if (bytes == 0)
			continue;
		if (src_offset + bytes > length)
			bytes = length - src_offset;

		op(addr_to_ppe(iov->iov_base + iov_offset, mr_list[i]), src + src_offset, bytes);

		iov_offset = 0;
		src_offset += bytes;
	}

	if (src_offset < length) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

/**
 * Get the number of iovec elements, starting element and offset.
 *
 * Given a data segment described by an iovec array and
 * the starting offset and length of a region in that
 * data segment, compute the index of the iovec array
 * element and starting offset of the iovec element that contains
 * the start of the region. Count and return the number of iovec elements
 * needed to reach the end of the region. Return -1 if the region
 * will not fit into the data segment.
 *
 * @param[in] iov the iovec list
 * @param[in] num_iov the number of entries in the iovec list
 * @param[in] offset the offset of the data region into the data segment
 * @param[in] length the length of the data region
 * @param[out] index_p the address of the returned index
 * @param[out] base_p the addresss of the returned base offset
 *
 * @return number of iovec elements on success
 * @return -1 on failure
 */
int iov_count_elem(ptl_iovec_t *iov, ptl_size_t num_iov,
				   ptl_size_t offset, ptl_size_t length,
				   ptl_size_t *index_p, ptl_size_t *base_p)
{
	ptl_size_t index_start;
	ptl_size_t index_stop;
	ptl_size_t iov_len;

	/* find the index of the iovec element and its starting
	 * offset that contains the start of the data region */
	for (index_start = 0; index_start < num_iov; index_start++) {
		iov_len = iov->iov_len;

		if (offset < iov_len) {
			/* Adjust total length so we start at the beggining of
			 * that iovec buffer. */
			length += offset;

			*base_p = offset;
			break;
		}

		offset -= iov_len;
		iov++;
	}

	/* Check out of range. */
	if (unlikely(index_start == num_iov)) {
		WARN();
		return -1;
	}

	/* find the index of the iovec element that contains the
	 * end of the data region */
	for (index_stop = index_start; index_stop < num_iov; index_stop++) {
		iov_len = iov->iov_len;

		if (length <= iov_len)
			break;

		length -= iov_len;
		iov++;
	}

	/* Check out of range. */
	if (unlikely(index_stop == num_iov)) {
		WARN();
		return -1;
	}

	*index_p = index_start;

	return index_stop - index_start + 1;
}
