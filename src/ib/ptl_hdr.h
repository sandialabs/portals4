/**
 * @file ptl_hdr.h
 *
 * @brief This file contains the interface for message headers.
 */

#ifndef PTL_HDR_H
#define PTL_HDR_H

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
	OP_SWAP,					/* must be last of requests */

	/* Either way. */
	OP_RDMA_DISC,

	/* from target to init. Do not change the order. */
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
#define PTL_COMMON1_HDR				\
	unsigned int	version:4;		\
	unsigned int	operation:4;	\
	unsigned int	ni_fail:4;	/* response only */	\
	unsigned int	data_in:1;		\
	unsigned int	data_out:1;		\
	unsigned int    pad:10;			\
	unsigned int	ni_type:4;	/* request only */	\
	unsigned int	pkt_fmt:4;	/* request only */	\
	__be32			handle;

#define PTL_COMMON2_HDR				\
	unsigned int	ack_req:4;		\
	unsigned int	atom_type:4;	\
	unsigned int	atom_op:5;		\
	unsigned int	reserved_19:19;	\
	union {							\
	__be32			dst_nid;		\
	__be32			dst_rank;		\
	};								\
	__be32			dst_pid;		\
	union {							\
	__be32			src_nid;		\
	__be32			src_rank;		\
	};								\
	__be32			src_pid;		\
	__be64			length;			\
	__be64			offset;			\

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
	PTL_COMMON1_HDR
	PTL_COMMON2_HDR
} hdr_t;

typedef struct req_hdr {
	PTL_COMMON1_HDR
	PTL_COMMON2_HDR
	PTL_REQ_HDR
} req_hdr_t;

/* Header for an ack or a reply. */
typedef struct ack_hdr {
	PTL_COMMON1_HDR
	__be64			length;
	__be64			offset;
	unsigned char   matching_list;
} ack_hdr_t;

#endif /* PTL_HDR_H */
