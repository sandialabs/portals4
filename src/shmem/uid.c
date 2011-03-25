#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* The API definition */
#include <portals4.h>

/* System headers */
#include <unistd.h>

/* Internals */
#include "ptl_visibility.h"
#ifndef NO_ARG_VALIDATION
# include "ptl_internal_error.h"
# include "ptl_internal_nit.h"
# include "ptl_internal_commpad.h"
#endif

int PtlGetUid(ptl_handle_ni_t ni_handle,
              ptl_uid_t      *uid)
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
    if (uid == NULL) {
        VERBOSE_ERROR("uid is a null pointer\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    *uid = geteuid();
    return PTL_OK;
}

/* vim:set expandtab: */
