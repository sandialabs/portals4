#include "config.h"

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_startup.h"
#include "shared/ptl_internal_handles.h"

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

    /* Initialize connection, if needed */
    if (0 == __sync_fetch_and_add(&ptl_global.init_count, 1)) {
        ret = ptl_ppe_global_init();
        if (ret < 0) return PTL_FAIL;
        ret = ptl_ppe_connect();
        if (ret < 0) return PTL_FAIL;
        ret = ptl_ppe_global_setup();
        if (ret < 0) return PTL_FAIL;
    }

    if (0 == __sync_fetch_and_add(&nit.refcount[ni.s.ni], 1)) {
        ptl_cqe_t *entry;
        ptl_cq_entry_alloc( get_cq_handle(), &entry );

        entry->type = PTLNINIT;
        entry->u.niInit.options = options;
        entry->u.niInit.pid = pid;
        entry->u.niInit.ni_handle.s.ni = ni.s.ni;
        entry->u.niInit.ni_handle.s.selector = get_my_ppe_rank();

        ptl_cq_entry_send(get_cq_handle(), get_cq_peer(), 
                          entry, sizeof(ptl_cqe_t));
        fprintf(stderr, "%s:%d BWB: FIX ME: Need to wait for ni info\n",
                __FILE__, __LINE__);
    } 

    /* BWB: FIX ME: Need to fill in NI information in either case */

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
        (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("Bad NI (%lu)\n", (unsigned long)ni_handle);
        return PTL_ARG_INVALID;
    }
#endif 

    if (0 == __sync_fetch_and_sub(&nit.refcount[ni.s.ni], 1)) {
        ni.s.selector = get_my_ppe_rank();

        ptl_cqe_t *entry;
        ptl_cq_entry_alloc( get_cq_handle(), &entry );
        entry->type = PTLNIFINI;
        entry->u.niFini.ni_handle = ni;
        ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, sizeof(ptl_cqe_t));
    }

    return PTL_OK;

}


int
PtlNIHandle(ptl_handle_any_t    handle,
            ptl_handle_ni_t*    ni_handle)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
#endif

    return PTL_FAIL;
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

    return PTL_FAIL;
}


int INTERNAL
PtlInternalNIValidator(const ptl_internal_handle_converter_t ni)
{
#ifndef NO_ARG_VALIDATION
    if (ni.s.selector != HANDLE_NI_CODE) {
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni > 3) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_FAIL;
}


