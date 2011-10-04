#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(HAVE_GETTIME_TIMER)
# define _POSIX_C_SOURCE 199309L
#endif

/* The API definition */
#include <portals4.h>

/* System headers */
#include <stdlib.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_timer.h"
#include "ptl_internal_papi.h"
#include "ptl_internal_alignment.h"
#ifndef NO_ARG_VALIDATION
# include "ptl_internal_commpad.h"
# include "ptl_internal_error.h"
#endif

const ptl_internal_handle_converter_t eq_none = {
    .s = {
        .selector = HANDLE_EQ_CODE,
        .ni       = ((1 << HANDLE_NI_BITS) - 1),
        .code     = ((1 << HANDLE_CODE_BITS) - 1)
    }
};

typedef union {
    struct {
        uint16_t sequence;
        uint16_t offset;
    } s;
    uint32_t u;
} eq_off_t;

typedef struct {
    ptl_internal_event_t *ring;
    uint32_t              size;
    volatile eq_off_t     leading_head, lagging_head, leading_tail, lagging_tail;
} ptl_internal_eq_t ALIGNED (CACHELINE_WIDTH);

static ptl_internal_eq_t *eqs[4] = { NULL, NULL, NULL, NULL };
static volatile uint64_t *eq_refcounts[4] = { NULL, NULL, NULL, NULL };

void INTERNAL PtlInternalEQNISetup(unsigned int ni)
{   /*{{{*/
    ptl_internal_eq_t *tmp;

    while ((tmp = PtlInternalAtomicCasPtr(&(eqs[ni]), NULL,
                                          (void *)1)) == (void *)1) SPINLOCK_BODY();
    if (tmp == NULL) {
        ALIGNED_CALLOC(tmp, CACHELINE_WIDTH, nit_limits[ni].max_eqs,
                       sizeof(ptl_internal_eq_t));
        assert(tmp != NULL);
        assert(eq_refcounts[ni] == NULL);
        ALIGNED_CALLOC(eq_refcounts[ni], CACHELINE_WIDTH,
                       nit_limits[ni].max_eqs,
                       sizeof(uint64_t));
        assert(eq_refcounts[ni] != NULL);
        __sync_synchronize();
        eqs[ni] = tmp;
    }
} /*}}}*/

void INTERNAL PtlInternalEQNITeardown(unsigned int ni)
{   /*{{{*/
    ptl_internal_eq_t *restrict tmp;
    volatile uint64_t *restrict rc;

    while (eqs[ni] == (void *)1) SPINLOCK_BODY();     // just in case (should never happen in sane code)
    tmp = PtlInternalAtomicSwapPtr((void *volatile *)&eqs[ni], NULL);
    rc  = PtlInternalAtomicSwapPtr((void *volatile *)&eq_refcounts[ni], NULL);
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    assert(rc != NULL);
    for (size_t i = 0; i < nit_limits[ni].max_eqs; ++i) {
        if (rc[i] != 0) {
            PtlInternalAtomicInc(&(rc[i]), -1);
            while (rc[i] != 0) SPINLOCK_BODY();
            free(tmp[i].ring);
            tmp[i].ring = NULL;
        }
    }
    ALIGNED_FREE(tmp, CACHELINE_WIDTH);
    ALIGNED_FREE((void *)rc, CACHELINE_WIDTH);
} /*}}}*/

#ifndef NO_ARG_VALIDATION
int INTERNAL PtlInternalEQHandleValidator(ptl_handle_eq_t handle,
                                          int             none_ok)
{   /*{{{*/
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
    if (eq_refcounts[eq.s.ni][eq.s.code] == 0) {
        VERBOSE_ERROR("EQ(%i,%i) appears to be deallocated\n", (int)eq.s.ni,
                      (int)eq.s.code);
        return PTL_ARG_INVALID;
    }
    return PTL_OK;
} /*}}}*/

#endif /* ifndef NO_ARG_VALIDATION */

int API_FUNC PtlEQAlloc(ptl_handle_ni_t  ni_handle,
                        ptl_size_t       count,
                        ptl_handle_eq_t *eq_handle)
{   /*{{{*/
    const ptl_internal_handle_converter_t ni  = { ni_handle };
    ptl_internal_handle_converter_t       eqh = { .s.selector = HANDLE_EQ_CODE };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if (nit.tables[ni.s.ni] == NULL) { // this should never happen
        assert(nit.tables[ni.s.ni] != NULL);
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
    assert(eqs[ni.s.ni] != NULL);
    eqh.s.ni = ni.s.ni;
    /* make count the next highest power of two (fast algorithm modified from
    * http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2) */
    if (count == 0) {
        count = 2;
    } else {
        count--;
        count |= count >> 1;
        count |= count >> 2;
        count |= count >> 4;
        count |= count >> 8;
        count++;
    }
    /* find an EQ handle */
    {
        ptl_internal_eq_t *ni_eqs = eqs[ni.s.ni];
        volatile uint64_t *rc     = eq_refcounts[ni.s.ni];
        for (uint32_t offset = 0;
             offset < nit_limits[ni.s.ni].max_eqs;
             ++offset) {
            if (rc[offset] == 0) {
                if (PtlInternalAtomicCas64(&(rc[offset]), 0, 1) == 0) {
                    ptl_internal_event_t *tmp;
                    ALIGNED_CALLOC(tmp, CACHELINE_WIDTH, count,
                                   sizeof(ptl_internal_event_t));
                    if (tmp == NULL) {
                        rc[offset] = 0;
                        return PTL_NO_SPACE;
                    }
                    eqh.s.code                              = offset;
                    ni_eqs[offset].leading_head.s.offset    = 0;
                    ni_eqs[offset].leading_head.s.sequence += 7;
                    ni_eqs[offset].lagging_head             = ni_eqs[offset].leading_head;
                    ni_eqs[offset].leading_tail.s.offset    = 0;
                    ni_eqs[offset].leading_tail.s.sequence += 11;
                    ni_eqs[offset].lagging_tail             = ni_eqs[offset].leading_tail;
                    ni_eqs[offset].size                     = count;
                    ni_eqs[offset].ring                     = tmp;
                    *eq_handle                              = eqh.a;
                    return PTL_OK;
                }
            }
        }
        *eq_handle = PTL_INVALID_HANDLE;
        return PTL_NO_SPACE;
    }
} /*}}}*/

int API_FUNC PtlEQFree(ptl_handle_eq_t eq_handle)
{   /*{{{*/
    const ptl_internal_handle_converter_t eqh = { eq_handle };
    ptl_internal_event_t                 *tmp;
    ptl_internal_eq_t                    *eq;

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
    eq = &(eqs[eqh.s.ni][eqh.s.code]);
    assert(eq->leading_tail.s.offset == eq->lagging_tail.s.offset);
    if (eq->leading_tail.s.offset != eq->lagging_tail.s.offset) {       // this EQ is busy
        return PTL_ARG_INVALID;
    }
    // should probably enqueue a death-event
    while (eq_refcounts[eqh.s.ni][eqh.s.code] != 1) SPINLOCK_BODY();
    tmp      = eq->ring;
    eq->ring = NULL;
    ALIGNED_FREE(tmp, CACHELINE_WIDTH);
    PtlInternalAtomicInc(&(eq_refcounts[eqh.s.ni][eqh.s.code]), -1);
    return PTL_OK;
} /*}}}*/

#define ASSIGN_EVENT(e, ie, ni) do { /*{{{*/                                                \
        e->type = (ptl_event_kind_t)(ie.type);                                              \
        switch (e->type) {                                                                  \
            case PTL_EVENT_ATOMIC: case PTL_EVENT_ATOMIC_OVERFLOW:             /* target */ \
            case PTL_EVENT_FETCH_ATOMIC: case PTL_EVENT_FETCH_ATOMIC_OVERFLOW: /* target */ \
                e->atomic_operation = (ptl_op_t)ie.atomic_operation;                        \
                e->atomic_type      = (ptl_datatype_t)ie.atomic_type;                       \
            case PTL_EVENT_PUT:                                                             \
            case PTL_EVENT_PUT_OVERFLOW:                                                    \
            case PTL_EVENT_SEARCH:                                          /* target */    \
                e->hdr_data = ie.hdr_data;                                                  \
            case PTL_EVENT_GET: case PTL_EVENT_GET_OVERFLOW:                                \
                if (ni <= 1) { /* logical */                                                \
                    e->initiator.rank = ie.initiator.rank;                                  \
                } else { /* physical */                                                     \
                    e->initiator.phys.pid = ie.initiator.phys.pid;                          \
                    e->initiator.phys.nid = ie.initiator.phys.nid;                          \
                }                                                                           \
                e->initiator.phys.nid = ie.initiator.phys.nid; /* this handles rank too */  \
                e->initiator.phys.pid = ie.initiator.phys.pid;                              \
                e->uid                = ie.uid;                                             \
                e->match_bits         = ie.match_bits;                                      \
                e->rlength            = ie.rlength;                                         \
                e->mlength            = ie.mlength;                                         \
                e->remote_offset      = ie.remote_offset;                                   \
                e->start              = ie.start;                                           \
            case PTL_EVENT_AUTO_UNLINK:                                     /* target */    \
            case PTL_EVENT_AUTO_FREE:                                       /* target */    \
            case PTL_EVENT_LINK:                                            /* target */    \
                e->user_ptr = ie.user_ptr;                                                  \
            case PTL_EVENT_PT_DISABLED:                                                     \
                e->pt_index     = ie.pt_index;                                              \
                e->ni_fail_type = ie.ni_fail_type;                                          \
                break;                                                                      \
            case PTL_EVENT_REPLY: case PTL_EVENT_ACK: /* initiator */                       \
                e->mlength       = ie.mlength;                                              \
                e->remote_offset = ie.remote_offset;                                        \
            case PTL_EVENT_SEND:                                                            \
                e->user_ptr     = ie.user_ptr;                                              \
                e->ni_fail_type = ie.ni_fail_type;                                          \
                break;                                                                      \
        }                                                                                   \
} while (0) /*}}}*/

int API_FUNC PtlEQGet(ptl_handle_eq_t eq_handle,
                      ptl_event_t    *event)
{   /*{{{*/
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
    const ptl_internal_handle_converter_t eqh  = { eq_handle };
    ptl_internal_eq_t *const              eq   = &(eqs[eqh.s.ni][eqh.s.code]);
    const uint32_t                        mask = eq->size - 1;
    eq_off_t                              readidx, curidx, newidx;

    PtlInternalPAPIStartC();
    /* first, increment the leading_head */
    curidx = eq->leading_head;
    do {
        readidx = curidx;
        if (readidx.s.offset == eq->lagging_tail.s.offset) {
            return PTL_EQ_EMPTY;
        }
        newidx.s.sequence = (uint16_t)(readidx.s.sequence + 23);        // a prime number
        newidx.s.offset   = (uint16_t)((readidx.s.offset + 1) & mask);
    } while ((curidx.u = PtlInternalAtomicCas32(&eq->leading_head.u, readidx.u, newidx.u)) != readidx.u);
    /* second, read from the queue with the offset I got from leading_head */
    ASSIGN_EVENT(event, eq->ring[readidx.s.offset], eqh.s.ni);
    __sync_synchronize();
    /* third, wait for the lagging_head to catch up */
    while (eq->lagging_head.u != readidx.u) SPINLOCK_BODY();
    /* and finally, push the lagging_head along */
    eq->lagging_head = newidx;
    PtlInternalPAPIDoneC(PTL_EQ_GET, 0);
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlEQWait(ptl_handle_eq_t eq_handle,
                       ptl_event_t    *event)
{   /*{{{*/
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
    const ptl_internal_handle_converter_t eqh  = { eq_handle };
    ptl_internal_eq_t *const              eq   = &(eqs[eqh.s.ni][eqh.s.code]);
    const uint32_t                        mask = eq->size - 1;
    volatile uint64_t                    *rc   = &(eq_refcounts[eqh.s.ni][eqh.s.code]);
    eq_off_t                              readidx, curidx, newidx;

    PtlInternalAtomicInc(rc, 1);
    /* first, increment the leading_head */
    curidx = eq->leading_head;
    do {
loopstart:
        readidx = curidx;
        if (readidx.s.offset >= eq->size) {
            PtlInternalAtomicInc(rc, -1);
            return PTL_INTERRUPTED;
        } else if (readidx.s.offset == eq->lagging_tail.s.offset) {
            curidx = eq->leading_head;
            goto loopstart;
        }
        newidx.s.sequence = (uint16_t)(readidx.s.sequence + 23);        // a prime number
        newidx.s.offset   = (uint16_t)((readidx.s.offset + 1) & mask);
    } while ((curidx.u = PtlInternalAtomicCas32(&eq->leading_head.u, readidx.u, newidx.u)) != readidx.u);
    /* second, read from the queue with the offset I got from leading_head */
    ASSIGN_EVENT(event, eq->ring[readidx.s.offset], eqh.s.ni);
    __sync_synchronize();
    /* third, wait for the lagging_head to catch up */
    while (eq->lagging_head.u != readidx.u) SPINLOCK_BODY();
    /* and finally, push the lagging_head along */
    eq->lagging_head = newidx;
    PtlInternalAtomicInc(rc, -1);
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlEQPoll(ptl_handle_eq_t *eq_handles,
                       unsigned int     size,
                       ptl_time_t       timeout,
                       ptl_event_t     *event,
                       unsigned int    *which)
{   /*{{{*/
    ptl_size_t eqidx, offset;
    size_t     nstart;
    TIMER_TYPE tp;

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
    ptl_internal_eq_t *eqs[size];
    uint32_t           masks[size];
    volatile uint64_t *rcs[size];
    int                ni = 0;
    for (eqidx = 0; eqidx < size; ++eqidx) {
        const ptl_internal_handle_converter_t eqh = { eq_handles[eqidx] };
        ni           = eqh.s.ni;
        eqs[eqidx]   = &(eqs[eqh.s.ni][eqh.s.code]);
        masks[eqidx] = eqs[eqidx]->size - 1;
        rcs[eqidx]   = &(eq_refcounts[eqh.s.ni][eqh.s.code]);
        PtlInternalAtomicInc(rcs[eqidx], 1);
    }

    {
        TIMER_TYPE start;
        MARK_TIMER(start);
        nstart = TIMER_INTS(start);
    }
    if (timeout != PTL_TIME_FOREVER) { // convert from milliseconds to timer units
        MILLI_TO_TIMER_INTS(timeout);
    }
    {
        uint16_t t = (uint16_t)(size - 1);
        t      = (uint16_t)(t | (t >> 1));
        t      = (uint16_t)(t | (t >> 2));
        t      = (uint16_t)(t | (t >> 4));
        t      = (uint16_t)(t | (t >> 8));
        offset = nstart & t;           // pseudo-random
    }
    do {
        for (eqidx = 0; eqidx < size; ++eqidx) {
            const ptl_size_t         ridx = (eqidx + offset) % size;
            ptl_internal_eq_t *const eq   = eqs[ridx];
            const uint32_t           mask = masks[ridx];
            eq_off_t                 readidx, curidx, newidx;

            /* first, read from the leading_head */
            curidx = eq->leading_head;
            do {
                readidx = curidx;
                if (readidx.s.offset >= eq->size) {
                    for (size_t idx = 0; idx < size; ++idx) PtlInternalAtomicInc(rcs[idx], -1);
                    return PTL_INTERRUPTED;
                } else if (readidx.s.offset == eq->lagging_tail.s.offset) {
                    continue;
                }
                newidx.s.sequence = (uint16_t)(readidx.s.sequence + 23);        // a prime number
                newidx.s.offset   = (uint16_t)((readidx.s.offset + 1) & mask);
            } while ((curidx.u = PtlInternalAtomicCas32(&eq->leading_head.u, readidx.u, newidx.u)) != readidx.u);
            /* second, read from the queue with the offset I got from leading_head */
            ASSIGN_EVENT(event, eq->ring[readidx.s.offset], ni);
            __sync_synchronize();
            /* third, wait for the lagging_head to catch up */
            while (eq->lagging_head.u != readidx.u) SPINLOCK_BODY();
            /* and finally, push the lagging_head along */
            eq->lagging_head = newidx;

            for (size_t idx = 0; idx < size; ++idx) PtlInternalAtomicInc(rcs[idx], -1);
            *which = (unsigned int)newidx.s.offset;
            return PTL_OK;
        }
        MARK_TIMER(tp);
    } while (timeout == PTL_TIME_FOREVER ||
             (TIMER_INTS(tp) - nstart) < timeout);
    for (size_t idx = 0; idx < size; ++idx) PtlInternalAtomicInc(rcs[idx], -1);
    return PTL_EQ_EMPTY;
} /*}}}*/

void INTERNAL PtlInternalEQPush(ptl_handle_eq_t       eq_handle,
                                ptl_internal_event_t *event)
{   /*{{{*/
    const ptl_internal_handle_converter_t eqh  = { eq_handle };
    ptl_internal_eq_t *const              eq   = &(eqs[eqh.s.ni][eqh.s.code]);
    const uint32_t                        mask = eq->size - 1;
    eq_off_t                              writeidx, curidx, newidx;

    // first, get a location from the leading_tail
    curidx = eq->leading_tail;
    do {
        while (((curidx.s.offset+1) & mask) == eq->lagging_head.s.offset) {
            SPINLOCK_BODY();
            curidx = eq->leading_tail;
        }
        writeidx          = curidx;
        newidx.s.sequence = (uint16_t)(writeidx.s.sequence + 23);
        newidx.s.offset   = (uint16_t)((writeidx.s.offset + 1) & mask);
    } while ((curidx.u = PtlInternalAtomicCas32(&eq->leading_tail.u, writeidx.u, newidx.u)) != writeidx.u);
    // at this point, we have a writeidx offset to fill
    eq->ring[writeidx.s.offset] = *event;
    // now, wait for our neighbor to finish
    while (eq->lagging_tail.u != writeidx.u) SPINLOCK_BODY();

    // now, update the lagging_tail
    eq->lagging_tail = newidx;
} /*}}}*/

void INTERNAL PtlInternalEQPushESEND(const ptl_handle_eq_t eq_handle,
                                     const uint32_t        length,
                                     const uint64_t        roffset,
                                     void *const           user_ptr)
{   /*{{{*/
    const ptl_internal_handle_converter_t eqh  = { eq_handle };
    ptl_internal_eq_t *const              eq   = &(eqs[eqh.s.ni][eqh.s.code]);
    const uint32_t                        mask = eq->size - 1;
    eq_off_t                              writeidx, curidx, newidx;

    // first, get a location from the leading_tail
    curidx = eq->leading_tail;
    do {
        while (((curidx.s.offset+1) & mask) == eq->lagging_head.s.offset) {
            SPINLOCK_BODY();
            curidx = eq->leading_tail;
        }
        writeidx          = curidx;
        newidx.s.sequence = (uint16_t)(writeidx.s.sequence + 23);
        newidx.s.offset   = (uint16_t)((writeidx.s.offset + 1) & mask);
    } while ((curidx.u = PtlInternalAtomicCas32(&eq->leading_tail.u, writeidx.u, newidx.u)) != writeidx.u);
    // at this point, we have a writeidx offset to fill
    eq->ring[writeidx.s.offset].type          = PTL_EVENT_SEND;
    eq->ring[writeidx.s.offset].mlength       = length;
    eq->ring[writeidx.s.offset].remote_offset = roffset;
    eq->ring[writeidx.s.offset].user_ptr      = user_ptr;
    eq->ring[writeidx.s.offset].ni_fail_type  = PTL_NI_OK;
    // now, wait for our neighbor to finish
    while (eq->lagging_tail.u != writeidx.u) SPINLOCK_BODY();
    // now, update the lagging_tail
    eq->lagging_tail = newidx;
} /*}}}*/

/* vim:set expandtab: */
