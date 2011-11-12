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

    *uid = (ptl_uid_t) getuid();

    return PTL_OK;
}


int
PtlGetId(ptl_handle_ni_t ni_handle,
         ptl_process_t  *id)
{
    ptl_internal_handle_converter_t ni = { ni_handle };
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("bad NI\n");
        return PTL_ARG_INVALID;
    }
#endif

    /* BWB: FIX ME */

    return PTL_FAIL;
}

#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalLogicalProcessValidator(ptl_process_t p)
{
    /* BWB: FIX ME */
    return PTL_FAIL;
}


int INTERNAL PtlInternalPhysicalProcessValidator(ptl_process_t p)
{
    /* BWB: FIX ME */
    return PTL_FAIL;
}
#endif /* ifndef NO_ARG_VALIDATION */
