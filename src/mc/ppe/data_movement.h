
#ifndef MC_PPE_DATA_MOVEMENT_H
#define MC_PPE_DATA_MOVEMENT_H

#include <portals4.h>

struct ack_ctx_t {
    ptl_handle_generic_t md_h;
    ptl_size_t           local_offset;
    void                *user_ptr;
};
    
typedef struct ack_ctx_t ack_ctx_t;

int alloc_ack_ctx( ptl_handle_generic_t md_h, ptl_size_t local_offset, 
                void *user_ptr );
void free_ack_ctx( int, ack_ctx_t* );

#endif
