/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */

/* Internals */
#include "ptl_internal_commpad.h"
#include "ptl_internal_pid.h"
#include "ptl_internal_nit.h"
#include "ptl_visibility.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_error.h"

int INTERNAL PtlInternalLogicalProcessValidator(
    ptl_process_t p)
{
    return (p.rank >= num_siblings);
}

int INTERNAL PtlInternalPhysicalProcessValidator(
    ptl_process_t p)
{
    /* pid == num_siblings is the COLLECTOR */
    return (p.phys.pid > num_siblings || p.phys.nid != 0);
}

int API_FUNC PtlGetId(
    ptl_handle_ni_t ni_handle,
    ptl_process_t * id)
{
    ptl_internal_handle_converter_t ni = { ni_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("bad NI\n");
        return PTL_ARG_INVALID;
    }
#endif
    switch (ni.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            id->rank = proc_number;    // heh
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            id->phys.pid = proc_number;
            id->phys.nid = 0;
            break;
        default:
            UNREACHABLE;
            *(int *)0 = 0;             // should never happen
    }
    return PTL_OK;
}
