/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */

/* Internals */
#include "ptl_visibility.h"

int API_FUNC PtlHandleIsEqual(ptl_handle_any_t handle1,
                              ptl_handle_any_t handle2)
{
    if ((uint32_t)handle1 == (uint32_t)handle2) {
        return PTL_OK;
    } else {
        return PTL_FAIL;
    }
}

/* vim:set expandtab */
