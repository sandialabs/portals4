#include "portals4.h"
#include "ppe_if.h"

int PtlNIInit(ptl_interface_t   iface,
              unsigned int      options,
              ptl_pid_t         pid,
              const ptl_ni_limits_t   *desired,
              ptl_ni_limits_t   *actual,
              ptl_handle_ni_t   *ni_handle)
{
    return FUNC_CALL( PtlNIInit, iface, options, pid, 
                                    desired, actual, ni_handle );
}

int PtlNIFini(ptl_handle_ni_t ni_handle)
{
    return FUNC_CALL( PtlNIFini, ni_handle );
}

int PtlNIHandle(ptl_handle_any_t    handle,
                ptl_handle_ni_t*    ni_handle)
{
    return FUNC_CALL( PtlNIHandle, handle, ni_handle ); 
}

int PtlNIStatus(ptl_handle_ni_t    handle,
                ptl_sr_index_t     status_register,
                ptl_sr_value_t     *status )
{
    return FUNC_CALL( PtlNIStatus, handle, status_register, status );
}
