
#ifndef MC_NAL_H
#define MC_NAL_H

#include "ppe/ppe.h"
#include "ppe/ptl_hdr.h"

enum { MD_CTX = 1, LE_CTX, DROP_CTX };

struct nal_ctx_t {
    int                   type;
    ptl_ppe_ni_t         *ppe_ni;
    lib_ni_t             *p3_ni;
    ptl_nid_t             src_nid;
    unsigned long         nal_msg_data;
    ptl_internal_header_t hdr; 
    ptl_md_iovec_t        iovec;

    union {
        struct {
            void           *user_ptr;
            ptl_ppe_md_t   *ppe_md;
        } md;
        struct {
            ptl_size_t      mlength;
            ptl_ppe_le_t   *ppe_le;
        } le;
    } u;
};
typedef struct nal_ctx_t nal_ctx_t;

extern lib_ni_t *_p3_ni;

int nal_init( ptl_ppe_t* );

#endif
