
#ifndef MC_NAL_H
#define MC_NAL_H

#include "ppe/ppe.h"
#include "ppe/ptl_hdr.h"

struct dm_ctx_t {               
    unsigned long   nal_msg_data;
    ptl_hdr_t       hdr;        
    void           *user_ptr;   
    ptl_md_iovec_t  iovec;
};                              
typedef struct dm_ctx_t dm_ctx_t; 

int nal_init( ptl_ppe_t* );

#endif
