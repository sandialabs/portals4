#include "config.h"

#include "portals4.h"

#include "ptl_internal_iface.h"        
#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_EQ.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

int PtlPTAlloc(ptl_handle_ni_t ni_handle,
               unsigned int    options,
               ptl_handle_eq_t eq_handle,
               ptl_pt_index_t  pt_index_req,
               ptl_pt_index_t *pt_index)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        VERBOSE_ERROR("Invalid NI passed to PtlPTAlloc\n");
        return PTL_ARG_INVALID;
    }
    if (options & ~PTL_PT_ALLOC_OPTIONS_MASK) {
        VERBOSE_ERROR("Invalid options to PtlPTAlloc (0x%x)\n", options);
        return PTL_ARG_INVALID;
    }
    if ((eq_handle == PTL_EQ_NONE) && options & PTL_PT_FLOWCTRL) {
        return PTL_PT_EQ_NEEDED;
    }
    if (PtlInternalEQHandleValidator(eq_handle, 1)) {
        VERBOSE_ERROR("Invalid EQ passed to PtlPTAlloc\n");
        return PTL_ARG_INVALID;
    }
    if ((pt_index_req > nit_limits[ni.s.ni].max_pt_index) && (pt_index_req != PTL_PT_ANY)) {
        VERBOSE_ERROR("Invalid pt_index request passed to PtlPTAlloc\n");
        return PTL_ARG_INVALID;
    }
    if (pt_index == NULL) {
        VERBOSE_ERROR("Invalid pt_index pointer (NULL) passed to PtlPTAlloc\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );

    entry->type = PTLPTALLOC;
    entry->u.ptAlloc.ni_handle = ni;
    entry->u.ptAlloc.eq_handle = ( ptl_internal_handle_converter_t ) eq_handle;

    ptl_cq_entry_send( ptl_iface_get_cq(&ptl_iface), ptl_iface_get_peer(&ptl_iface), entry,
                                    sizeof(ptl_cqe_t) );

    return PTL_OK;
}

int PtlPTFree(ptl_handle_ni_t ni_handle,
              ptl_pt_index_t  pt_index)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("Not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        VERBOSE_ERROR
            ("ni.s.ni too big (%u >= 4) or ni.s.code wrong (%u != 0) or nit not initialized\n",
            ni.s.ni, ni.s.code);
        return PTL_ARG_INVALID;
    }
    if (pt_index == PTL_PT_ANY) {
        VERBOSE_ERROR("pt_index is PTL_PT_ANY\n");
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index is too big (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( ptl_iface_get_cq(&ptl_iface), &entry );

    entry->type = PTLPTFREE;
    entry->u.ptFree.ni_handle = ni;
    entry->u.ptFree.pt_index = pt_index;

    ptl_cq_entry_send( ptl_iface_get_cq(&ptl_iface), ptl_iface_get_peer(&ptl_iface), entry,
                                    sizeof(ptl_cqe_t) );

    return PTL_OK;
}


int PtlPTDisable(ptl_handle_ni_t ni_handle,
                 ptl_pt_index_t  pt_index)
{
    fprintf(stderr, "PtlPTDisable() unimplemented\n");
    return PTL_FAIL;

}


int PtlPTEnable(ptl_handle_ni_t ni_handle,
                ptl_pt_index_t  pt_index)

{
    fprintf(stderr, "PtlPTEnable() unimplemented\n");
    return PTL_FAIL;
}
