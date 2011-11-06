
#ifndef PPE_IF_NI_H
#define PPE_IF_NI_H

#include <stdio.h>

static inline int 
if_PtlNIInit( ptl_interface_t   iface,
                unsigned int      options,
                ptl_pid_t         pid,
                const ptl_ni_limits_t   *desired,
                ptl_ni_limits_t   *actual,
                ptl_handle_ni_t   *ni_handle)
{
    ptl_internal_handle_converter_t ni = { .s = { HANDLE_NI_CODE, 0, 0 } };

    __DBG("\n");
    if (iface == PTL_IFACE_DEFAULT) {
        iface = 0;
    }           
    ni.s.code = iface;
    switch (options) {
        case (PTL_NI_MATCHING | PTL_NI_LOGICAL):
            ni.s.ni = 0;
            break;
        case PTL_NI_NO_MATCHING | PTL_NI_LOGICAL:
            ni.s.ni = 1;
            break;
        case (PTL_NI_MATCHING | PTL_NI_PHYSICAL):
            ni.s.ni = 2;
            break;
        case PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL:
            ni.s.ni = 3;
            break;
#ifndef NO_ARG_VALIDATION
        default:
            return PTL_ARG_INVALID;
#endif
    }

    ppe_if_init( );

    ptl_cqe_t *entry; 
    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLNINIT;
    entry->u.niInit.options = options;
    entry->u.niInit.pid = pid;
    entry->u.niInit.ni_handle.s.ni = ni.s.ni;
    entry->u.niInit.ni_handle.s.selector = get_ppe_index();

    ptl_cq_entry_send(get_cq_handle(), get_cq_peer(), entry, sizeof(ptl_cqe_t));

    *ni_handle = ni.a;
    
    return PTL_OK;
}

static inline int 
if_PtlNIFini( ptl_handle_ni_t ni_handle)
{
    ptl_internal_handle_converter_t ni_hc = { ni_handle };

    ni_hc.s.selector = get_ppe_index(); 

    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( get_cq_handle(), &entry );
    entry->type = PTLNIFINI;
    entry->u.niFini.ni_handle = ni_hc;
    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, sizeof(ptl_cqe_t));
    return PTL_OK;
}

static inline int 
if_PtlNIHandle( ptl_handle_any_t    handle,
                ptl_handle_ni_t*    ni_handle)
{
    return PTL_FAIL;
}

static inline int 
if_PtlNIStatus( ptl_handle_ni_t    handle,
                ptl_sr_index_t     status_register,
                ptl_sr_value_t     *status )
{
    return PTL_FAIL;
}

#endif
