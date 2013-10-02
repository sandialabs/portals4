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
int append_immediate_data(void *start, mr_t **mr_list, int num_iov,
                          data_dir_t dir, ptl_size_t offset,
                          ptl_size_t length, buf_t *buf)
{
    int err;
    data_t *data = (data_t *)(buf->data + buf->length);

    if (!length)
        return PTL_OK;

    data->data_fmt = DATA_FMT_IMMEDIATE;

    if (dir == DATA_DIR_OUT) {
        data->immediate.data_length = cpu_to_le32(length);

        if (num_iov) {
            err =
                iov_copy_out(data->immediate.data, start, mr_list, num_iov,
                             offset, length);
            if (err) {
                WARN();
                return err;
            }
        } else {
            void *from = addr_to_ppe(start + offset, mr_list[0]);
            memcpy(data->immediate.data, from, length);
        }

        buf->length += sizeof(*data) + length;
    } else {
        assert(dir == DATA_DIR_IN);

        /* The reply will be immediate. No need to build an array of
         * iovecs. This assumes that PTL_MAX_INLINE_DATA is consistent
         * amongst the ranks. */
        data->immediate.data_length = 0;
        buf->length += sizeof(*data);
    }

    return PTL_OK;
}
