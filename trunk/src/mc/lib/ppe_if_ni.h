
#ifndef PPE_IF_NI_H
#define PPE_IF_NI_H

#include <assert.h>

#include "command_queue.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_error.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_locks.h"

static inline int 
if_PtlNIInit( ptl_interface_t   iface,
                unsigned int      options,
                ptl_pid_t         pid,
                const ptl_ni_limits_t   *desired,
                ptl_ni_limits_t   *actual,
                ptl_handle_ni_t   *ni_handle)
{
    ptl_internal_handle_converter_t ni = { .s = { HANDLE_NI_CODE, 0, 0 } };

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

printf("%s():%d\n",__func__,__LINE__);
    *ni_handle = ni.a;
    if ((desired != NULL) &&
        (PtlInternalAtomicCas32(&nit_limits_init[ni.s.ni], 0, 1) == 0)) {
        /* nit_limits_init[ni.s.ni] now marked as "being initialized" */
        if ((desired->max_entries > 0) &&
            (desired->max_entries < (1 << HANDLE_CODE_BITS))) {
            nit_limits[ni.s.ni].max_entries = desired->max_entries;
        }
        if (desired->max_unexpected_headers > 0) {
            nit_limits[ni.s.ni].max_unexpected_headers = desired->max_unexpected_headers;
        }
        if ((desired->max_mds > 0) &&
            (desired->max_mds < (1 << HANDLE_CODE_BITS))) {
            nit_limits[ni.s.ni].max_mds = desired->max_mds;
        }
        if ((desired->max_cts > 0) &&
            (desired->max_cts < (1 << HANDLE_CODE_BITS))) {
            nit_limits[ni.s.ni].max_cts = desired->max_cts;
        }
        if ((desired->max_eqs > 0) &&
            (desired->max_eqs < (1 << HANDLE_CODE_BITS))) {
            nit_limits[ni.s.ni].max_eqs = desired->max_eqs;
        }
        if (desired->max_pt_index >= 63) {      // XXX: there may need to be more restrictions on this
            nit_limits[ni.s.ni].max_pt_index = desired->max_pt_index;
        }
        // nit_limits[ni.s.ni].max_iovecs = INT_MAX;      // ???
        if ((desired->max_list_size > 0) &&
            (desired->max_list_size < (1ULL << (sizeof(uint32_t) * 8)))) {
            nit_limits[ni.s.ni].max_list_size = desired->max_list_size;
        }
        if ((desired->max_triggered_ops >= 0) &&
            (desired->max_triggered_ops < (1ULL << (sizeof(uint32_t) * 8)))) {
            nit_limits[ni.s.ni].max_triggered_ops = desired->max_triggered_ops;
        }
        if ((desired->max_msg_size > 0) &&
            (desired->max_msg_size < UINT32_MAX)) {
            nit_limits[ni.s.ni].max_msg_size = desired->max_msg_size;
        }
        if ((desired->max_atomic_size >= 8) &&
#ifdef USE_TRANSFER_ENGINE
            (desired->max_atomic_size <= UINT32_MAX)
#else
            (desired->max_atomic_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))
#endif
            ) {
            nit_limits[ni.s.ni].max_atomic_size = desired->max_atomic_size;
        }
        if ((desired->max_fetch_atomic_size >= 8) &&
#ifdef USE_TRANSFER_ENGINE
            (desired->max_fetch_atomic_size <= UINT32_MAX)
#else
            (desired->max_fetch_atomic_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))
#endif
            ) {
            nit_limits[ni.s.ni].max_fetch_atomic_size = desired->max_fetch_atomic_size;
        }
        if ((desired->max_waw_ordered_size >= 8) &&
            (desired->max_waw_ordered_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))) {
            nit_limits[ni.s.ni].max_waw_ordered_size = desired->max_waw_ordered_size;
        }
        if ((desired->max_war_ordered_size >= 8) &&
            (desired->max_war_ordered_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))) {
            nit_limits[ni.s.ni].max_war_ordered_size = desired->max_war_ordered_size;
        }
        if ((desired->max_volatile_size >= 8) &&
            (desired->max_volatile_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))) {
            nit_limits[ni.s.ni].max_volatile_size = desired->max_volatile_size;
        }
        nit_limits[ni.s.ni].features = PTL_TARGET_BIND_INACCESSIBLE | PTL_TOTAL_DATA_ORDERING;
        nit_limits_init[ni.s.ni]     = 2;       // mark it as done being initialized
    }
    while (nit_limits_init[ni.s.ni] == 1) SPINLOCK_BODY();     /* if being initialized by another thread, wait for it to be initialized */
    if (actual != NULL) {
        *actual = nit_limits[ni.s.ni];
    }

    ptl_cq_handle_t cq_handle;
    int retval = ptl_cq_create( 100, 100, &cq_handle );
    assert( retval == 0 );
    
    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( cq_handle, &entry );
    ptl_cq_entry_send( cq_handle, entry );
    return PTL_OK;
}

static inline int 
if_PtlNIFini( ptl_handle_ni_t ni_handle)
{
    ptl_cq_t* cq = NULL;
    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( cq, &entry );
    ptl_cq_entry_send( cq, entry );
    return PTL_OK;
}

static inline int 
if_PtlNIHandle( ptl_handle_any_t    handle,
                ptl_handle_ni_t*    ni_handle)
{
    ptl_cq_t* cq = NULL;
    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( cq, &entry );
    ptl_cq_entry_send( cq, entry );
    return PTL_OK;
}

static inline int 
if_PtlNIStatus( ptl_handle_ni_t    handle,
                ptl_sr_index_t     status_register,
                ptl_sr_value_t     *status )
{
    ptl_cq_t* cq = NULL;
    ptl_cqe_t *entry;
    ptl_cq_entry_alloc( cq, &entry );
    ptl_cq_entry_send( cq, entry );
    return PTL_OK;
}

#endif
