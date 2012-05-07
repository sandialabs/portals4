#include "config.h"

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_startup.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

#ifndef NO_ARG_VALIDATION

static inline int validate_args( int type, const ptl_internal_handle_converter_t ni, 
                        int pt_index )
{
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || 
                            (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if ( type == PTLMEAPPEND ) { 
        if ((ni.s.ni == 1) || (ni.s.ni == 3)) { // must be a matching NI
            VERBOSE_ERROR("must be a matching NI\n");
            return PTL_ARG_INVALID;
        }
    } else {
        if ((ni.s.ni == 0) || (ni.s.ni == 2)) { // must be a non-matching NI
            VERBOSE_ERROR("must be a non-matching NI\n");
            return PTL_ARG_INVALID;
        }
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    return PTL_OK;
}
#endif /* ifndef NO_ARG_VALIDATION */

static inline void copy_entry( int type, ptl_cqe_me_t *me, void *obj )
{
    if ( type == PTLMEAPPEND ) {
        ptl_me_t *ptl_me = (ptl_me_t *) obj;
        me->le.start      = ptl_me->start;
        me->le.length     = ptl_me->length;
        me->le.ct_handle  = ptl_me->ct_handle;
        me->le.uid        = ptl_me->uid;
        me->le.options    = ptl_me->options;
        me->match_id      = ptl_me->match_id;
        me->match_bits    = ptl_me->match_bits;
        me->ignore_bits   = ptl_me->ignore_bits;
        me->min_free      = ptl_me->min_free;
    } else {
        ptl_le_t *ptl_le = (ptl_le_t *) obj;
        me->le.start      = ptl_le->start;
        me->le.length     = ptl_le->length;
        me->le.ct_handle  = ptl_le->ct_handle;
        me->le.uid        = ptl_le->uid;
        me->le.options    = ptl_le->options;
    }
}

static inline int 
list_append( 
        int             type,
        ptl_handle_ni_t ni_handle,
        ptl_pt_index_t  pt_index,
        void*           *obj,
        ptl_list_t      ptl_list,
        void            *user_ptr,
        ptl_internal_handle_converter_t *handle
)
{
    const ptl_internal_handle_converter_t ni  = { ni_handle };
    ptl_cqe_t *entry;
    int ret; 

    
#ifndef NO_ARG_VALIDATION
    ret = validate_args( type, ni, pt_index );
    if ( ret != PTL_OK ) return ret;
#endif /* ifndef NO_ARG_VALIDATION */

    handle->s.ni       = ni.s.ni;
    handle->s.selector = type == PTLMEAPPEND ? HANDLE_ME_CODE : HANDLE_LE_CODE;
    handle->s.code = find_me_index( ni.s.ni );
    if ( handle->s.code == - 1 ) return PTL_LIST_TOO_LONG;

    ret = ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );
    if ( 0 != ret ) return PTL_FAIL;

    entry->base.type            = type;
    entry->base.remote_id       = ptl_iface_get_rank(&ptl_iface);
    entry->list_append.pt_index = pt_index;
    entry->list_append.ptl_list = ptl_list;
    entry->list_append.user_ptr = user_ptr;
    entry->list_append.entry_handle   = *handle;

    copy_entry( type, &entry->list_append.me, obj );

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_meappend_t));
    if ( 0 != ret ) return PTL_FAIL;

    return PTL_OK;
}

int PtlMEAppend(
        ptl_handle_ni_t  ni_handle,
        ptl_pt_index_t   pt_index,
        const ptl_me_t  *me,
        ptl_list_t       ptl_list,
        void            *user_ptr,
        ptl_handle_me_t *me_handle
)
{
    return list_append( PTLMEAPPEND, ni_handle, pt_index, (void*) me, ptl_list,
                    user_ptr, (ptl_internal_handle_converter_t*) me_handle );
}

int PtlLEAppend(
        ptl_handle_ni_t  ni_handle,
        ptl_pt_index_t   pt_index,
        const ptl_le_t  *le,
        ptl_list_t       ptl_list,
        void            *user_ptr,
        ptl_handle_le_t *le_handle
)
{
    return list_append( PTLLEAPPEND, ni_handle, pt_index, (void*) le, ptl_list,
                    user_ptr, (ptl_internal_handle_converter_t*) le_handle );
}

static inline int 
list_unlink( int type, const ptl_internal_handle_converter_t handle )
{
    ptl_cqe_t *entry;
    int ret, cmd_ret = PTL_STATUS_LAST;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized");
        return PTL_NO_INIT;
    }
    if ((handle.s.ni > 3) || 
        (handle.s.code > nit_limits[handle.s.ni].max_entries) ||
        (ptl_iface.ni[handle.s.ni].refcount == 0)) 
    {
        VERBOSE_ERROR("Entry Handle has bad NI (%u > 3) or bad code (%u > %u)"
                        " or the NIT is uninitialized\n",
                handle.s.ni, handle.s.code, 
                                nit_limits[handle.s.ni].max_entries);
        return PTL_ARG_INVALID;
    }
    __sync_synchronize();
    if ( me_is_free(handle.s.ni, handle.s.code ) ) {
        VERBOSE_ERROR("Entry appears to be free already\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */


    ret = ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );
    if (0 != ret) return PTL_FAIL;
    
    entry->base.type                = type;
    entry->base.remote_id           = ptl_iface_get_rank(&ptl_iface);
    entry->list_unlink.entry_handle = handle;
    entry->list_unlink.retval_ptr   = &cmd_ret;

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                      ptl_iface_get_peer(&ptl_iface),
                      entry, sizeof(ptl_cqe_meunlink_t));
    if (ret < 0) return PTL_FAIL;

    do {
        ret = ptl_ppe_progress(&ptl_iface, 1);
        if (ret < 0) return PTL_FAIL;
        __sync_synchronize();
    } while (PTL_STATUS_LAST == cmd_ret);

    return cmd_ret;
}

int PtlLEUnlink(ptl_handle_le_t le_handle)
{
    return list_unlink( PTLLEUNLINK,
                 (const ptl_internal_handle_converter_t) le_handle );
}

int PtlMEUnlink(ptl_handle_me_t me_handle)
{
    return list_unlink( PTLMEUNLINK,
                (const ptl_internal_handle_converter_t) me_handle );
}

static inline int 
list_search(
        int type,
        ptl_handle_ni_t ni_handle,
        ptl_pt_index_t  pt_index,
        void           *obj,
        ptl_search_op_t ptl_search_op,
        void           *user_ptr
)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    int ret;
    ptl_cqe_t *entry;

#ifndef NO_ARG_VALIDATION
    ret = validate_args( type, ni, pt_index );
    if ( ret != PTL_OK ) return ret;
#endif /* ifndef NO_ARG_VALIDATION */


    ret = ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );
    if ( 0 != ret ) return PTL_FAIL;;

    entry->base.type                    = PTLMESEARCH;
    entry->base.remote_id               = ptl_iface_get_rank(&ptl_iface);
    entry->list_search.ni_handle        = ni;
    entry->list_search.pt_index         = pt_index;
    entry->list_search.ptl_search_op    = ptl_search_op;
    entry->list_search.user_ptr         = user_ptr;

    copy_entry( type, &entry->list_append.me, obj );

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface), 
                      ptl_iface_get_peer(&ptl_iface),
                      entry, sizeof(ptl_cqe_mesearch_t));

    if ( 0 != ret ) return PTL_FAIL;;

    return PTL_OK;
}

int PtlMESearch(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index,
                const ptl_me_t *me,
                ptl_search_op_t ptl_search_op,
                void           *user_ptr)
{
    return list_search( PTLMESEARCH, ni_handle, pt_index, (void*) me,
                                        ptl_search_op, user_ptr );
}

int PtlLESearch(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index,
                const ptl_le_t *le,
                ptl_search_op_t ptl_search_op,
                void           *user_ptr)
{
    return list_search( PTLLESEARCH, ni_handle, pt_index, (void*) le,
                                        ptl_search_op, user_ptr );
}

