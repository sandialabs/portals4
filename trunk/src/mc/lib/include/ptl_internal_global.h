#ifndef PTL_INTERNAL_GLOBAL_H
#define PTL_INTERNAL_GLOBAL_H

#include <assert.h>


static inline int
find_md_index(int ni)
{
#if 0
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
#endif
    return 0;
}

static inline int 
md_is_inuse(int ni, int md_index )
{
    return 0;
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
le_is_free( int ni, int le_index )
{
    return 0;
}

static inline int
find_me_index(int ni)
{
    return 0;
}

static inline int
me_is_free( int ni, int me_index )
{
    return 0;
}

static inline int
find_pt_index(int ni)
{
    return 0;
}

#endif
