#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* The API definition */
#include <portals4.h>

/* Internals */
#include "ptl_visibility.h"
#ifndef NO_ARG_VALIDATION
# include "ptl_internal_error.h"
# include "ptl_internal_nit.h"
# include "ptl_internal_commpad.h"
#endif

int API_FUNC PtlStartBundle(ptl_handle_ni_t ni_handle)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ni = { ni_handle };
    if (comm_pad == NULL) {
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

int API_FUNC PtlEndBundle(ptl_handle_ni_t ni_handle)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ni = { ni_handle };
    if (comm_pad == NULL) {
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
