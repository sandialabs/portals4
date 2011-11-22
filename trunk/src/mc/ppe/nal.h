
#ifndef MC_NAL_H
#define MC_NAL_H

#include "ppe/ppe.h"
#include "ppe/ptl_hdr.h"

enum { MD_CTX = 1, ME_CTX, LE_CTX  };

struct dm_ctx_t {               
    int             id;
    unsigned long   nal_msg_data;
    ptl_hdr_t       hdr;        
    void           *user_ptr;   
    ptl_md_iovec_t  iovec;
    ptl_ppe_ni_t   *ppe_ni;
    ptl_ppe_pt_t   *ppe_pt;
    union {
        ptl_ppe_md_t   *ppe_md;
        ptl_ppe_me_t   *ppe_me;
        ptl_ppe_le_t   *ppe_le;
    } u;
};                              
typedef struct dm_ctx_t dm_ctx_t; 

extern lib_ni_t *_p3_ni;

int nal_init( ptl_ppe_t* );

#endif
