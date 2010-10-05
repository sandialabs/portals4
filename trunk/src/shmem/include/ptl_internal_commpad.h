#ifndef PTL_INTERNAL_COMMPAD_H
#define PTL_INTERNAL_COMMPAD_H

#include <portals4.h>

#include <stddef.h>		       /* for size_t */

extern volatile char *comm_pad;
extern size_t num_siblings;
extern size_t proc_number;
extern size_t per_proc_comm_buf_size;
extern size_t firstpagesize;

#define HDR_TYPE_PUT		0  /* ____ */
#define HDR_TYPE_GET		1  /* ___1 */
#define HDR_TYPE_ATOMIC		2  /* __1_ */
#define HDR_TYPE_FETCHATOMIC	3  /* __11 */
#define HDR_TYPE_SWAP		4  /* _1__ */
#define HDR_TYPE_ACKFLAG	8  /* 1___ */
#define HDR_TYPE_ACKMASK	7  /* _111 */

typedef struct {
    void *volatile next;
    unsigned char type;		// 0=put, 1=get, 2=atomic, 3=fetchatomic, 4=swap
    unsigned char ni;
    ptl_pid_t src;
    ptl_process_t target_id;
    ptl_pt_index_t pt_index;
    ptl_match_bits_t match_bits;
    ptl_size_t dest_offset;
    ptl_size_t length;
    void *user_ptr;
    void *src_data_ptr;
    union {
	struct {
	    ptl_hdr_data_t hdr_data;
	    ptl_ack_req_t ack_req;
	} put;
	struct {
	} get;
	struct {
	    ptl_hdr_data_t hdr_data;
	    ptl_ack_req_t ack_req;
	    ptl_op_t operation;
	    ptl_datatype_t datatype;
	} atomic;
	struct {
	    ptl_hdr_data_t hdr_data;
	    ptl_op_t operation;
	    ptl_datatype_t datatype;
	} fetchatomic;
	struct {
	    ptl_hdr_data_t hdr_data;
	    ptl_op_t operation;
	    ptl_datatype_t datatype;
	} swap;
    } info;
    char data[];
} ptl_internal_header_t;

typedef struct {
    ptl_internal_header_t hdr;
    void * buffered_data;
} ptl_internal_buffered_header_t;

#endif
