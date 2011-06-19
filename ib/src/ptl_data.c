/*
 * ptl_data.c
 */

#include "ptl_loc.h"

/*
 * iov_count_sge - return the number of SG entries required to cover IO vector
 * from offset for length.
 */
static int iov_count_sge(ptl_iovec_t *iov, ptl_size_t num_iov,
						 ptl_size_t offset, ptl_size_t length)
{
	ptl_size_t i, j;
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
		/* iovec too small for offset
		 * should never happen
		 */
		WARN();
		return -1;
	}

	for (j = 0; i < num_iov && dst_offset < length; i++, iov++) {
		bytes = iov->iov_len - iov_offset;
		if (bytes == 0)
			continue;

		if (dst_offset + bytes > length)
			bytes = length - dst_offset;

		j++;
		iov_offset = 0;
		dst_offset += bytes;
	}

	if (dst_offset < length) {
		/* iovec too small for offset+length
		 * should never happen
		 */
		WARN();
		return -1;
	}

	return j;
}

/*
 * iov_to_sge - Build a SG list from an IO vector starting at the IO vector
 * offset for the specified length.
 */
static int iov_to_sge(mr_t **mr_list, struct ibv_sge *sge_list,
		      ptl_iovec_t *iov, ptl_size_t num_iov,
		      ptl_size_t offset, ptl_size_t length)
{
	ptl_size_t i, j;
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

	for (j = 0; i < num_iov && dst_offset < length; i++, iov++) {
		bytes = iov->iov_len - iov_offset;
		if (bytes == 0)
			continue;

		if (dst_offset + bytes > length)
			bytes = length - dst_offset;

		if (j >= get_param(PTL_MAX_INLINE_SGE)) {
			WARN();
			return PTL_FAIL;
		}

		sge_list[j].addr = cpu_to_be64((uintptr_t)iov->iov_base +
					       iov_offset);
		sge_list[j].length = cpu_to_be32(bytes);
		sge_list[j].lkey = cpu_to_be32(mr_list[i]->ibmr->lkey);

#if 0
		if (debug) {
			printf("sge_list[%d].addr(0x%lx), length(%d),"
				" lkey(%d)\n", (int)j,
				be64_to_cpu(sge_list[j].addr),
				be32_to_cpu(sge_list[j].length),
				be32_to_cpu(sge_list[j].lkey));
		}
#endif
		j++;
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
 * data_size - return the length of the data area of a portals message.
 */
int data_size(data_t *data)
{
	int size = sizeof(*data);

	if (!data)
		return 0;

	switch (data->data_fmt) {
	case DATA_FMT_IMMEDIATE:
		size += be32_to_cpu(data->data_length);
		break;
	case DATA_FMT_DMA:
		size += be32_to_cpu(data->num_sge) * sizeof(struct ibv_sge);
		break;
	case DATA_FMT_INDIRECT:
		size += sizeof(struct ibv_sge);
		break;
	default:
		assert(0);
		break;
	}

	return size;
}

/*
 * append_init_data - Build and append the data portion of a portals message.
 */
int append_init_data(md_t *md, data_dir_t dir, ptl_size_t offset,
		     ptl_size_t length, buf_t *buf)
{
	int err = PTL_OK;
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	data_t *data = (data_t *)(buf->data + buf->length);
	int num_sge;

	if (dir == DATA_DIR_IN)
		hdr->data_in = 1;
	else
		hdr->data_out = 1;

	if (dir == DATA_DIR_OUT && length <= get_param(PTL_MAX_INLINE_DATA)) {
		data->data_fmt = DATA_FMT_IMMEDIATE;
		data->data_length = cpu_to_be32(length);

		if (md->options & PTL_IOVEC) {
			err = iov_copy_out(data->data, md->start, md->num_iov,
					   offset, length);
			if (err) {
				WARN();
				return err;
			}
		} else {
			memcpy(data->data, md->start + offset, length);
		}

		buf->length += sizeof(*data) + length;
		goto done;
	}

	if (md->options & PTL_IOVEC && md->num_iov > get_param(PTL_MAX_INLINE_SGE)) {
		num_sge = iov_count_sge((ptl_iovec_t *)md->start,
					md->num_iov, offset, length);
		if (num_sge < 0) {
			WARN();
			return PTL_FAIL;
		}

		if (num_sge > get_param(PTL_MAX_INLINE_SGE)) {
			data->data_fmt = DATA_FMT_INDIRECT;
			data->num_sge = cpu_to_be32(1);

			data->sge_list->addr
				= cpu_to_be64((uintptr_t)md->sge_list);
			data->sge_list->length
				= cpu_to_be32(num_sge *
					      sizeof(struct ibv_sge));
			data->sge_list->lkey
				= cpu_to_be32(md->mr->ibmr->lkey);

			buf->length += sizeof(*data) + sizeof(struct ibv_sge);
			goto done;
		} else {
			data->data_fmt = DATA_FMT_DMA;
			data->num_sge = cpu_to_be32(num_sge);
			buf->length += sizeof(*data) + num_sge *
					sizeof(struct ibv_sge);

			err = iov_to_sge(md->mr_list, data->sge_list,
					 (ptl_iovec_t *)md->start, md->num_iov,
					 offset, length);
			if (err) {
				WARN();
				return err;
			}
		}
	} else if (md->options & PTL_IOVEC) {
		data->data_fmt = DATA_FMT_DMA;
		data->num_sge = cpu_to_be32(md->num_iov);
		buf->length += sizeof(*data) +
			md->num_iov * sizeof(struct ibv_sge);

		err = iov_to_sge(md->mr_list, data->sge_list,
				 (ptl_iovec_t *)md->start,
				 md->num_iov, offset, length);
		if (err) {
			WARN();
			return err;
		}
	} else {
		data->data_fmt = DATA_FMT_DMA;
		data->num_sge = cpu_to_be32(1);
		buf->length += sizeof(*data) + sizeof(struct ibv_sge);

		data->sge_list[0].addr = cpu_to_be64((uintptr_t)md->start +
						    offset);
		data->sge_list[0].length = cpu_to_be32(length);
		data->sge_list[0].lkey = cpu_to_be32(md->mr->ibmr->rkey);
#if 0
		if (debug) {
			printf("md->mr->ibmr->addr(%p), lkey(%d), rkey(%d)\n",
				md->mr->ibmr->addr, md->mr->ibmr->lkey,
				md->mr->ibmr->rkey);
			printf("md->start(0x%lx), offset(0x%x)\n",
				(uintptr_t)md->start, (int) offset);
			printf("sge_list[0].addr(0x%lx), length(%d),"
				" lkey(%d)\n",
				 be64_to_cpu(data->sge_list[0].addr),
				be32_to_cpu(data->sge_list[0].length),
				be32_to_cpu(data->sge_list[0].lkey));
		}
#endif
	}

done:
	return err;
}
