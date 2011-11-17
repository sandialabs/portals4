/**
 * @file ptl_hdr.h
 *
 * @brief This file contains the interface for message headers.
 */

#ifndef PTL_HDR_H
#define PTL_HDR_H

// TODO consider developing CT/OC/ACK types that are distinct from reply

/*
 * PTL_XPORT_HDR
 *	mainly carries information needed to route
 *	packet to remote site and validate it
 *	packed some unrelated info into the first word
 */

#define PTL_HDR_VER_1		(1)

/* note: please keep all init->target before all tgt->init */
enum hdr_op {
	/* from init to target */
	OP_PUT=1,
	OP_GET,
	OP_ATOMIC,
	OP_FETCH,
	OP_SWAP,

	/* from target to init */
	OP_DATA,
	OP_REPLY,
	OP_ACK,
	OP_CT_ACK,
	OP_OC_ACK,
	OP_NO_ACK,	 /* when remote ME has ACK_DISABLE */

	OP_LAST,
};

enum hdr_fmt {
	PKT_FMT_REQ,
	PKT_FMT_REPLY,
	PKT_FMT_ACK,
	PKT_FMT_LAST,
};

/**
 * @brief Common header for portals request/response messages.
 */
#define PTL_COMMON_HDR					\
	unsigned		version:4;		\
	unsigned		operation:4;		\
	unsigned		atom_type:4;		\
	unsigned		ack_req:4;		\
	unsigned		ni_type:4;		\
	unsigned		pkt_fmt:4;		\
	unsigned		hdr_size:8;		\
	unsigned		atom_op:5;		\
	unsigned		data_in:1;		\
	unsigned		data_out:1;		\
	unsigned		reserved_1:1;		\
	unsigned		ni_fail:4;		\
	unsigned		reserved_20:20;		\
	union {						\
	__be32			dst_nid;		\
	__be32			dst_rank;		\
	};						\
	__be32			dst_pid;		\
	union {						\
	__be32			src_nid;		\
	__be32			src_rank;		\
	};						\
	__be32			src_pid;		\
	__be64			length;			\
	__be64			offset;			\
	__be32			handle;			\

/**
 * @brief Header for Portals request messages.
 */
#define PTL_REQ_HDR					\
	__be64			match_bits;		\
	__be64			hdr_data;		\
	__be64			operand;		\
	__be32			pt_index;		\
	__be32			uid;

typedef struct ptl_hdr {
	PTL_COMMON_HDR
} hdr_t;

typedef struct req_hdr {
	PTL_COMMON_HDR
	PTL_REQ_HDR
} req_hdr_t;

void xport_hdr_from_buf(hdr_t *hdr, buf_t *buf);

void base_hdr_from_buf(hdr_t *hdr, buf_t *buf);

#endif /* PTL_HDR_H */
