#if 0
#include "config.h"

#include <assert.h>

#include "portals4.h"

#include "ptl_internal_iface.h"        
#include "ptl_internal_global.h"        
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_startup.h"        
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

union le_me_t {
    ptl_le_t le;
    ptl_me_t me;
};
typedef union le_me_t le_me_t;

static inline int 
le_append(
        int                 type, 
        ptl_handle_ni_t     ni_handle,
        ptl_pt_index_t      pt_index,
        const le_me_t      *le_me,
        ptl_list_t          ptl_list,
        void                *user_ptr,
        ptl_handle_le_t     *le_handle
)
{
    const ptl_internal_handle_converter_t ni     = { ni_handle };
    ptl_internal_handle_converter_t le_hc     = { .s.ni = ni.s.ni,
                                            .s.selector = HANDLE_LE_CODE };
    ptl_cqe_t *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || 
                        (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni == 0) || (ni.s.ni == 2)) { // must be a non-matching NI
        VERBOSE_ERROR("must be a non-matching NI\n");
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    le_hc.s.code = find_le_index( ni.s.ni );
    if ( le_hc.s.code == -1 ) return PTL_LIST_TOO_LONG;

    ret = ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );
    if ( 0 != ret ) return PTL_FAIL;

#if 0
    entry->base.type            = type;
    entry->base.remote_id       = ptl_iface_get_rank(&ptl_iface);
    entry->lemeAppend.handle    = le_hc.s.code;
    entry->lemeAppend.pt_index  = pt_index;
    entry->lemeAppend.ptl_list  = ptl_list;
    entry->lemeAppend.user_ptr  = user_ptr;

    if ( type == PTLLEAPPEND ) {
        entry->lemeAppend.me.le.start      = le_me->le.start;
        entry->lemeAppend.me.le.length     = le_me->le.length;
        entry->lemeAppend.me.le.ct_handle  = le_me->le.ct_handle;
        entry->lemeAppend.me.le.uid        = le_me->le.uid;
        entry->lemeAppend.me.le.options    = le_me->le.options;
    } else {
        entry->lemeAppend.me.le.start      = le_me->me.start;
        entry->lemeAppend.me.le.length     = le_me->me.length;
        entry->lemeAppend.me.le.ct_handle  = le_me->me.ct_handle;
        entry->lemeAppend.me.le.uid        = le_me->me.uid;
        entry->lemeAppend.me.le.options    = le_me->me.options;
        entry->lemeAppend.me.match_id      = le_me->me.match_id;
        entry->lemeAppend.me.match_bits    = le_me->me.match_bits;
        entry->lemeAppend.me.ignore_bits   = le_me->me.ignore_bits;
        entry->lemeAppend.me.min_free      = le_me->me.min_free;
    }
#endif

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_leappend_t));
    if ( ret != 0 ) return PTL_FAIL;

    *le_handle = le_hc.a; 
    return PTL_OK;
}

int
PtlLEAppend(
        ptl_handle_ni_t     ni_handle,
        ptl_pt_index_t      pt_index,
        const ptl_le_t     *le,
        ptl_list_t          ptl_list,
        void               *user_ptr,
        ptl_handle_le_t    *le_handle
)
{
    return le_append( PTLLEAPPEND, ni_handle, pt_index, (le_me_t*) le, ptl_list,
                    user_ptr, le_handle );
}

static inline int 
le_unlink( int type, ptl_handle_le_t le_handle)
{
   const ptl_internal_handle_converter_t le_hc = { le_handle };
   ptl_cqe_t *entry;
   int ret, cmd_ret = PTL_STATUS_LAST;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((le_hc.s.ni > 3) || 
        (le_hc.s.code > nit_limits[le_hc.s.ni].max_entries) ||
        (ptl_iface.ni[le_hc.s.ni].refcount == 0)) 
    {
        VERBOSE_ERROR("LE Handle has bad NI (%u > 3) or bad code (%u > %u)"
            " or the NIT is uninitialized\n",
                  le_hc.s.ni, le_hc.s.code, nit_limits[le_hc.s.ni].max_entries);
        return PTL_ARG_INVALID;
    }
    __sync_synchronize();
    if ( le_is_free( le_hc.s.ni, le_hc.s.code ) ) {
        VERBOSE_ERROR("LE appears to be free already\n");
        return PTL_ARG_INVALID;
    }           
#endif /* ifndef NO_ARG_VALIDATION */

    ret = ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );
    if ( 0!= ret ) return PTL_FAIL;

    entry->base.type          = type;
    entry->base.remote_id     = ptl_iface_get_rank(&ptl_iface);
    entry->leUnlink.le_handle = le_hc;
    entry->leUnlink.retval_ptr = &cmd_ret;

    ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_leunlink_t));

    if ( ret < 0 ) return PTL_FAIL;

    do {
        ret = ptl_ppe_progress(&ptl_iface, 1);
        if (ret < 0) return PTL_FAIL;
        __sync_synchronize();
    } while (PTL_STATUS_LAST == cmd_ret);

    return PTL_OK;
}

int PtlLEUnlink(ptl_handle_le_t le_handle)
{
    return le_unlink( PTLLEUNLINK, le_handle );
}


static inline int 
le_search( 
        int                 type,
        ptl_handle_ni_t     ni_handle,
        ptl_pt_index_t      pt_index,
        const le_me_t     *le_me,
        ptl_search_op_t     ptl_search_op,
        void               *user_ptr
)
{

    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni == 0) || (ni.s.ni == 2)) { // must be a non-matching NI
        VERBOSE_ERROR("must be a non-matching NI\n");
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );

    entry->base.type            = type;
    entry->base.remote_id       = ptl_iface_get_rank(&ptl_iface);
    entry->leSearch.ni_handle   = ni;
    entry->leSearch.pt_index    = pt_index;
    if ( type == PTLLESEARCH ) {
        entry->leSearch.le          = le_me->le;
    } else {
    }
    entry->leSearch.ptl_search_op = ptl_search_op;
    entry->leSearch.user_ptr    = user_ptr;

    ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_lesearch_t));

    return PTL_OK;
}

int PtlLESearch(
        ptl_handle_ni_t     ni_handle,
        ptl_pt_index_t      pt_index,
        const ptl_le_t     *le,
        ptl_search_op_t     ptl_search_op,
        void               *user_ptr
)
{
    return le_search( PTLLESEARCH, ni_handle, pt_index, (le_me_t*) le, 
                            ptl_search_op, user_ptr);
}
#endif
