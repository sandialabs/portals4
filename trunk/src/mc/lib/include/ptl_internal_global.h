#ifndef PTL_INTERNAL_GLOBAL_H
#define PTL_INTERNAL_GLOBAL_H

#include <assert.h>

#include "shared/ptl_connection_manager.h"
#include "shared/ptl_command_queue.h"
#include "shared/ptl_command_queue_entry.h"

struct ptl_global_t {
    int32_t         init_count;
    int             my_ppe_rank;
    void*           sharedBase;
    ptl_ni_limits_t limits;
    ptl_md_t*       mdBase;
    int             mdFreeHint;
    ptl_cm_client_handle_t cm_h;
    ptl_cq_handle_t cq_h;
};
typedef struct ptl_global_t ptl_global_t;
extern ptl_global_t ptl_global;


static inline int
PtlInternalLibraryInitialized(void)
{
    return (ptl_global.init_count > 0) ? PTL_OK : PTL_FAIL;
}

static inline ptl_cq_handle_t
get_cq_handle(void)
{
    return ptl_global.cq_h;
}

static inline int
get_cq_peer(void)
{
    return 0;
}

static inline int
find_md_index(int ni)
{
    int index = -1;
    int cnt = ptl_global.limits.max_mds;
    while ( cnt-- ) { 
        // use options as avail flag 
        if ( ! ptl_global.mdBase[ ptl_global.mdFreeHint  ].options ) {
            index = ptl_global.mdFreeHint;
            ++ptl_global.mdFreeHint;

            ptl_global.mdFreeHint %= ptl_global.limits.max_mds; 
            break;
        }
    } 
    assert( index != -1 );
    return index;
}

static inline int
find_ct_index(int ni)
{
    return 0;
}

static inline int
find_eq_index(int ni)
{
    return 0;
}

static inline int
find_le_index(int ni)
{
    return 0;
}

static inline int
find_me_index(int ni)
{
    return 0;
}

static inline int
find_pt_index(int ni)
{
    return 0;
}

static inline int
get_my_ppe_rank(void)
{
    return ptl_global.my_ppe_rank;
}

#endif
