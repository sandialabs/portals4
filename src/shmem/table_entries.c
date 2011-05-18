#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <portals4.h>

/* System headers */
#include <string.h>                    /* for memcpy() */

/* Internal headers */
#include "ptl_visibility.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_PT.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_error.h"

#define PT_FREE     0
#define PT_ENABLED  1
#define PT_DISABLED 2

/**************************************************************************************************
 * API Functions
 */

int API_FUNC PtlPTAlloc(ptl_handle_ni_t ni_handle,
                        unsigned int    options,
                        ptl_handle_eq_t eq_handle,
                        ptl_pt_index_t  pt_index_req,
                        ptl_pt_index_t *pt_index)
{                                      /*{{{ */
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_table_entry_t                    *pt = NULL;

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("Invalid NI passed to PtlPTAlloc\n");
        return PTL_ARG_INVALID;
    }
    if (options & ~PTL_PT_INIT_OPTIONS_MASK) {
        VERBOSE_ERROR("Invalid options to PtlPTAlloc (0x%x)\n", options);
        return PTL_ARG_INVALID;
    }
    if ((eq_handle == PTL_EQ_NONE) && options & PTL_PT_FLOWCTRL) {
        return PTL_PT_EQ_NEEDED;
    }
    if (PtlInternalEQHandleValidator(eq_handle, 1)) {
        VERBOSE_ERROR("Invalid EQ passed to PtlPTAlloc\n");
        return PTL_ARG_INVALID;
    }
    if ((pt_index_req > nit_limits[ni.s.ni].max_pt_index) && (pt_index_req != PTL_PT_ANY)) {
        VERBOSE_ERROR("Invalid pt_index request passed to PtlPTAlloc\n");
        return PTL_ARG_INVALID;
    }
    if (pt_index == NULL) {
        VERBOSE_ERROR("Invalid pt_index pointer (NULL) passed to PtlPTAlloc\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    if (pt_index_req != PTL_PT_ANY) {
        pt = &(nit.tables[ni.s.ni][pt_index_req]);
        PTL_LOCK_LOCK(pt->lock);
        if (pt->status != PT_FREE) {
            PTL_LOCK_UNLOCK(pt->lock);
            return PTL_PT_IN_USE;
        }
        pt->status = PT_DISABLED;
        *pt_index  = pt_index_req;
    } else {
        ptl_pt_index_t pti;
        for (pti = 0; pti <= nit_limits[ni.s.ni].max_pt_index; ++pti) {
            if (nit.tables[ni.s.ni][pti].status == PT_FREE) {
                pt = &(nit.tables[ni.s.ni][pti]);
                PTL_LOCK_LOCK(pt->lock);
                if (pt->status == PT_FREE) {
                    *pt_index  = pti;
                    pt->status = PT_DISABLED;
                    break;
                }
                PTL_LOCK_UNLOCK(pt->lock);
                pt = NULL;
            }
        }
        if (pt == NULL) {
            return PTL_PT_FULL;
        }
    }
    assert(pt->priority.head == NULL);
    assert(pt->priority.tail == NULL);
    assert(pt->overflow.head == NULL);
    assert(pt->overflow.head == NULL);
    pt->EQ      = eq_handle;
    pt->options = options;
    pt->status  = PT_ENABLED;
    PTL_LOCK_UNLOCK(pt->lock);
    return PTL_OK;
}                                      /*}}} */

int API_FUNC PtlPTFree(ptl_handle_ni_t ni_handle,
                       ptl_pt_index_t  pt_index)
{                                      /*{{{ */
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_table_entry_t                    *pt = NULL;

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        VERBOSE_ERROR("Not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR
            ("ni.s.ni too big (%u >= 4) or ni.s.code wrong (%u != 0) or nit not initialized\n",
            ni.s.ni, ni.s.code);
        return PTL_ARG_INVALID;
    }
    if (pt_index == PTL_PT_ANY) {
        VERBOSE_ERROR("pt_index is PTL_PT_ANY\n");
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index is too big (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    pt = &(nit.tables[ni.s.ni][pt_index]);
    if (pt->priority.head || pt->priority.tail || pt->overflow.head ||
        pt->overflow.tail) {
        VERBOSE_ERROR("pt in use (%p,%p)\n", pt->priority.head,
                      pt->overflow.head);
        return PTL_PT_IN_USE;
    }
    switch (PtlInternalAtomicCas32(&pt->status, PT_ENABLED, PT_FREE)) {
        default:
        case PT_FREE:                 // already free'd (double-free)
            VERBOSE_ERROR("pt_index(%i) already free'd\n", pt_index);
            return PTL_ARG_INVALID;

        case PT_ENABLED:              // success!
            return PTL_OK;

        case PT_DISABLED:             // OK, it was disabled, so...
            switch (PtlInternalAtomicCas32(&pt->status, PT_DISABLED, PT_FREE)) {
                default:
                case PT_FREE:         // already free'd (double-free)
                case PT_ENABLED:      // if this happens, someone else is monkeying with this PT
                    VERBOSE_ERROR("someone else is monkeying with this pt_index in a highly suspicious manner\n");
                    return PTL_ARG_INVALID;

                case PT_DISABLED:
                    return PTL_OK;
            }
    }
}                                      /*}}} */

int API_FUNC PtlPTDisable(Q_UNUSED ptl_handle_ni_t ni_handle,
                          Q_UNUSED ptl_pt_index_t  pt_index)
{                                      /*{{{ */
    fprintf(stderr, "PtlPTDisable() unimplemented\n");
    return PTL_FAIL;
}                                      /*}}} */

int API_FUNC PtlPTEnable(Q_UNUSED ptl_handle_ni_t ni_handle,
                         Q_UNUSED ptl_pt_index_t  pt_index)
{                                      /*{{{ */
    fprintf(stderr, "PtlPTEnable() unimplemented\n");
    return PTL_FAIL;
}                                      /*}}} */

/**************************************************************************************************
 * Internal Functions
 */

void INTERNAL PtlInternalPTInit(ptl_table_entry_t *t)
{                                      /*{{{ */
    PTL_LOCK_INIT(t->lock);
    t->priority.head = NULL;
    t->priority.tail = NULL;
    t->overflow.head = NULL;
    t->overflow.tail = NULL;
    t->EQ            = PTL_EQ_NONE;
    t->status        = PT_FREE;
}                                      /*}}} */

int INTERNAL PtlInternalPTValidate(ptl_table_entry_t *t)
{                                      /*{{{ */
#ifndef NO_ARG_VALIDATION
    if (PtlInternalEQHandleValidator(t->EQ, 1)) {
        VERBOSE_ERROR("PTValidate sees invalid EQ handle\n");
        return 3;
    }
#endif
    switch (t->status) {
        case PT_FREE:
            VERBOSE_ERROR("PT has not been allocated\n");
            return 1;

        case PT_DISABLED:
            VERBOSE_ERROR("PT has been disabled\n");
            return 2;

        case PT_ENABLED:
            return 0;

        default:
            // should never happen
            *(int *)0 = 0;
            return 3;
    }
}                                      /*}}} */

void INTERNAL PtlInternalPTBufferUnexpectedHeader(ptl_table_entry_t *restrict const           t,
                                                  const ptl_internal_header_t *restrict const hdr,
                                                  const uintptr_t                             entry_ptr,
                                                  const uintptr_t                             data)
{                                      /*{{{ */
    ptl_internal_buffered_header_t *restrict bhdr =
        PtlInternalAllocUnexpectedHeader(hdr->ni);

    memcpy(&(bhdr->hdr), hdr, sizeof(ptl_internal_header_t));
    bhdr->buffered_data    = (void *)data;
    bhdr->unexpected_entry = (void *)entry_ptr;
    bhdr->hdr.next         = NULL;
    if (t->buffered_headers.head == NULL) {
        t->buffered_headers.head = bhdr;
    } else {
        ((ptl_internal_buffered_header_t *)(t->buffered_headers.tail))->hdr.
        next = bhdr;
    }
    t->buffered_headers.tail = bhdr;
}                                      /*}}} */

/* vim:set expandtab: */
