/*
 * ptl_hdr.h - the wire protocol
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
typedef enum {
	/* from init to target */
	OP_PUT,
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

	OP_LAST,
} op_t;

typedef enum {
	PKT_FMT_REQ,
	PKT_FMT_REPLY,
	PKT_FMT_ACK,
	PKT_FMT_LAST,
} pkt_fmt_t;

#define PTL_XPORT_HDR					\
	unsigned		version:4;		\
	unsigned		operation:4;		\
	unsigned		atom_op:5;		\
	unsigned		atom_type:4;		\
	unsigned		ack_req:4;		\
	unsigned		ni_type:4;		\
	unsigned		pkt_fmt:4;		\
	unsigned		data_in:1;		\
	unsigned		data_out:1;		\
	unsigned		pkt_reserved:1;		\
	unsigned		ni_fail:4;		\
	unsigned		reserved_1:28;		\
	union {						\
	__be32			dst_nid;		\
	__be32			dst_rank;		\
	};						\
	__be32			dst_pid;		\
	union {						\
	__be32			src_nid;		\
	__be32			src_rank;		\
	};						\
	__be32			src_pid;

/*
 * PTL_BASE_HDR
 *	contains common information shared by request
 *	ack and reply messages
 */
#define PTL_BASE_HDR					\
	__be64			length;			\
	__be64			offset;			\
	__be64			handle;			\

/*
 * PTL_REQ_HDR
 *	contains information specific to a request
 */
#define PTL_REQ_HDR					\
	__be64			match_bits;		\
	__be64			hdr_data;		\
	__be64			operand;		\
	__be32			pt_index;		\
	__be32			jid;			\
	__be32			uid;			\
	__be32			reserved_2;

typedef struct ptl_hdr {
	PTL_XPORT_HDR
	PTL_BASE_HDR
} hdr_t;

typedef struct req_hdr {
	PTL_XPORT_HDR
	PTL_BASE_HDR
	PTL_REQ_HDR
} req_hdr_t;

void xport_hdr_from_xi(hdr_t *hdr, xi_t *xi);
void xport_hdr_from_xt(hdr_t *hdr, xt_t *xt);

void xport_hdr_to_xi(hdr_t *hdr, xi_t *xi);
void xport_hdr_to_xt(hdr_t *hdr, xt_t *xt);

void base_hdr_from_xi(hdr_t *hdr, xi_t *xi);
void base_hdr_from_xt(hdr_t *hdr, xt_t *xt);

void base_hdr_to_xi(hdr_t *hdr, xi_t *xi);
void base_hdr_to_xt(hdr_t *hdr, xt_t *xt);

void req_hdr_from_xi(req_hdr_t *hdr, xi_t *xi);
void req_hdr_to_xt(req_hdr_t *hdr, xt_t *xt);

#endif /* PTL_HDR_H */
