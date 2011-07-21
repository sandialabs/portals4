/*
 * ptl_data.c
 */

#include "ptl_loc.h"

/*
 * iov_count_sge - return the number of SG entries required to cover IO vector
 * from offset for length.
 */
static int iov_count_sge(ptl_iovec_t *iov, ptl_size_t num_iov,
						 ptl_size_t offset, ptl_size_t length,
						 int *iov_start_p, ptl_size_t *iov_offset_p)
{
	ptl_size_t i, j;
	ptl_size_t iov_offset = 0;
	ptl_size_t src_offset = 0;
	ptl_size_t cur_length;
	ptl_size_t bytes;
	ptl_size_t start_offset;	/* offset of the beginning of iov_start. */

	assert(num_iov > 0);

	start_offset = 0;
	for (i = 0; i < num_iov; i++, iov++) {
		iov_offset = offset - src_offset;
		if (iov_offset > iov->iov_len)
			iov_offset = iov->iov_len;
		src_offset += iov_offset;

		if (src_offset >= offset)
			break;

		start_offset += iov->iov_len;
	}

	*iov_start_p = i;
	*iov_offset_p = start_offset;

	if (src_offset < offset) {
		/* iovec too small for offset
		 * should never happen
		 */
		WARN();
		return -1;
	}

	cur_length = 0;
	for (j = 0;
		 i < num_iov && cur_length < length; i++, iov++) {
		bytes = iov->iov_len - iov_offset;

		if (bytes > length - cur_length)
			bytes = length - cur_length;

		j++;
		iov_offset = 0;
		cur_length += bytes;
	}

	if (cur_length < length) {
		/* iovec too small for offset+length
		 * should never happen
		 */
		WARN();
		return -1;
	}

	return j;
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
	int iov_start;
	ptl_size_t iov_offset;

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
		assert(buf->length <= buf->size);
	} 
	else if (md->options & PTL_IOVEC) {
		/* Find the index and offset of the first IOV as well as the
		 * total number of IOVs to transfer. */
		num_sge = iov_count_sge((ptl_iovec_t *)md->start,
								md->num_iov, offset, length,
								&iov_start,	&iov_offset);
		if (num_sge < 0) {
			WARN();
			return PTL_FAIL;
		}

		if (num_sge > get_param(PTL_MAX_INLINE_SGE)) {
			/* Indirect case. The IOVs do not fit in a buf_t. */
			data->data_fmt = DATA_FMT_INDIRECT;
			data->num_sge = cpu_to_be32(1);

			data->sge_list->addr
				= cpu_to_be64((uintptr_t)&md->sge_list[iov_start]);
			data->sge_list->length
				= cpu_to_be32(num_sge *
					      sizeof(struct ibv_sge));
			data->sge_list->lkey
				= cpu_to_be32(md->sge_list_mr->ibmr->rkey);

			buf->length += sizeof(*data) + sizeof(struct ibv_sge);

		} else {
			data->data_fmt = DATA_FMT_DMA;
			data->num_sge = cpu_to_be32(num_sge);
			buf->length += sizeof(*data) + num_sge *
					sizeof(struct ibv_sge);

			memcpy(data->sge_list,
				   &md->sge_list[iov_start],
				   num_sge*sizeof(struct ibv_sge));
		}

		/* Adjust the header offset for iov start. */
		hdr->offset = cpu_to_be64(be64_to_cpu(hdr->offset) - iov_offset);

		assert(buf->length <= buf->size);
	} else {
		void *addr;
		
		data->data_fmt = DATA_FMT_DMA;
		data->num_sge = cpu_to_be32(1);
		buf->length += sizeof(*data) + sizeof(struct ibv_sge);

		addr = md->start + offset;

		data->sge_list[0].addr = cpu_to_be64((uintptr_t)addr);
		data->sge_list[0].length = cpu_to_be32(length);

		err = mr_lookup(md->obj.obj_ni, addr, length, &buf->mr_list[buf->num_mr]);
		if (err) {
			WARN();
			return err;
		}

		data->sge_list[0].lkey = cpu_to_be32(buf->mr_list[buf->num_mr]->ibmr->rkey);

		buf->num_mr ++;
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

	return err;
}
