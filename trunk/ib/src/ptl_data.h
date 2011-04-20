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
	DATA_FMT_DMA,
	DATA_FMT_INDIRECT,
	DATA_FMT_LAST,
} data_fmt_t;

typedef struct data {
	uint8_t			data_fmt;
	uint8_t			data_reserved[3];
	union {
	__be32			data_length;
	__be32			num_sge;
	};
	union {
	uint8_t			data[0];
	struct ibv_sge		sge_list[0];
	};
} __attribute__((__packed__)) data_t;

int data_size(data_t *data);

int append_init_data(md_t *md, data_dir_t dir, ptl_size_t offset,
		     ptl_size_t length, buf_t *buf);

#endif /* PTL_DATA_H */
