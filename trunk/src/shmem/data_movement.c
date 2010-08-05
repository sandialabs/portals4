/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <stdlib.h>
#include <assert.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_queues.h"
#include "ptl_internal_DM.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_pid.h"
#include "ptl_internal_handles.h"

volatile uint32_t iops_lock = 0;	// 1 = busy, 2 = initialized
static ptl_internal_q_t inflight_ops;

static size_t num_hdrs = 0;
static volatile char *msg_buf = NULL;

typedef struct {
    enum { PUT, GET, ATOMIC, FETCHATOMIC, SWAP } type;
    ptl_process_id_t target_id;
    ptl_pt_index_t pt_index;
    ptl_match_bits_t match_bits;
    ptl_size_t remote_offset;
    void *user_ptr;
    ptl_hdr_data_t hdr_data;
    union {
	struct {
	    ptl_handle_md_t md_handle;
	    ptl_size_t local_offset;
	    ptl_size_t length;
	    ptl_ack_req_t ack_req;
	} put;
	struct {
	    ptl_handle_md_t md_handle;
	    ptl_size_t local_offset;
	    ptl_size_t length;
	} get;
	struct {
	    ptl_handle_md_t md_handle;
	    ptl_size_t local_offset;
	    ptl_size_t length;
	    ptl_ack_req_t ack_req;
	    ptl_op_t operation;
	    ptl_datatype_t datatype;
	} atomic;
	struct {
	    ptl_handle_md_t get_md_handle;
	    ptl_size_t local_get_offset;
	    ptl_handle_md_t put_md_handle;
	    ptl_size_t local_put_offset;
	    ptl_size_t length;
	    ptl_op_t operation;
	    ptl_datatype_t datatype;
	} fetchatomic;
	struct {
	    ptl_handle_md_t get_md_handle;
	    ptl_size_t local_get_offset;
	    ptl_handle_md_t put_md_handle;
	    ptl_size_t local_put_offset;
	    ptl_size_t length;
	    void *operand;
	    ptl_op_t operation;
	    ptl_datatype_t datatype;
	} swap;
    } info;
} ptl_internal_operation_t;

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
    ptl_internal_operation_t *qme;
    const ptl_internal_handle_converter_t md = { md_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle)) {
	return PTL_ARG_INVALID;
    }
    switch(md.s.ni) {
	case 0: // Logical
	case 1: // Logical
	    if (PtlInternalLogicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
	case 2: // Physical
	case 3: // Physical
	    if (PtlInternalPhysicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
    }
#endif
    qme = malloc(sizeof(ptl_internal_operation_t));
    if (qme == NULL) {
#warning The spec does not specify what happens when memory allocation fails in PtlPut(); assuming PTL_FAIL
	return PTL_FAIL; // unspecified in the spec
    }
    qme->type = PUT;
    qme->info.put.md_handle = md_handle;
    qme->info.put.local_offset = local_offset;
    qme->info.put.length = length;
    qme->info.put.ack_req = ack_req;
    qme->target_id = target_id;
    qme->pt_index = pt_index;
    qme->match_bits = match_bits;
    qme->remote_offset = remote_offset;
    qme->user_ptr = user_ptr;
    qme->hdr_data = hdr_data;
    PtlInternalQueueAppend(&inflight_ops, qme);
    return PTL_OK;
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
    ptl_internal_operation_t *qme;
    const ptl_internal_handle_converter_t md = { md_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle)) {
	return PTL_ARG_INVALID;
    }
    switch(md.s.ni) {
	case 0: // Logical
	case 1: // Logical
	    if (PtlInternalLogicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
	case 2: // Physical
	case 3: // Physical
	    if (PtlInternalPhysicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
    }
#endif
    qme = malloc(sizeof(ptl_internal_operation_t));
    if (qme == NULL) {
#warning The spec does not specify what happens when memory allocation fails in PtlGet(); assuming PTL_FAIL
	return PTL_FAIL; // unspecified in the spec
    }
    qme->type = GET;
    qme->info.get.md_handle = md_handle;
    qme->info.get.local_offset = local_offset;
    qme->info.get.length = length;
    qme->target_id = target_id;
    qme->pt_index = pt_index;
    qme->match_bits = match_bits;
    qme->remote_offset = remote_offset;
    qme->user_ptr = user_ptr;
    PtlInternalQueueAppend(&inflight_ops, qme);
    return PTL_OK;
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
    ptl_internal_operation_t *qme;
    const ptl_internal_handle_converter_t md = { md_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle)) {
	return PTL_ARG_INVALID;
    }
    switch(md.s.ni) {
	case 0: // Logical
	case 1: // Logical
	    if (PtlInternalLogicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
	case 2: // Physical
	case 3: // Physical
	    if (PtlInternalPhysicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
    }
#endif
    qme = malloc(sizeof(ptl_internal_operation_t));
    if (qme == NULL) {
#warning The spec does not specify what happens when memory allocation fails in PtlAtomic(); assuming PTL_FAIL
	return PTL_FAIL; // unspecified in the spec
    }
    qme->type = ATOMIC;
    qme->info.atomic.md_handle = md_handle;
    qme->info.atomic.local_offset = local_offset;
    qme->info.atomic.length = length;
    qme->info.atomic.ack_req = ack_req;
    qme->info.atomic.operation = operation;
    qme->info.atomic.datatype = datatype;
    qme->target_id = target_id;
    qme->pt_index = pt_index;
    qme->match_bits = match_bits;
    qme->remote_offset = remote_offset;
    qme->user_ptr = user_ptr;
    qme->hdr_data = hdr_data;
    PtlInternalQueueAppend(&inflight_ops, qme);
    return PTL_OK;
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
    ptl_internal_operation_t *qme;
    const ptl_internal_handle_converter_t get_md = { get_md_handle };
    const ptl_internal_handle_converter_t put_md = { put_md_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(get_md_handle)) {
	return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(put_md_handle)) {
	return PTL_ARG_INVALID;
    }
    if (get_md.s.ni != put_md.s.ni) {
#warning The spec does not specify what happens if get_md and put_md are on different NIs
	return PTL_ARG_INVALID;
    }
    switch(get_md.s.ni) {
	case 0: // Logical
	case 1: // Logical
	    if (PtlInternalLogicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
	case 2: // Physical
	case 3: // Physical
	    if (PtlInternalPhysicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
    }
#endif
    qme = malloc(sizeof(ptl_internal_operation_t));
    if (qme == NULL) {
#warning The spec does not specify what happens when memory allocation fails in PtlFetchAtomic(); assuming PTL_FAIL
	return PTL_FAIL; // unspecified in the spec
    }
    qme->type = FETCHATOMIC;
    qme->info.fetchatomic.get_md_handle = get_md_handle;
    qme->info.fetchatomic.local_get_offset = local_get_offset;
    qme->info.fetchatomic.put_md_handle = put_md_handle;
    qme->info.fetchatomic.local_put_offset = local_put_offset;
    qme->info.fetchatomic.length = length;
    qme->info.fetchatomic.operation = operation;
    qme->info.fetchatomic.datatype = datatype;
    qme->target_id = target_id;
    qme->pt_index = pt_index;
    qme->match_bits = match_bits;
    qme->remote_offset = remote_offset;
    qme->user_ptr = user_ptr;
    qme->hdr_data = hdr_data;
    PtlInternalQueueAppend(&inflight_ops, qme);
    return PTL_OK;
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
    ptl_internal_operation_t *qme;
    const ptl_internal_handle_converter_t get_md = { get_md_handle };
    const ptl_internal_handle_converter_t put_md = { put_md_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(get_md_handle)) {
	return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(put_md_handle)) {
	return PTL_ARG_INVALID;
    }
    if (get_md.s.ni != put_md.s.ni) {
#warning The spec does not specify what happens if get_md and put_md are on different NIs
	return PTL_ARG_INVALID;
    }
    switch(get_md.s.ni) {
	case 0: // Logical
	case 1: // Logical
	    if (PtlInternalLogicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
	case 2: // Physical
	case 3: // Physical
	    if (PtlInternalPhysicalProcessValidator(target_id)) {
		return PTL_ARG_INVALID;
	    }
	    break;
    }
#endif
    qme = malloc(sizeof(ptl_internal_operation_t));
    if (qme == NULL) {
#warning The spec does not specify what happens when memory allocation fails in PtlSwap(); assuming PTL_FAIL
	return PTL_FAIL; // unspecified in the spec
    }
    qme->type = SWAP;
    qme->info.swap.get_md_handle = get_md_handle;
    qme->info.swap.local_get_offset = local_get_offset;
    qme->info.swap.put_md_handle = put_md_handle;
    qme->info.swap.local_put_offset = local_put_offset;
    qme->info.swap.length = length;
    qme->info.swap.operand = operand;
    qme->info.swap.operation = operation;
    qme->info.swap.datatype = datatype;
    qme->target_id = target_id;
    qme->pt_index = pt_index;
    qme->match_bits = match_bits;
    qme->remote_offset = remote_offset;
    qme->user_ptr = user_ptr;
    qme->hdr_data = hdr_data;
    PtlInternalQueueAppend(&inflight_ops, qme);
    return PTL_OK;
}
