#include "config.h"

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

int PtlMEAppend(ptl_handle_ni_t  ni_handle,
                ptl_pt_index_t   pt_index,
                ptl_me_t        *me,
                ptl_list_t       ptl_list,
                void            *user_ptr,
                ptl_handle_me_t *me_handle)
{
    const ptl_internal_handle_converter_t ni  = { ni_handle };
    ptl_internal_handle_converter_t me_hc     = { .s.ni = ni.s.ni,
                                            .s.selector = HANDLE_ME_CODE };
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
    if ((ni.s.ni == 1) || (ni.s.ni == 3)) { // must be a non-matching NI
        VERBOSE_ERROR("must be a matching NI\n");
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    me_hc.s.code = find_me_index( ni.s.ni );
    if ( me_hc.s.code == - 1 ) return PTL_LIST_TOO_LONG;

    ret = ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );
    if ( 0 != ret ) return PTL_FAIL;

    entry->base.type          = PTLMEAPPEND;
    entry->base.remote_id     = ptl_iface_get_rank(&ptl_iface);
    entry->meAppend.pt_index  = pt_index;
    entry->meAppend.me        = *me;
    entry->meAppend.list      = ptl_list;
    entry->meAppend.user_ptr  = user_ptr;
    entry->meAppend.me_handle = (ptl_internal_handle_converter_t) me_hc.a;

    ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), 
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_meappend_t));

    *me_handle = me_hc.a;

    return PTL_OK;
}

int PtlMEUnlink(ptl_handle_me_t me_handle)
{
    const ptl_internal_handle_converter_t         me_hc = { me_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized");
        return PTL_NO_INIT;
    }
    if ((me_hc.s.ni > 3) || 
        (me_hc.s.code > nit_limits[me_hc.s.ni].max_entries) ||
        (ptl_iface.ni[me_hc.s.ni].refcount == 0)) 
    {
        VERBOSE_ERROR("ME Handle has bad NI (%u > 3) or bad code (%u > %u)"
                        " or the NIT is uninitialized\n",
                me_hc.s.ni, me_hc.s.code, nit_limits[me_hc.s.ni].max_entries);
        return PTL_ARG_INVALID;
    }
    __sync_synchronize();
    if ( me_is_free(me_hc.s.ni,me_hc.s.code ) ) {
        VERBOSE_ERROR("ME appears to be free already\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );
    
    entry->base.type          = PTLMEUNLINK;
    entry->base.remote_id     = ptl_iface_get_rank(&ptl_iface);
    entry->meUnlink.me_handle = me_hc;

    ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), 
                      ptl_iface_get_peer(&ptl_iface),
                      entry, sizeof(ptl_cqe_meunlink_t));

    return PTL_OK;
}


int PtlMESearch(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index,
                const ptl_me_t *me,
                ptl_search_op_t ptl_search_op,
                void           *user_ptr)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };

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

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );

    entry->base.type = PTLMESEARCH;
    entry->base.remote_id  = ptl_iface_get_rank(&ptl_iface);
    entry->meSearch.ni_handle = ni;
    entry->meSearch.pt_index = pt_index;
    entry->meSearch.me = *me;
    entry->meSearch.ptl_search_op = ptl_search_op;
    entry->meSearch.user_ptr = user_ptr;

    ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), 
                      ptl_iface_get_peer(&ptl_iface),
                      entry, sizeof(ptl_cqe_mesearch_t));

    return PTL_OK;
}
