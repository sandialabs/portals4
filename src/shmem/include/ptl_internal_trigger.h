#ifndef PTL_INTERNAL_TRIGGER_H
#define PTL_INTERNAL_TRIGGER_H

#include "ptl_visibility.h"

typedef enum {
    PUT,
    GET,
    ATOMIC,
    FETCHATOMIC,
    SWAP,
    CTINC,
    CTSET
} ptl_internal_trigtype_t;

typedef struct ptl_internal_trigger_s {
    volatile struct ptl_internal_trigger_s *next; // this is for the pool of triggers
    ptl_size_t                              next_threshold;
    ptl_size_t                              threshold;
    ptl_internal_trigtype_t                 type;
    union {
        struct {
            ptl_handle_md_t  md_handle;
            ptl_size_t       local_offset;
            ptl_size_t       length;
            ptl_ack_req_t    ack_req;
            ptl_process_t    target_id;
            ptl_pt_index_t   pt_index;
            ptl_match_bits_t match_bits;
            ptl_size_t       remote_offset;
            const void      *user_ptr;
            ptl_hdr_data_t   hdr_data;
        } put;
        struct {
            ptl_handle_md_t  md_handle;
            ptl_size_t       local_offset;
            ptl_size_t       length;
            ptl_process_t    target_id;
            ptl_pt_index_t   pt_index;
            ptl_match_bits_t match_bits;
            ptl_size_t       remote_offset;
            const void      *user_ptr;
        } get;
        struct {
            ptl_handle_md_t  md_handle;
            ptl_size_t       local_offset;
            ptl_size_t       length;
            ptl_ack_req_t    ack_req;
            ptl_process_t    target_id;
            ptl_pt_index_t   pt_index;
            ptl_match_bits_t match_bits;
            ptl_size_t       remote_offset;
            const void      *user_ptr;
            ptl_hdr_data_t   hdr_data;
            ptl_op_t         operation;
            ptl_datatype_t   datatype;
        } atomic;
        struct {
            ptl_handle_md_t  get_md_handle;
            ptl_size_t       local_get_offset;
            ptl_handle_md_t  put_md_handle;
            ptl_size_t       local_put_offset;
            ptl_size_t       length;
            ptl_process_t    target_id;
            ptl_pt_index_t   pt_index;
            ptl_match_bits_t match_bits;
            ptl_size_t       remote_offset;
            const void      *user_ptr;
            ptl_hdr_data_t   hdr_data;
            ptl_op_t         operation;
            ptl_datatype_t   datatype;
        } fetchatomic;
        struct {
            ptl_handle_md_t  get_md_handle;
            ptl_size_t       local_get_offset;
            ptl_handle_md_t  put_md_handle;
            ptl_size_t       local_put_offset;
            ptl_size_t       length;
            ptl_process_t    target_id;
            ptl_pt_index_t   pt_index;
            ptl_match_bits_t match_bits;
            ptl_size_t       remote_offset;
            const void      *user_ptr;
            ptl_hdr_data_t   hdr_data;
            unsigned char    operand[32];
            ptl_op_t         operation;
            ptl_datatype_t   datatype;
        } swap;
        struct {
            ptl_handle_ct_t ct_handle;
            ptl_ct_event_t  increment;
        } ctinc;
        struct {
            ptl_handle_ct_t ct_handle;
            ptl_ct_event_t  newval;
        } ctset;
    } args;
} ptl_internal_trigger_t;

void INTERNAL PtlInternalTriggerPull(ptl_internal_trigger_t *t);

#endif /* ifndef PTL_INTERNAL_TRIGGER_H */
/* vim:set expandtab: */
