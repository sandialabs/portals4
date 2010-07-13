/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>

/* Internals */
#include "ptl_visibility.h"

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
