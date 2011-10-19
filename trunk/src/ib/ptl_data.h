/*
 * ptl_data.h - data descriptor on the wire
 */

#ifndef PTL_DATA_H
#define PTL_DATA_H

typedef enum {
	DATA_DIR_IN,
	DATA_DIR_OUT,
} data_dir_t;

typedef enum {
	DATA_FMT_NONE,
	DATA_FMT_IMMEDIATE,

	DATA_FMT_RDMA_DMA,
	DATA_FMT_RDMA_INDIRECT,

	DATA_FMT_SHMEM_DMA,
	DATA_FMT_SHMEM_INDIRECT,

	DATA_FMT_LAST,
} data_fmt_t;

struct shmem_iovec {
	uint64_t cookie;
	uint64_t offset;
	uint64_t length;
};

typedef struct data {
	uint8_t			data_fmt;
	uint8_t			data_reserved[3];
	union {
		struct {
			__be32			data_length;
			uint8_t			data[0];
		} immediate;
		struct {
			__be32			num_sge;
			struct ibv_sge	sge_list[0];
		} rdma;
		struct {
			/* Similar to rdma. Don't bother to byteswap because it's
			 * on the same node. */
			unsigned int num_knem_iovecs;
			struct shmem_iovec knem_iovec[0];
		} shmem;
	};
} __attribute__((__packed__)) data_t;

int data_size(data_t *data);

int append_init_data(md_t *md, data_dir_t dir, ptl_size_t offset,
					 ptl_size_t length, buf_t *buf,
					 enum transport_type transport_type);
int append_tgt_data(me_t *me, ptl_size_t offset,
					ptl_size_t length, buf_t *buf, enum transport_type transport_type);

#endif /* PTL_DATA_H */
