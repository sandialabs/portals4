#include "config.h"

#include <assert.h>

#include "portals4.h"

#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_EQ.h"
#include "shared/ptl_internal_handles.h"

const ptl_internal_handle_converter_t eq_none = {
    .s = {
        .selector = HANDLE_EQ_CODE,
        .ni       = ((1 << HANDLE_NI_BITS) - 1),
        .code     = ((1 << HANDLE_CODE_BITS) - 1)
    }
};

typedef struct {
#if 0
    ptl_internal_event_t *ring;
    uint32_t              size;
    eq_off_t              leading_head, lagging_head, leading_tail, lagging_tail;
#endif
} ptl_internal_eq_t ALIGNED (CACHELINE_WIDTH);


static ptl_internal_eq_t *eqs[4] = { NULL, NULL, NULL, NULL };
static uint64_t          *eq_refcounts[4] = { NULL, NULL, NULL, NULL };

int PtlEQAlloc(ptl_handle_ni_t  ni_handle,
               ptl_size_t       count,
               ptl_handle_eq_t *eq_handle)
{
    const ptl_internal_handle_converter_t ni_hc  = { ni_handle };
    ptl_internal_handle_converter_t eq_hc  = { .s.ni = ni_hc.s.ni };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni_hc)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if (nit.tables[ni_hc.s.ni] == NULL) { // this should never happen
        assert(nit.tables[ni_hc.s.ni] != NULL);
        return PTL_ARG_INVALID;
    }
    if (eq_handle == NULL) {
        VERBOSE_ERROR("passed in a NULL for eq_handle");
        return PTL_ARG_INVALID;
    }
    if (count > 0xffff) {
        VERBOSE_ERROR("insanely large count");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    eq_hc.s.selector = get_my_ppe_rank();
    eq_hc.s.code = find_eq_index( ni_hc.s.ni);

    ptl_cqe_t *entry;
        
    ptl_cq_entry_alloc( get_cq_handle(), &entry );
    
    entry->type = PTLEQALLOC;
    entry->u.eqAlloc.eq_handle = ni_hc;
    entry->u.eqAlloc.count = count;
    entry->u.eqAlloc.addr = NULL;
    
    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, 
                        sizeof(ptl_cqe_t) );

    *eq_handle = eq_hc.a;
    return PTL_OK;
}

int PtlEQFree(ptl_handle_eq_t eq_handle)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalEQHandleValidator(eq_handle, 0)) {
        VERBOSE_ERROR("invalid EQ handle\n");
        return PTL_ARG_INVALID;
    }
#endif     

    ptl_cqe_t *entry;
        
    ptl_cq_entry_alloc( get_cq_handle(), &entry );
    
    entry->type = PTLEQFREE;
    entry->u.eqFree.eq_handle = ( ptl_internal_handle_converter_t ) eq_handle;
    entry->u.eqFree.eq_handle.s.selector = get_my_ppe_rank();
    
    ptl_cq_entry_send( get_cq_handle(), get_cq_peer(), entry, 
                        sizeof(ptl_cqe_t) );

    return PTL_OK;
}


int PtlEQGet(ptl_handle_eq_t eq_handle,
             ptl_event_t    *event)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalEQHandleValidator(eq_handle, 0)) {
        VERBOSE_ERROR("invalid EQ handle\n");
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        VERBOSE_ERROR("null event\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    return PTL_OK;
}

int PtlEQWait(ptl_handle_eq_t eq_handle,
              ptl_event_t    *event)
{
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalEQHandleValidator(eq_handle, 0)) {
        VERBOSE_ERROR("invalid EQ handle\n");
        return PTL_ARG_INVALID;
    }
    if (event == NULL) {
        VERBOSE_ERROR("null event\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    return PTL_OK;
}

int PtlEQPoll(const ptl_handle_eq_t *eq_handles,
              unsigned int           size,
              ptl_time_t             timeout,
              ptl_event_t           *event,
              unsigned int          *which)
{
    ptl_size_t eqidx; 
#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((event == NULL) && (which == NULL)) {  
        VERBOSE_ERROR("null event or null which\n");
        return PTL_ARG_INVALID;           
    }
    for (eqidx = 0; eqidx < size; ++eqidx) {
        if (PtlInternalEQHandleValidator(eq_handles[eqidx], 0)) {
            VERBOSE_ERROR("invalid EQ handle\n");
            return PTL_ARG_INVALID;
        }
    }   
#endif /* ifndef NO_ARG_VALIDATION */

    return PTL_OK;
}

#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalEQHandleValidator(ptl_handle_eq_t handle,
                                          int             none_ok)
{
    const ptl_internal_handle_converter_t eq = { handle };

    if (eq.s.selector != HANDLE_EQ_CODE) {
        VERBOSE_ERROR("Expected EQ handle, but it's not one (%u != %u, 0x%lx, 0x%lx)\n",
                      eq.s.selector, HANDLE_EQ_CODE, handle, eq_none.i);
        return PTL_ARG_INVALID;
    }
    if ((none_ok == 1) && (handle == PTL_EQ_NONE)) {
        return PTL_OK;
    }
    if ((eq.s.ni > 3) || (eq.s.code > nit_limits[eq.s.ni].max_eqs) ||
        (nit.refcount[eq.s.ni] == 0)) {
        VERBOSE_ERROR("EQ NI too large (%u > 3) or code is wrong (%u > %u) or nit table is uninitialized\n",
                      eq.s.ni, eq.s.code, nit_limits[eq.s.ni].max_cts);
        return PTL_ARG_INVALID;
    }
    if (eqs[eq.s.ni] == NULL) {
        VERBOSE_ERROR("EQ table for NI uninitialized\n");
        return PTL_ARG_INVALID;
    }
    __sync_synchronize();
    if (eq_refcounts[eq.s.ni][eq.s.code] == 0) {
        VERBOSE_ERROR("EQ(%i,%i) appears to be deallocated\n", (int)eq.s.ni,
                      (int)eq.s.code);
        return PTL_ARG_INVALID;
    }
    return PTL_OK;
}

#endif /* ifndef NO_ARG_VALIDATION */
