
#include "../include/p3-config.h"

#include "../include/p3lib/p3lib_support.h"
#include "../include/p3/nal_types.h"
#include "../nal/tcp/lib-tcpnal.h"

lib_ni_t *p3lib_get_ni_pid(ptl_interface_t type, ptl_pid_t pid)
{
    return 0;
}

void p3lib_nal_setup(void)
{
#ifdef PTL_UTCP_NAL_SUPPORT
    lib_register_nal(PTL_NALTYPE_UTCP, "UTCP",
             p3tcp_create_nal, p3tcp_stop_nal, p3tcp_destroy_nal,
             p3tcp_pid_ranges);
    lib_register_nal(PTL_NALTYPE_UTCP1, "UTCP1",
             p3tcp_create_nal, p3tcp_stop_nal, p3tcp_destroy_nal,
             p3tcp_pid_ranges);
    lib_register_nal(PTL_NALTYPE_UTCP2, "UTCP2",
             p3tcp_create_nal, p3tcp_stop_nal, p3tcp_destroy_nal,
             p3tcp_pid_ranges);
    lib_register_nal(PTL_NALTYPE_UTCP3, "UTCP3",
             p3tcp_create_nal, p3tcp_stop_nal, p3tcp_destroy_nal,
             p3tcp_pid_ranges);
#endif
}

void p3lib_nal_teardown(void)
{
#ifdef PTL_UTCP_NAL_SUPPORT
    lib_unregister_nal(PTL_NALTYPE_UTCP);
    lib_unregister_nal(PTL_NALTYPE_UTCP1);
    lib_unregister_nal(PTL_NALTYPE_UTCP2);
    lib_unregister_nal(PTL_NALTYPE_UTCP3);
#endif
}

