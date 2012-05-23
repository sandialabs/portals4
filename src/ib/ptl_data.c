/**
 * @file ptl_data.c
 *
 * @brief This file contains the method implementations for data_t.
 *
 * Each message, request or response, may contain zero, one or two optional
 * data segments after the message header. The data segments are for input
 * data or output data. A PtlPut request message has a data out segnent,
 * a PtlGet request message has a data in segment and a PtlSwap request may
 * have both a data out and a data in data segment.
 *
 * Each data segment can have several formats. It can contain the actual
 * data for small messages, one or more DMA descriptors, or for messages
 * with very many segments the data segment can contain a DMA descriptor for
 * an external segment list. These formats are called IMMEDIATE, DMA and
 * INDIRECT. InfiniBand DMA descriptors are based on OFA verbs sge's
 * (scatter gather elements). Shared memory DMA descriptors are based on
 * struct mem_iovec described below.
 *
 * Three APIs are provided with the data_t struct: data_size returns
 * the actual size of a data segment, append_init_data and append_tgt_data
 * build data segments for request and response messages respectively.
 */

#include "ptl_loc.h"

/**
 * @brief Return the size of a data descriptor
 *
 * @param[in] data the data descriptor
 *
 * @return the size in bytes of the data descriptor
 */
int data_size(data_t *data)
{
	int size = sizeof(*data);

	if (!data)
		return 0;

	switch (data->data_fmt) {
	case DATA_FMT_IMMEDIATE:
		size += le32_to_cpu(data->immediate.data_length);
		break;

#if WITH_TRANSPORT_IB
	case DATA_FMT_RDMA_DMA:
		size += le32_to_cpu(data->rdma.num_sge) * sizeof(struct ibv_sge);
		break;
	case DATA_FMT_RDMA_INDIRECT:
		size += sizeof(struct ibv_sge);
		break;
#endif

#if WITH_TRANSPORT_SHMEM && USE_KNEM
	case DATA_FMT_KNEM_DMA:
		size += data->mem.num_mem_iovecs * sizeof(struct mem_iovec);
		break;
	case DATA_FMT_KNEM_INDIRECT:
		size += sizeof(struct mem_iovec);
		break;
#endif

#if IS_PPE
	case DATA_FMT_MEM_DMA:
		size += data->mem.num_mem_iovecs * sizeof(struct mem_iovec);
		break;
	case DATA_FMT_MEM_INDIRECT:
		size += sizeof(struct mem_iovec);
		break;
#endif

	default:
		abort();
		break;
	}

	return size;
}

/**
 * @brief Build and append a data segment to a request message.
 *
 * @param[in] md the md that contains the data
 * @param[in] dir the data direction, in or out
 * @param[in] offset the offset into the md
 * @param[in] length the length of the data
 * @param[in] buf the buf the add the data segment to
 * @param[in] type the transport type
 *
 * @return status
 */
int append_init_data(md_t *md, data_dir_t dir, ptl_size_t offset,
		     ptl_size_t length, buf_t *buf, const conn_t *conn)
{
	int err = PTL_OK;
	req_hdr_t *hdr = (req_hdr_t *)buf->data;
	data_t *data = (data_t *)(buf->data + buf->length);
	int num_sge;
	ptl_size_t iov_start = 0;
	ptl_size_t iov_offset = 0;

	if (dir == DATA_DIR_OUT && length <= get_param(PTL_MAX_INLINE_DATA)) {
		data->data_fmt = DATA_FMT_IMMEDIATE;
		data->immediate.data_length = cpu_to_le32(length);

		if (md->options & PTL_IOVEC) {
			err = iov_copy_out(data->immediate.data, md->start,
					   md->num_iov, offset, length);
			if (err) {
				WARN();
				return err;
			}
		} else {
			memcpy(data->immediate.data, md->start + offset,
			       length);
		}

		buf->length += sizeof(*data) + length;
	} 
	else if (md->options & PTL_IOVEC) {
		ptl_iovec_t *iovecs = md->start;

		/* Find the index and offset of the first IOV as well as the
		 * total number of IOVs to transfer. */
		num_sge = iov_count_elem(iovecs, md->num_iov,
								 offset, length, &iov_start, &iov_offset);
		if (num_sge < 0) {
			WARN();
			return PTL_FAIL;
		}

		if (num_sge > get_param(PTL_MAX_INLINE_SGE))
			/* Indirect case. The IOVs do not fit in a buf_t. */
			conn->transport.append_init_data_iovec_indirect(data, md, iov_start, num_sge, length, buf);
		else
			conn->transport.append_init_data_iovec_direct(data, md, iov_start, num_sge, length, buf);

		/* @todo this is completely bogus */
		/* Adjust the header offset for iov start. */
		hdr->offset = cpu_to_le64(le64_to_cpu(hdr->offset) - iov_offset);
	} else {
		void *addr;
		mr_t *mr;
		ni_t *ni = obj_to_ni(md);

		addr = md->start + offset;
		err = mr_lookup(ni, addr, length, &mr);
		if (err) {
			WARN();
			return err;
		}

		buf->mr_list[buf->num_mr++] = mr;

		conn->transport.append_init_data_direct(data, mr, addr, length, buf);
	}

	assert(buf->length <= BUF_DATA_SIZE);
	return err;
}

/**
 * @brief Build and append a data segment to a response message.
 *
 * This is only called for short Get/Fetch/Swap responses
 * that can be sent as immediate inline data.
 *
 * @param[in] me the me or le that contains the data
 * @param[in] offset the offset into the me of the data
 * @param[in] length the length of the data
 * @param[in] buf the buf to add the data segment to
 *
 * @return status
 */
int append_tgt_data(me_t *me, ptl_size_t offset,
		    ptl_size_t length, buf_t *buf)
{
	int err = PTL_OK;
	data_t *data = (data_t *)(buf->data + buf->length);

	assert(length <= get_param(PTL_MAX_INLINE_DATA));

	data->data_fmt = DATA_FMT_IMMEDIATE;
	data->immediate.data_length = cpu_to_le32(length);

	if (me->options & PTL_IOVEC) {
		err = iov_copy_out(data->immediate.data, me->start,
				   me->num_iov, offset, length);
		if (err) {
			WARN();
			return err;
		}
	} else {
		memcpy(data->immediate.data, me->start + offset, length);
	}

	buf->length += sizeof(*data) + length;
	assert(buf->length <= BUF_DATA_SIZE);

	return err;
}
