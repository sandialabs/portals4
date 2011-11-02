
#ifndef PPE_IF_NI_H
#define PPE_IF_NI_H

static inline int if_PtlNIInit(ptl_interface_t   iface,
              unsigned int      options,
              ptl_pid_t         pid,
              const ptl_ni_limits_t   *desired,
              ptl_ni_limits_t   *actual,
              ptl_handle_ni_t   *ni_handle)
{
    return PTL_FAIL;
}

static inline int if_PtlNIFini(ptl_handle_ni_t ni_handle)
{
    return PTL_FAIL;
}

static inline int if_PtlNIHandle(ptl_handle_any_t    handle,
                ptl_handle_ni_t*    ni_handle)
{
    return PTL_FAIL;
}

static inline int if_PtlNIStatus(ptl_handle_ni_t    handle,
                ptl_sr_index_t     status_register,
                ptl_sr_value_t     *status )
{
    return PTL_FAIL;
}

#endif
