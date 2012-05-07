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
int iov_copy_out(void *dst, ptl_iovec_t *iov, ptl_size_t num_iov,
		 ptl_size_t offset, ptl_size_t length)
{
	ptl_size_t i;
	ptl_size_t iov_offset = 0;
	ptl_size_t src_offset = 0;
	ptl_size_t dst_offset = 0;
	ptl_size_t bytes;

	/* find starting point in iovec from offset,
	 * when loop stops and there is enough room in iovec,
	 * dst_offset == offset, iov points to iovec entry
	 * containing starting point, i is its index in the
	 * array and iov_offset contains the offset into iov */
	for (i = 0; i < num_iov && src_offset < offset; i++, iov++) {
		iov_offset = offset - src_offset;
		if (iov_offset > iov->iov_len)
			iov_offset = iov->iov_len;
		src_offset += iov_offset;
	}

	/* check if we ran off the end of the iovec before we started */
	if (src_offset < offset) {
		WARN();
		return PTL_FAIL;
	}

	/* copy each segment. The first one can have a non zero offset */
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

	/* check if we ran off the end of the iovec after we started */
	if (dst_offset < length) {
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
		  ptl_iovec_t *iov, ptl_size_t num_iov,
		  ptl_size_t offset, ptl_size_t length)
{
	ptl_size_t i;
	ptl_size_t iov_offset = 0;
	ptl_size_t src_offset = 0;
	ptl_size_t dst_offset = 0;
	ptl_size_t bytes;
	int have_odd_size_chunk = 0;

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

			if (bytes & (atom_size - 1)) {
				have_odd_size_chunk++;
				break;
			}

			iov_offset = 0;
			src_offset += bytes;
		}

		/* if we have an odd size chunk it will cross an atom_data
		 * boundary and op will not work. So make a copy of the
		 * target data and do the operation there */
		if (have_odd_size_chunk) {
			void *copy;

			copy = malloc(length);
			if (!copy) {
				WARN();
				return PTL_NO_SPACE;
			}

			iov_copy_out(copy, iov, num_iov, offset, length);
			op(copy, src, length);
			iov_copy_in(copy, iov, num_iov, offset, length);
			free(copy);
			return PTL_OK;
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
	ptl_size_t base;
	ptl_size_t iov_len;

	/* find the index of the iovec element and its starting
	 * offset that contains the start of the data region */
	base = 0;
	for (index_start = 0; index_start < num_iov; index_start++) {
		iov_len = (iov++)->iov_len;
		if (offset < base + iov_len)
			break;
		base += iov_len;
	}

	if (index_start == num_iov)
		return -1;

	/* adjust for offset into first element */
	if (offset > base)
		length += offset - base;

	/* find the index of the iovec element that contains the
	 * end of the data region */
	for (index_stop = index_start; index_stop < num_iov; index_stop++) {
		iov_len = (iov++)->iov_len;
		if (length <= iov_len)
			break;
		length -= iov_len;
	}

	if (index_stop == num_iov)
		return -1;

	*index_p = index_start;
	*base_p = base;

	return index_stop - index_start + 1;
}
