#ifndef PPE_IF_H
#define PPE_IF_H

#include <assert.h>
#include "func_call.h"

#define DEBUG
#ifndef DEBUG

#define __DBG( fmt, args...)

#else

#define __DBG( fmt, args...) \
fprintf(stderr,"%d:%s():%i: " fmt, -1, __FUNCTION__, __LINE__, ## args);

#endif

#include "command_queue.h"
#include "command_queue_entry.h"


struct ppe_if {
    ptl_cq_handle_t cq_handle; 
    int             tileColumn; 
    void*           sharedBase;
    ptl_ni_limits_t limits;
    ptl_md_t*       mdBase;
    int             mdFreeHint;
};

extern struct ppe_if ppe_if_data;

static inline ptl_cq_handle_t get_cq_handle( void ) 
{
    return ppe_if_data.cq_handle;
}

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

static inline int get_ppe_index( void )
{
    return ppe_if_data.tileColumn;
}

extern void ppe_if_init(void);

#include "ppe_if_ni.h"
#include "ppe_if_md.h"

#endif
