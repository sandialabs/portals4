#include "config.h"

#include "portals4.h"
#include "ppe_if.h"

#include "shared/ptl_internal_handles.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"


int PtlStartBundle(ptl_handle_ni_t ni_handle)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ni = { ni_handle };
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_OK;

}

int PtlEndBundle(ptl_handle_ni_t ni_handle)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ni = { ni_handle };
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_OK;

}

/* vim:set expandtab */

