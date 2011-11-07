
#include "config.h"

#include "portals4.h"
#include "ppe_if.h"

#include "shared/ptl_internal_handles.h"
#include "ptl_internal_error.h"

#include "ptl_internal_nit.h"

int PtlSetMap(ptl_handle_ni_t      ni_handle,
              ptl_size_t           map_size,
              const ptl_process_t *mapping)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ni = { ni_handle };

    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("NI handle is invalid.\n");
        return PTL_ARG_INVALID;
    }
    if (ni.s.ni > 1) {
        VERBOSE_ERROR("NI handle is for physical interface\n");
        return PTL_ARG_INVALID;
    }
    if (map_size == 0) {
        VERBOSE_ERROR("Input map_size is zero\n");
        return PTL_ARG_INVALID;
    }
    if (mapping == NULL) {
        VERBOSE_ERROR("Input mapping is NULL\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    return PTL_FAIL;
}

int PtlGetMap(ptl_handle_ni_t ni_handle,
              ptl_size_t      map_size,
              ptl_process_t  *mapping,
              ptl_size_t     *actual_map_size)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ni = { ni_handle };

    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("NI handle is invalid.\n");
        return PTL_ARG_INVALID;
    }
    if ((map_size == 0) && ((mapping != NULL) || (actual_map_size == NULL))) {
        VERBOSE_ERROR("Input map_size is zero\n");
        return PTL_ARG_INVALID;
    }
    if ((mapping == NULL) && ((map_size != 0) || (actual_map_size == NULL))) {
        VERBOSE_ERROR("Output mapping ptr is NULL\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    return PTL_FAIL;
}
