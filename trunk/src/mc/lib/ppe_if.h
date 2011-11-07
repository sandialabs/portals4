#ifndef PPE_IF_H
#define PPE_IF_H

#include <assert.h>

#define DEBUG
#ifndef DEBUG

#define __DBG( fmt, args...)

#else

#define __DBG( fmt, args...) \
fprintf(stderr,"%d:%s():%i: " fmt, -1, __FUNCTION__, __LINE__, ## args);

#endif

#include "shared/ptl_connection_manager.h"
#include "shared/ptl_command_queue.h"
#include "shared/ptl_command_queue_entry.h"

static inline int
PtlInternalLibraryInitialized(void)
{
    /* BWB: FIX ME! */
    return PTL_OK;
}

struct ppe_if {
    int             my_id; 
    void*           sharedBase;
    ptl_ni_limits_t limits;
    ptl_md_t*       mdBase;
    int             mdFreeHint;
    ptl_cm_client_handle_t cm_h;
    ptl_cq_handle_t cq_h;
    int my_ppe_rank;
};

extern struct ppe_if ppe_if_data;

static inline ptl_cq_handle_t get_cq_handle( void ) 
{
    return ppe_if_data.cq_h;
}

static inline int get_cq_peer(void) { return 0; }

static inline int find_md_index( int ni )
{
    int index = -1;
    int cnt = ppe_if_data.limits.max_mds;
    while ( cnt-- ) { 
        // use options as avail flag 
        if ( ! ppe_if_data.mdBase[ ppe_if_data.mdFreeHint  ].options ) {
            index = ppe_if_data.mdFreeHint;
            ++ppe_if_data.mdFreeHint;

            ppe_if_data.mdFreeHint %= ppe_if_data.limits.max_mds; 
            break;
        }
    } 
    assert( index != -1 );
    return index;
}
static inline int find_ct_index( int ni )
{
    return 0;
}

static inline int find_eq_index( int ni )
{
    return 0;
}

static inline int find_le_index( int ni )
{
    return 0;
}

static inline int find_me_index( int ni )
{
    return 0;
}

static inline int find_pt_index( int ni )
{
    return 0;
}

static inline int get_my_id( void )
{
    return ppe_if_data.my_id;
}

extern void ppe_if_init(void);

#endif
