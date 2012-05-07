#ifndef PTL_INTERNAL_GLOBAL_H
#define PTL_INTERNAL_GLOBAL_H

#include "ptl_internal_nit.h"
#include "ptl_internal_iface.h"

static inline int
find_md_index(int ni)
{
    int index;
    for ( index = 0; index < nit_limits[ni].max_mds; index++ ) {
        if ( ! ptl_iface.ni[ni].i_md[ index ].in_use ) {
            ptl_iface.ni[ni].i_md[ index ].in_use = 1; 
            return index;
        }
    }
    return -1;
}

static inline int 
md_is_inuse(int ni, int md_index )
{
    return ( ptl_iface.ni[ni].i_md[ md_index ].in_use == 1 );
} 

static inline int
find_le_index(int ni)
{
    int index; 
    for ( index = 0; index < nit_limits[ni].max_list_size; index++ ) {
        if ( ! ptl_iface.ni[ni].i_le[ index ].in_use ) {
            ptl_iface.ni[ni].i_le[ index ].in_use = 1; 
            return index;
        }
    }
    return -1;
}

static inline int
le_is_free( int ni, int le_index )
{
    return ( ptl_iface.ni[ni].i_le[ le_index ].in_use != 1 );
}

static inline int
find_me_index(int ni)
{
    int index;
    for ( index = 0; index < nit_limits[ni].max_list_size; index++ ) {
        if ( ! ptl_iface.ni[ni].i_me[ index ].in_use ) {
            ptl_iface.ni[ni].i_me[ index ].in_use = 1; 
            return index;
        }
    }
    return -1;
}

static inline int
me_is_free( int ni, int me_index )
{
    return ( ptl_iface.ni[ni].i_me[ me_index ].in_use != 1 );
}

static inline int
find_pt_index(int ni)
{
    int index ;
    for ( index = 0; index < nit_limits[ni].max_pt_index; index++ ) {
        if ( ! ptl_iface.ni[ni].i_pt[ index ].in_use ) {
            ptl_iface.ni[ni].i_pt[ index ].in_use = 1; 
            return index;
        }
    }
    return -1;
}

static inline void 
mark_pt_inuse( int ni, int pt_index )
{
    ptl_iface.ni[ni].i_pt[ pt_index ].in_use = 1;
}

static inline int
pt_is_inuse( int ni, int pt_index )
{
    return ( ptl_iface.ni[ni].i_pt[ pt_index ].in_use == 1 );
}

static inline int
find_ct_index(int ni)
{
    int index;
    for ( index = 0; index < nit_limits[ni].max_cts; index++ ) {
        if ( ! ptl_iface.ni[ni].i_ct[ index ].in_use ) {
            ptl_iface.ni[ni].i_ct[ index ].in_use = 1; 
            return index;
        }
    }
    return -1;
}

static inline ptl_internal_ct_t*
get_ct( int ni, int ct_index ) 
{
    return ptl_iface.ni[ni].i_ct + ct_index;
} 


static inline int
find_eq_index(int ni)
{
    int index;
    for ( index = 0; index < nit_limits[ni].max_eqs; index++ ) {
        if ( ! ptl_iface.ni[ni].i_eq[ index ].in_use ) {
            ptl_iface.ni[ni].i_eq[ index ].in_use = 1; 
            return index;
        }
    }
    return -1;
}

static inline void
free_eq_index( int ni, int eq_index )
{
    ptl_iface.ni[ni].i_eq[ eq_index ].in_use = 0; 
}

static inline ptl_internal_eq_t*
get_eq( int ni, int eq_index ) 
{
    return ptl_iface.ni[ni].i_eq + eq_index;
} 

static inline int
find_triggered_index(int ni)
{
    int index;
    for ( index = 0; index < nit_limits[ni].max_triggered_ops; index++ ) {
        if ( ! ptl_iface.ni[ni].i_triggered[ index ].in_use ) {
            ptl_iface.ni[ni].i_triggered[ index ].in_use = 1; 
            return index;
        }
    }
    return -1;
}

static inline void
free_triggered_index( int ni, int eq_index )
{
    ptl_iface.ni[ni].i_triggered[ eq_index ].in_use = 0; 
}

#endif
/* vim:set expandtab: */
