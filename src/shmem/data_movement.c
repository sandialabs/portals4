/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_DM.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_queues.h"
#include "ptl_internal_atomic.h"

volatile uint32_t iops_lock = 0;	// 1 = busy, 2 = initialized
static ptl_internal_q_t inflight_ops;

static size_t num_hdrs = 0;
static volatile char *msg_buf = NULL;

void INTERNAL PtlInternalDMSetup(
    ptl_size_t max_msg_size)
{
    uint32_t tmp;
    size_t msg_buf_size, hdr_buf_size;
    while ((tmp = PtlInternalAtomicCas32(&(iops_lock), 0, 1)) == 1) ;
    if (tmp == 0) {
	PtlInternalQueueInit(&inflight_ops);
	msg_buf_size = max_msg_size;
	assert(msg_buf_size < per_proc_comm_buf_size);
	hdr_buf_size = per_proc_comm_buf_size - msg_buf_size;
	assert(hdr_buf_size >= sizeof(ptl_internal_header_t));
	assert(hdr_buf_size % sizeof(ptl_internal_header_t) == 0);
	num_hdrs = hdr_buf_size / sizeof(ptl_internal_header_t);
	for (size_t i = 0; i < num_hdrs - 1; ++i) {
	    ops[i].next = ops + i;
	}
	ops[num_hdrs - 1].next = NULL;
	msg_buf = (char *)(ops + num_hdrs);
	__sync_synchronize();
	iops_lock = 2;
    }
}

void INTERNAL PtlInternalDMTeardown(void)
{
    PtlInternalQueueDestroy(&inflight_ops);
}

int API_FUNC PtlPut(
    ptl_handle_md_t md_handle,
    ptl_size_t local_offset,
    ptl_size_t length,
    ptl_ack_req_t ack_req,
    ptl_process_id_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    ptl_size_t remote_offset,
    void *user_ptr,
    ptl_hdr_data_t hdr_data)
{
    return PTL_FAIL;
}

int API_FUNC PtlGet(
    ptl_handle_md_t md_handle,
    ptl_size_t local_offset,
    ptl_size_t length,
    ptl_process_id_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    void *user_ptr,
    ptl_size_t remote_offset)
{
    return PTL_FAIL;
}

int API_FUNC PtlAtomic(
    ptl_handle_md_t md_handle,
    ptl_size_t local_offset,
    ptl_size_t length,
    ptl_ack_req_t ack_req,
    ptl_process_id_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    ptl_size_t remote_offset,
    void *user_ptr,
    ptl_hdr_data_t hdr_data,
    ptl_op_t operation,
    ptl_datatype_t datatype)
{
    return PTL_FAIL;
}

int API_FUNC PtlFetchAtomic(
    ptl_handle_md_t get_md_handle,
    ptl_size_t local_get_offset,
    ptl_handle_md_t put_md_handle,
    ptl_size_t local_put_offset,
    ptl_size_t length,
    ptl_process_id_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    ptl_size_t remote_offset,
    void *user_ptr,
    ptl_hdr_data_t hdr_data,
    ptl_op_t operation,
    ptl_datatype_t datatype)
{
    return PTL_FAIL;
}

int API_FUNC PtlSwap(
    ptl_handle_md_t get_md_handle,
    ptl_size_t local_get_offset,
    ptl_handle_md_t put_md_handle,
    ptl_size_t local_put_offset,
    ptl_size_t length,
    ptl_process_id_t target_id,
    ptl_pt_index_t pt_index,
    ptl_match_bits_t match_bits,
    ptl_size_t remote_offset,
    void *user_ptr,
    ptl_hdr_data_t hdr_data,
    void *operand,
    ptl_op_t operation,
    ptl_datatype_t datatype)
{
    return PTL_FAIL;
}
