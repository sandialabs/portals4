/**
 * @file ptl_data.h
 *
 * @brief This file contains the interface to ptl_data.c
 */

#ifndef PTL_DATA_H
#define PTL_DATA_H

enum data_dir {
	DATA_DIR_IN,
	DATA_DIR_OUT,
};

typedef enum data_dir data_dir_t;

enum data_fmt {
	DATA_FMT_NONE = 0,
	DATA_FMT_IMMEDIATE,

#if WITH_TRANSPORT_IB
	DATA_FMT_RDMA_DMA,
	DATA_FMT_RDMA_INDIRECT,
#endif

#if WITH_TRANSPORT_SHMEM
	DATA_FMT_SHMEM_DMA,
	DATA_FMT_SHMEM_INDIRECT,
#endif

	DATA_FMT_LAST,
};

typedef enum data_fmt data_fmt_t;

struct mem_iovec {
#if WITH_TRANSPORT_SHMEM
	uint64_t		cookie;
	uint64_t		offset;		/* add to cookie to get address */
#endif
	uint64_t		length;
};

/**
 * @brief Descriptor for input or output data for a message.
 */
struct data {
	/** The data descriptor format */
	uint8_t			data_fmt;
	uint8_t			data_reserved[3];
	union {
		/** Inline immediate data */
		struct {
			__le32			data_length;
			uint8_t			data[0];
		} immediate;

#if WITH_TRANSPORT_IB
		/** DMA or Indirect RDMA data */
		struct {
			__le32			num_sge;
			struct ibv_sge		sge_list[0];
		} rdma;
#endif

#if WITH_TRANSPORT_SHMEM
		/* DMA or Indirect shmem data */
		struct {
			unsigned int		num_mem_iovecs;
			struct mem_iovec	mem_iovec[0];
		} mem;
#endif

	};
} __attribute__((__packed__));

typedef struct data data_t;

int data_size(data_t *data);

int append_init_data(md_t *md, data_dir_t dir, ptl_size_t offset,
		     ptl_size_t length, buf_t *buf, enum transport_type type);

int append_tgt_data(me_t *me, ptl_size_t offset,
		    ptl_size_t length, buf_t *buf);

#endif /* PTL_DATA_H */
