#include "config.h"

#include <unistd.h>
#include <sys/types.h>

#include "portals4.h"
        
#include "ptl_internal_iface.h"
#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h" 
#include "ptl_internal_pid.h"
#include "shared/ptl_internal_handles.h"


int
PtlGetUid(ptl_handle_ni_t ni_handle,
          ptl_uid_t      *uid)
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
    if (uid == NULL) {
        VERBOSE_ERROR("uid is a null pointer\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    *uid = ptl_iface.uid;

    return PTL_OK;
}


int
PtlGetId(ptl_handle_ni_t ni_handle,
         ptl_process_t  *id)
{
    ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_ni_t *nit;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("bad NI\n");
        return PTL_ARG_INVALID;
    }
#endif

    nit = &ptl_iface.ni[ni.s.ni];

    switch (ni.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            id->rank = *(nit->logical_address);
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            id->phys = nit->physical_address->phys;
            break;
        default:
            UNREACHABLE;
            abort();
    }

    return PTL_OK;
}


int
PtlGetPhysId(ptl_handle_ni_t ni_handle,
             ptl_process_t  *id)
{
    ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_ni_t *nit;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("bad NI\n");
        return PTL_ARG_INVALID;
    }
#endif

    nit = &ptl_iface.ni[ni.s.ni];

    id->phys = nit->physical_address->phys;

    return PTL_OK;
}

#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalLogicalProcessValidator(ptl_process_t p)
{
    return PTL_OK;
}


int INTERNAL PtlInternalPhysicalProcessValidator(ptl_process_t p)
{
    return PTL_OK;
}
#endif /* ifndef NO_ARG_VALIDATION */