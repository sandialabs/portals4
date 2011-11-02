
#ifndef PPE_IF_NI_H
#define PPE_IF_NI_H

#include "command_queue.h"

static inline int 
if_PtlNIInit( ptl_cq_t* cq, 
                ptl_interface_t   iface,
                unsigned int      options,
                ptl_pid_t         pid,
                const ptl_ni_limits_t   *desired,
                ptl_ni_limits_t   *actual,
                ptl_handle_ni_t   *ni_handle)
{
    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( cq, &entry );
    ptl_cq_entry_send( cq, entry );
    return PTL_OK;
}

static inline int 
if_PtlNIFini( ptl_cq_t* cq,
                ptl_handle_ni_t ni_handle)
{
    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( cq, &entry );
    ptl_cq_entry_send( cq, entry );
    return PTL_OK;
}

static inline int 
if_PtlNIHandle( ptl_cq_t* cq, 
                ptl_handle_any_t    handle,
                ptl_handle_ni_t*    ni_handle)
{
    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( cq, &entry );
    ptl_cq_entry_send( cq, entry );
    return PTL_OK;
}

static inline int 
if_PtlNIStatus( ptl_cq_t* cq,
                ptl_handle_ni_t    handle,
                ptl_sr_index_t     status_register,
                ptl_sr_value_t     *status )
{
    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( cq, &entry );
    ptl_cq_entry_send( cq, entry );
    return PTL_OK;
}

#endif
