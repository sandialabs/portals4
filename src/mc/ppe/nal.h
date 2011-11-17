
#ifndef MC_NAL_H
#define MC_NAL_H

#include "ppe/ppe.h"
#include "ppe/ptl_hdr.h"

enum { MD_CTX = 1, ME_CTX  };

struct dm_ctx_t {               
    int             id;
    unsigned long   nal_msg_data;
    ptl_hdr_t       hdr;        
    void           *user_ptr;   
    ptl_md_iovec_t  iovec;
    ptl_ppe_ni_t   *ni;
    union {
        ptl_ppe_md_t   *ppe_md;
        ptl_ppe_me_t   *ppe_me;
    } u;
};                              
typedef struct dm_ctx_t dm_ctx_t; 

int nal_init( ptl_ppe_t* );

#endif
