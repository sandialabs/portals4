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
    OP_PUT = 1,
    OP_GET,
    OP_ATOMIC,
    OP_FETCH,
    OP_SWAP,                           /* must be last of requests */

    /* Either way. */
    OP_RDMA_DISC,

    /* from target to init. Do not change the order. */
    OP_REPLY,
    OP_ACK,
    OP_CT_ACK,
    OP_OC_ACK,
    OP_NO_ACK,                         /* when remote ME has ACK_DISABLE */

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
struct hdr_common {
    unsigned int version:4;
    unsigned int operation:4;
    unsigned int ni_fail:4;     /* response only */
    unsigned int data_in:1;
    unsigned int data_out:1;
    unsigned int matching_list:2;   /* response only */
    unsigned int operand:1;
    unsigned int pad:6;
    unsigned int physical:1;    /* PPE */
    unsigned int ni_type:4;     /* request only */
    unsigned int pkt_fmt:4;     /* request only */
    __le32 handle;
    union {
        __le32 src_nid;
        __le32 src_rank;
    };
    __le32 src_pid;
#if IS_PPE
    /* This information is always needed by the PPE to find the
     * destination NI. */
    __le32 hash;
    union {
        __le32 dst_nid;
        __le32 dst_rank;
    };
    __le32 dst_pid;
#endif
};

/**
 * @brief Header for Portals request messages.
 *
 * Due to headers being reused to send a reply/ack, hdr_common must
 * be first, followed by hdr_region.
 */
typedef struct req_hdr {
    struct hdr_common h1;
    unsigned int ack_req:4;
    unsigned int atom_type:4;
    unsigned int atom_op:5;
    unsigned int reserved_19:19;
    __le64 rlength;
    __le64 roffset;
    __le64 match_bits;
    __le64 hdr_data;
    __le32 pt_index;
    __le32 uid;
#if WITH_TRANSPORT_UDP
    unsigned int udp_is_large:1;
    unsigned int fragment_seq:8;
#endif
} req_hdr_t;

/* Header for an ack or a reply. */
typedef struct ack_hdr {
    struct hdr_common h1;
    __le64 mlength;
    __le64 moffset;
} ack_hdr_t;

#endif /* PTL_HDR_H */
