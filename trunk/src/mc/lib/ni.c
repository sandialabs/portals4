#include "config.h"

#include "portals4.h"
#include "ppe_if.h"

#include "shared/ptl_internal_handles.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"

ptl_internal_nit_t nit = { { 0, 0, 0, 0 },
                           { 0, 0, 0, 0 },
                              };

ptl_ni_limits_t    nit_limits[4];

uint32_t nit_limits_init[4] = { 0, 0, 0, 0 };

int PtlNIInit(ptl_interface_t       iface,
              unsigned int          options,
              ptl_pid_t             pid,
              const ptl_ni_limits_t *desired,
              ptl_ni_limits_t       *actual,
              ptl_handle_ni_t       *ni_handle)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (pid != PTL_PID_ANY) {
#if 0
        if ((proc_number != -1) && (pid != proc_number)) {
            VERBOSE_ERROR("Invalid pid (%i), rank may already be set (%i)\n", (int)pid, (int)proc_number);
            return PTL_ARG_INVALID;
        }
#endif
        if (pid > PTL_PID_MAX) {
            VERBOSE_ERROR("Pid too large (%li > %li)\n", (long)pid, (long)PTL_PID_MAX);
            return PTL_ARG_INVALID;
        }
    }
    if ((iface != 0) && (iface != PTL_IFACE_DEFAULT)) {
        VERBOSE_ERROR("Invalid Interface (%i)\n", (int)iface);
        return PTL_ARG_INVALID;
    }
    if (options & ~(PTL_NI_INIT_OPTIONS_MASK)) {
        VERBOSE_ERROR("Invalid options value (0x%x)\n", options);
        return PTL_ARG_INVALID;
    }
    if (options & PTL_NI_MATCHING && options & PTL_NI_NO_MATCHING) {
        VERBOSE_ERROR("Neither matching nor non-matching\n");
        return PTL_ARG_INVALID;
    }
    if (options & PTL_NI_LOGICAL && options & PTL_NI_PHYSICAL) {
        VERBOSE_ERROR("Neither logical nor physical\n");
        return PTL_ARG_INVALID;
    }
    if (ni_handle == NULL) {
        VERBOSE_ERROR("ni_handle == NULL\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    return FUNC_CALL( PtlNIInit, iface, options, pid, 
                                    desired, actual, ni_handle );
}

int PtlNIFini(ptl_handle_ni_t ni_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("Bad NI (%lu)\n", (unsigned long)ni_handle);
        return PTL_ARG_INVALID;
    }
   // assert(my_shmid != -1);
#endif 

    return FUNC_CALL(PtlNIFini, ni_handle );
}

int PtlNIHandle(ptl_handle_any_t    handle,
                ptl_handle_ni_t*    ni_handle)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
#endif
    return FUNC_CALL( PtlNIHandle, handle, ni_handle ); 
}

int PtlNIStatus(ptl_handle_ni_t    ni_handle,
                ptl_sr_index_t     status_register,
                ptl_sr_value_t     *status )
{
    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        return PTL_ARG_INVALID;
    }
    if (status == NULL) {
        return PTL_ARG_INVALID;
    }
    if (status_register >= PTL_SR_LAST) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    return FUNC_CALL( PtlNIStatus, ni_handle, status_register, status );
}

int INTERNAL PtlInternalNIValidator(const ptl_internal_handle_converter_t ni)
{
#ifndef NO_ARG_VALIDATION
    if (ni.s.selector != HANDLE_NI_CODE) {
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni > 3) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_OK;
}


