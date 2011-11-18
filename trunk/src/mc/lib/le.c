#include "config.h"

#include <assert.h>

#include "portals4.h"

#include "ptl_internal_iface.h"        
#include "ptl_internal_global.h"        
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

int PtlLEAppend(ptl_handle_ni_t  ni_handle,
                ptl_pt_index_t   pt_index,
                const ptl_le_t  *le,
                ptl_list_t       ptl_list,
                void            *user_ptr,
                ptl_handle_le_t *le_handle)
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

    entry->base.type = PTLLEAPPEND;
    entry->base.remote_id  = ptl_iface_get_rank(&ptl_iface);
    entry->leAppend.le_handle = le_hc;
    entry->leAppend.pt_index  = pt_index;
    entry->leAppend.le        = *le;
    entry->leAppend.list      = ptl_list;
    entry->leAppend.user_ptr  = user_ptr;

    ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface),
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_leappend_t));

    *le_handle = le_hc.a; 
    return PTL_OK;
}

int PtlLEUnlink(ptl_handle_le_t le_handle)
{
   const ptl_internal_handle_converter_t le_hc = { le_handle };
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

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );

    entry->base.type = PTLLEUNLINK;
    entry->base.remote_id  = ptl_iface_get_rank(&ptl_iface);
    entry->leUnlink.le_handle = le_hc;

    ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface),
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_leunlink_t));

    return PTL_OK;
}

int PtlLESearch(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index,
                const ptl_le_t *le,
                ptl_search_op_t ptl_search_op,
                void           *user_ptr)
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

    entry->base.type = PTLLESEARCH;
    entry->base.remote_id  = ptl_iface_get_rank(&ptl_iface);
    entry->leSearch.ni_handle = ni;
    entry->leSearch.pt_index = pt_index;
    entry->leSearch.le = *le;
    entry->leSearch.ptl_search_op = ptl_search_op;
    entry->leSearch.user_ptr = user_ptr;

    ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface),
                      ptl_iface_get_peer(&ptl_iface), 
                      entry, sizeof(ptl_cqe_lesearch_t));

    return PTL_OK;
}
