#include "config.h"

#include <assert.h>

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_CT.h"
#include "shared/ptl_internal_handles.h"


static ptl_ct_event_t *restrict ct_events[4] = { NULL, NULL, NULL, NULL };
static uint_fast64_t *restrict  ct_event_refcounts[4] = { NULL, NULL, NULL, NULL };

int PtlCTAlloc(ptl_handle_ni_t  ni_handle,
               ptl_handle_ct_t *ct_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t    ct_hc = { .s.ni = ni.s.ni };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if (ct_events[ni.s.ni] == NULL) {
        assert(ct_events[ni.s.ni] != NULL);
        return PTL_ARG_INVALID;
    }
    if (ct_handle == NULL) {
        VERBOSE_ERROR("passed in a NULL for ct_handle\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ct_hc.s.selector = get_my_id();
    ct_hc.s.code = find_ct_index( ni.s.ni ); 

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLCTALLOC;
    entry->u.ctAlloc.ct_handle  = ct_hc;
    entry->u.ctAlloc.addr       = NULL;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                        sizeof(ptl_cqe_t) );

    *ct_handle = ct_hc.a;

    return PTL_OK;
}


int PtlCTFree(ptl_handle_ct_t ct_handle)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }       
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif  

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLCTFREE;
    entry->u.ctFree.ct_handle = ( ptl_internal_handle_converter_t ) ct_handle;
    entry->u.ctFree.ct_handle.s.selector = get_my_id();

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                        sizeof(ptl_cqe_t) );

    return PTL_OK;
}

int PtlCTCancelTriggered(ptl_handle_ct_t ct_handle)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }                                  
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif

    return PTL_FAIL;
}

int PtlCTGet(ptl_handle_ct_t ct_handle,
             ptl_ct_event_t *event)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_OK;
}

int PtlCTWait(ptl_handle_ct_t ct_handle,
              ptl_size_t      test,
              ptl_ct_event_t *event)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        return PTL_ARG_INVALID;
    }
#endif
    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLCTALLOC;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                        sizeof(ptl_cqe_t) );

    return PTL_OK;
}

int PtlCTPoll(const ptl_handle_ct_t *ct_handles,
              const ptl_size_t      *tests,
              unsigned int           size,
              ptl_time_t             timeout,
              ptl_ct_event_t        *event,
              unsigned int          *which)
{
    ptl_size_t ctidx;
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ct_handles == NULL) || (tests == NULL) || (size == 0)) {
        return PTL_ARG_INVALID;
    }
    for (ctidx = 0; ctidx < size; ++ctidx) {
        if (PtlInternalCTHandleValidator(ct_handles[ctidx], 0)) {
            return PTL_ARG_INVALID;
        }
    }
    if (size > UINT32_MAX) {
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        return PTL_ARG_INVALID;
    }
    if (which == NULL) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    return PTL_OK;
}

int PtlCTSet(ptl_handle_ct_t ct_handle,
                      ptl_ct_event_t  test)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLCTSET;
    entry->u.ctSet.ct_handle = ( ptl_internal_handle_converter_t ) ct_handle;
    entry->u.ctSet.ct_handle.s.selector = get_my_id();
    entry->u.ctSet.test      = test;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                                    sizeof(ptl_cqe_t) );

    return PTL_OK;
}


int PtlCTInc(ptl_handle_ct_t ct_handle,
             ptl_ct_event_t  increment)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
        return PTL_ARG_INVALID;
    }
#endif

    ptl_cqe_t *entry;

    ptl_cq_entry_alloc( get_cq_handle(), &entry );

    entry->type = PTLCTSET;
    entry->u.ctInc.ct_handle = ( ptl_internal_handle_converter_t ) ct_handle;
    entry->u.ctInc.ct_handle.s.selector = get_my_id();
    entry->u.ctInc.increment = increment;

    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry,
                                                    sizeof(ptl_cqe_t) );


    return PTL_OK;
}

int INTERNAL PtlInternalCTHandleValidator(ptl_handle_ct_t handle,
                                          uint_fast8_t    none_ok)
{                                      /*{{{ */
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ct = { handle };
    if (ct.s.selector != HANDLE_CT_CODE) {
        VERBOSE_ERROR("Expected CT handle, but it's not a CT handle\n");
        return PTL_ARG_INVALID;
    }
    if (handle == PTL_CT_NONE) {
        if (none_ok == 1) {
            return PTL_OK;
        } else {
            VERBOSE_ERROR("PTL_CT_NONE not allowed here\n");
            return PTL_ARG_INVALID;
        }
    }
    if ((ct.s.ni > 3) || (ct.s.code > nit_limits[ct.s.ni].max_cts) ||
        (nit.refcount[ct.s.ni] == 0)) {
        VERBOSE_ERROR("CT NI too large (%u > 3) or code is wrong (%u > %u) or nit table is uninitialized\n",
                      ct.s.ni, ct.s.code, nit_limits[ct.s.ni].max_cts);
        return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni] == NULL) {
        VERBOSE_ERROR("CT table for NI uninitialized\n");
        return PTL_ARG_INVALID;
    }
    if (ct_event_refcounts[ct.s.ni][ct.s.code] == 0) {
        VERBOSE_ERROR("CT appears to be deallocated\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    return PTL_OK;
} 

/* vim:set expandtab: */

