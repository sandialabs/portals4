#include "config.h"

#include <strings.h>
#include <limits.h>

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_iface.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_startup.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"


ptl_ni_limits_t nit_limits[4];

ptl_ni_limits_t default_limits = {
    1024, /* max_entries */
    1024, /* max_unexpected_headers */
    1024, /* max_mds */
    256,  /* max_cts */
    256,  /* max_eqs */
    63,   /* max_pt_index */
    1,    /* max_iovecs */
    512,  /* max_list_size */
    64,   /* max_triggered_ops */
    LONG_MAX, /* max_msg_size */
    sizeof(long double), /* max_atomic_size */
    sizeof(long double), /* max_fetch_atomic_size */
    sizeof(long double), /* max_waw_ordered_size */
    sizeof(long double), /* max_war_ordered_size */
    8,   /* max_volatile_size */
    0    /* features */
};

int PtlNIInit(ptl_interface_t       iface,
              unsigned int          options,
              ptl_pid_t             pid,
              const ptl_ni_limits_t *desired,
              ptl_ni_limits_t       *actual,
              ptl_handle_ni_t       *ni_handle)
{
    int ret, cmd_ret = PTL_STATUS_LAST;
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

    /* Initialize network interface, if needed.  Other NIInit calls on
       the same network interface will block until limits_refcount is
       non-zero */
    if (0 == __sync_fetch_and_add(&ptl_iface.ni[ni.s.ni].refcount, 1)) {
        ptl_cqe_t *entry;

        /* Initialize connection, if needed.  Other NIInit calls will
           block until ptl_iface.connection_established is non-zero. */
        if (0 == __sync_fetch_and_add(&ptl_iface.connection_count, 1)) {
            ret = ptl_ppe_connect(&ptl_iface);
            if (ret < 0) return PTL_FAIL;

            ptl_iface.segid = xpmem_make (0, 0xffffffffffffffffll, 
                                          XPMEM_PERMIT_MODE,
                                          (void *)0666);
            if (-1 == ptl_iface.segid) return PTL_FAIL;

            __sync_synchronize();
            ptl_iface.connection_established = 1;
        }
        while (0 == ptl_iface.connection_established) __sync_synchronize();

        if (NULL == desired) {
            nit_limits[ni.s.ni] = default_limits;
        } else {
            nit_limits[ni.s.ni] = *desired;
        }

        ptl_iface.ni[ni.s.ni].shared_mem = malloc( 
            sizeof( ptl_internal_le_t) * nit_limits[ni.s.ni].max_list_size + 
            sizeof( ptl_internal_md_t) * nit_limits[ni.s.ni].max_mds +
            sizeof( ptl_internal_me_t) * nit_limits[ni.s.ni].max_list_size +
            sizeof( ptl_internal_ct_t) * nit_limits[ni.s.ni].max_cts +
            sizeof( ptl_internal_eq_t) * nit_limits[ni.s.ni].max_eqs +
            sizeof( ptl_internal_pt_t) * nit_limits[ni.s.ni].max_pt_index
        );
        ptl_iface.ni[ni.s.ni].i_le = ptl_iface.ni[ni.s.ni].shared_mem;
        ptl_iface.ni[ni.s.ni].i_md = (void*) (ptl_iface.ni[ni.s.ni].i_le + 
                                            nit_limits[ni.s.ni].max_list_size);
        ptl_iface.ni[ni.s.ni].i_me = (void*) (ptl_iface.ni[ni.s.ni].i_md + 
                                            nit_limits[ni.s.ni].max_mds );
        ptl_iface.ni[ni.s.ni].i_ct = (void*) (ptl_iface.ni[ni.s.ni].i_me + 
                                            nit_limits[ni.s.ni].max_list_size );
        ptl_iface.ni[ni.s.ni].i_eq = (void*) (ptl_iface.ni[ni.s.ni].i_ct + 
                                            nit_limits[ni.s.ni].max_cts );
        ptl_iface.ni[ni.s.ni].i_pt = (void*) (ptl_iface.ni[ni.s.ni].i_eq + 
                                            nit_limits[ni.s.ni].max_eqs );

        ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);

        entry->type = PTLNIINIT;
        entry->u.niInit.ni_handle = ni;
        entry->u.niInit.ni_handle.s.code = ptl_iface_get_rank(&ptl_iface);
        entry->u.niInit.options = options;
        entry->u.niInit.pid = pid;
        entry->u.niInit.limits = &nit_limits[ni.s.ni];
        entry->u.niInit.lePtr = ptl_iface.ni[ni.s.ni].i_le;
        entry->u.niInit.mdPtr = ptl_iface.ni[ni.s.ni].i_md;
        entry->u.niInit.mePtr = ptl_iface.ni[ni.s.ni].i_me;
        entry->u.niInit.ctPtr = ptl_iface.ni[ni.s.ni].i_ct;
        entry->u.niInit.eqPtr = ptl_iface.ni[ni.s.ni].i_eq;
        entry->u.niInit.ptPtr = ptl_iface.ni[ni.s.ni].i_pt;
        entry->u.niInit.retval_ptr = &cmd_ret;

        ret = ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), 
                                ptl_iface_get_peer(&ptl_iface),
                                entry, sizeof(ptl_cqe_t));
        if (0 != ret) return PTL_FAIL;

        /* wait for result */
        do {
            ret = ptl_ppe_progress(&ptl_iface, 1);
            if (ret < 0) return PTL_FAIL;
            __sync_synchronize();
        } while (PTL_STATUS_LAST == cmd_ret);
        if (PTL_OK != cmd_ret) return cmd_ret;

        __sync_synchronize();
        ptl_iface.ni[ni.s.ni].limits_refcount = 1;
    } else {
        if ((pid != PTL_PID_ANY) && pid != ptl_iface.ni[ni.s.ni].pid) {
            return PTL_ARG_INVALID;
        }
    }

    /* wait for ni limits to be ready... */
    while (ptl_iface.ni[ni.s.ni].limits_refcount == 0) { 
        __sync_synchronize(); 
    }

    if (NULL != actual) {
        *actual = nit_limits[ni.s.ni];
    }
    *ni_handle = ni.a;

    return PTL_OK;
}


int
PtlNIFini(ptl_handle_ni_t ni_handle)
{
    ptl_internal_handle_converter_t ni = { ni_handle };
    int ret, cmd_ret = PTL_STATUS_LAST;

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
        entry->u.niFini.ni_handle.s.code = ptl_iface_get_rank(&ptl_iface);
        entry->u.niFini.retval_ptr = &cmd_ret;
        ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), 
                          ptl_iface_get_peer(&ptl_iface), 
                          entry, sizeof(ptl_cqe_t));

        /* wait for result */
        do {
            ret = ptl_ppe_progress(&ptl_iface, 1);
            if (ret < 0) return PTL_FAIL;
            __sync_synchronize();
        } while (PTL_STATUS_LAST == cmd_ret);
        if (PTL_OK != cmd_ret) return cmd_ret;

        if (0 == __sync_fetch_and_sub(&ptl_iface.connection_count, 1)) {
            ret = ptl_ppe_disconnect(&ptl_iface);
            if (ret < 0) return PTL_FAIL;
        }
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


