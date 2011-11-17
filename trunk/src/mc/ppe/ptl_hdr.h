
#ifndef MC_PTL_HDR_T 
#define MC_PTL_HDR_T 

#include <portals4.h>

struct ptl_hdr_t {
    ptl_match_bits_t    match_bits;
    ptl_pt_index_t      pt_index; 
    ptl_process_t           src_id;
    ptl_process_t       target_id;
    ptl_size_t          length;
    ptl_size_t          remote_offset;
    ptl_hdr_data_t      hdr_data;
    ptl_ack_req_t       ack_req;
    unsigned char       ni;
};

typedef struct ptl_hdr_t ptl_hdr_t;

#endif
