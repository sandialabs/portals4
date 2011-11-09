#include "config.h"

#include <strings.h>

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_iface.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_startup.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"


ptl_ni_limits_t nit_limits[4];


int PtlNIInit(ptl_interface_t       iface,
              unsigned int          options,
              ptl_pid_t             pid,
              const ptl_ni_limits_t *desired,
              ptl_ni_limits_t       *actual,
              ptl_handle_ni_t       *ni_handle)
{
    int ret;
    ptl_internal_handle_converter_t ni = { .s = { HANDLE_NI_CODE, 0, 0 } };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (pid != PTL_PID_ANY && pid > PTL_PID_MAX) {
        VERBOSE_ERROR("Pid too large (%li > %li)\n", 
                      (long)pid, (long)PTL_PID_MAX);
        return PTL_ARG_INVALID;
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

    if (iface == PTL_IFACE_DEFAULT) {
        iface = 0;
    }
    ni.s.code = 0;
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

    /* Initialize connection, if needed */
    if (0 == __sync_fetch_and_add(&ptl_iface.connection_count, 1)) {
        ret = ptl_ppe_global_init(&ptl_iface);
        if (ret < 0) return PTL_FAIL;
        ret = ptl_ppe_connect(&ptl_iface);
        if (ret < 0) return PTL_FAIL;
        ret = ptl_ppe_global_setup(&ptl_iface);
        if (ret < 0) return PTL_FAIL;
    }

    if (0 == __sync_fetch_and_add(&ptl_iface.ni[ni.s.ni].refcount, 1)) {
        ptl_cqe_t recv_entry;
        ptl_cqe_t *entry;

        ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);

        entry->type = PTLNINIT;
        entry->u.niInit.ni_handle = ni;
        entry->u.niInit.ni_handle.s.code = ptl_iface_get_rank(&ptl_iface);
        entry->u.niInit.options = options;
        entry->u.niInit.pid = pid;

        ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), ptl_iface_get_peer(&ptl_iface),
                          entry, sizeof(ptl_cqe_t));


        ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);

        entry->type = PTLNIINIT_LIMITS;
        entry->u.niInitLimits.ni_handle = ni;
        entry->u.niInit.ni_handle.s.code = ptl_iface_get_rank(&ptl_iface);
        entry->u.niInitLimits.ni_limits = *desired;

        ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), ptl_iface_get_peer(&ptl_iface),
                          entry, sizeof(ptl_cqe_t));

        ret = 1;
        while (1 == ret) {
            ret = ptl_cq_entry_recv(ptl_iface_get_cq(&ptl_iface), &recv_entry);
            if (ret < 0)  return PTL_FAIL;
        }
        if (recv_entry.type != PTLNIINIT_LIMITS) {
            return PTL_FAIL;
        }
        nit_limits[ni.s.ni] = recv_entry.u.niInitLimits.ni_limits;
        __sync_synchronize();
        ptl_iface.ni[ni.s.ni].limits_refcount = 1;

#if 0
        ptl_iface.ni[ni.s.ni].shared_mem = malloc( 
            sizeof( ptl_internal_le_t) * returned_limits.max_list_size + 
            sizeof( ptl_internal_md_t) * returned_limits.max_mds +
            sizeof( ptl_internal_me_t) * returned_limits.max_list_size +
            sizeof( ptl_internal_ct_t) * returned_limits.max_cts +
            sizeof( ptl_internal_eq_t) * returned_limits.max_eqs +
            sizeof( ptl_internal_pt_t) * returned_limits.max_pt_index
        );
        ptl_iface.ni[ni.s.ni].i_le = ptl_iface.ni[ni.s.ni].shared_mem;
        ptl_iface.ni[ni.s.ni].i_md = (void*) (ptl_iface.ni[ni.s.ni].i_le + 
                                            returned_limits.max_list_size);
        ptl_iface.ni[ni.s.ni].i_me = (void*) (ptl_iface.ni[ni.s.ni].i_md + 
                                            returned_limits.max_mds );
        ptl_iface.ni[ni.s.ni].i_ct = (void*) (ptl_iface.ni[ni.s.ni].i_me + 
                                            returned_limits.max_list_size );
        ptl_iface.ni[ni.s.ni].i_eq = (void*) (ptl_iface.ni[ni.s.ni].i_ct + 
                                            returned_limits.max_cts );
        ptl_iface.ni[ni.s.ni].i_pt = (void*) (ptl_iface.ni[ni.s.ni].i_eq + 
                                            returned_limits.max_eqs );
#endif

    } else {
        if ((pid != PTL_PID_ANY) && pid != ptl_iface.ni[ni.s.ni].pid) {
            return PTL_ARG_INVALID;
        }
    }

    /* wait for ni limits to be ready... */
    while (ptl_iface.ni[ni.s.ni].limits_refcount == 0) { __sync_synchronize(); }

    *actual = nit_limits[ni.s.ni];
    *ni_handle = ni.a;

    return PTL_OK;
}


int
PtlNIFini(ptl_handle_ni_t ni_handle)
{
    ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || 
        (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        VERBOSE_ERROR("Bad NI (%lu)\n", (unsigned long)ni_handle);
        return PTL_ARG_INVALID;
    }
#endif 

    if (0 == __sync_fetch_and_sub(&ptl_iface.ni[ni.s.ni].refcount, 1)) {
        ptl_cqe_t *entry;

        ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);

        entry->type = PTLNIFINI;
        entry->u.niFini.ni_handle = ni;
        ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), ptl_iface_get_peer(&ptl_iface), 
                          entry, sizeof(ptl_cqe_t));
    }

    return PTL_OK;
}


int
PtlNIHandle(ptl_handle_any_t    handle,
            ptl_handle_ni_t*    ni_handle)
{
    ptl_internal_handle_converter_t ni = { handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
#endif

    ni.s.selector = HANDLE_NI_CODE;
    /* NI stays the same */
    ni.s.code = 0; /* code is unused with nis */
    *ni_handle = ni.i;

    return PTL_OK;
}


int
PtlNIStatus(ptl_handle_ni_t    ni_handle,
            ptl_sr_index_t     status_register,
            ptl_sr_value_t     *status )
{
    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        return PTL_ARG_INVALID;
    }
    if (status == NULL) {
        return PTL_ARG_INVALID;
    }
    if (status_register >= PTL_SR_LAST) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    *status = ptl_iface.ni[ni.s.ni].status_registers[status_register];

    return PTL_OK;
}


int INTERNAL
PtlInternalNIValidator(const ptl_internal_handle_converter_t ni)
{
#ifndef NO_ARG_VALIDATION
    if (ni.s.selector != HANDLE_NI_CODE) {
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni > 3) || (ni.s.code != 0) || (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_OK;
}


