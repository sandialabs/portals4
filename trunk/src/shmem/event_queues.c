#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(HAVE_GETTIME_TIMER)
#define _POSIX_C_SOURCE 199309L
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
#include "ptl_internal_error.h"
#include "ptl_internal_timer.h"
#ifndef NO_ARG_VALIDATION
#include "ptl_internal_commpad.h"
#endif

const ptl_internal_handle_converter_t eq_none = { .s = { .selector=HANDLE_EQ_CODE, .ni=((1<<HANDLE_NI_BITS)-1), .code=((1<<HANDLE_CODE_BITS)-1) } };
const ptl_handle_eq_t PTL_EQ_NONE = 0x3fffffff;	/* (1<<29) & 0x1fffffff */

typedef union {
    struct {
	uint16_t sequence;
	uint16_t offset;
    } s;
    uint32_t u;
} eq_off_t;

typedef struct {
    ptl_match_bits_t match_bits;	// 8 bytes
    void *start;		// 8 bytes (16)
    void *user_ptr;		// 8 bytes (24)
    ptl_hdr_data_t hdr_data;	// 8 bytes (32)
    uint32_t rlength;		// 4 bytes (36)
    uint32_t mlength;		// 4 bytes (40)
    union {
	uint32_t rank;
	struct {
	    uint16_t nid;
	    uint16_t pid;
	} phys;
    } initiator;		// 4 bytes (44)
    uint16_t uid;		// 2 bytes (46)
    uint16_t jid;		// 2 bytes (48)
    uint64_t remote_offset:40;	// 5 bytes (53)
    uint8_t pt_index:5;
    uint8_t ni_fail_type:2;
    uint8_t atomic_operation:5;
    uint8_t atomic_type:4;
} ptl_internal_target_event_t;	// grand total: 56 bytes

typedef struct {
    union {
	ptl_internal_target_event_t tevent;
	ptl_initiator_event_t ievent;
    } event;
    uint8_t type;
} ptl_internal_event_t;

typedef struct {
    ptl_internal_event_t *ring;
    uint32_t size;
    volatile eq_off_t head, leading_tail, lagging_tail;
} ptl_internal_eq_t;

static ptl_internal_eq_t *eqs[4] = { NULL, NULL, NULL, NULL };
static volatile uint64_t *eq_refcounts[4] = { NULL, NULL, NULL, NULL };

#define PSIZE(x) printf("\t%s @%lu\n", #x, offsetof(ptl_internal_target_event_t,x))

void INTERNAL PtlInternalEQNISetup(
    unsigned int ni)
{
    ptl_internal_eq_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(eqs[ni]), NULL,
				    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
	tmp = calloc(nit_limits.max_eqs, sizeof(ptl_internal_eq_t));
	assert(tmp != NULL);
	assert(eq_refcounts[ni] == NULL);
	eq_refcounts[ni] = calloc(nit_limits.max_eqs, sizeof(uint64_t));
	assert(eq_refcounts[ni] != NULL);
	__sync_synchronize();
	eqs[ni] = tmp;
    }
}

void INTERNAL PtlInternalEQNITeardown(
    unsigned int ni)
{
    ptl_internal_eq_t *restrict tmp;
    volatile uint64_t *restrict rc;
    while (eqs[ni] == (void *)1) ;     // just in case (should never happen in sane code)
    tmp = PtlInternalAtomicSwapPtr((void *volatile *)&eqs[ni], NULL);
    rc = PtlInternalAtomicSwapPtr((void *volatile *)&eq_refcounts[ni], NULL);
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    assert(rc != NULL);
    for (size_t i = 0; i < nit_limits.max_eqs; ++i) {
	if (rc[i] != 0) {
	    PtlInternalAtomicInc(&(rc[i]), -1);
	    while (rc[i] != 0) ;
	    free(tmp[i].ring);
	    tmp[i].ring = NULL;
	}
    }
    free(tmp);
    free((void *)rc);
}


int INTERNAL PtlInternalEQHandleValidator(
    ptl_handle_eq_t handle,
    int none_ok)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t eq = { handle };
    if (eq.s.selector != HANDLE_EQ_CODE) {
	VERBOSE_ERROR("Expected EQ handle, but it's not one (%u != %u, 0x%lx, 0x%lx)\n", eq.s.selector, HANDLE_EQ_CODE, handle, eq_none.i);
	return PTL_ARG_INVALID;
    }
    if (none_ok == 1 && handle == PTL_EQ_NONE) {
	return PTL_OK;
    }
    if (eq.s.ni > 3 || eq.s.code > nit_limits.max_eqs ||
	(nit.refcount[eq.s.ni] == 0)) {
	VERBOSE_ERROR
	    ("EQ NI too large (%u > 3) or code is wrong (%u > %u) or nit table is uninitialized\n",
	     eq.s.ni, eq.s.code, nit_limits.max_cts);
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
#endif
    return PTL_OK;
}

int API_FUNC PtlEQAlloc(
    ptl_handle_ni_t ni_handle,
    ptl_size_t count,
    ptl_handle_eq_t * eq_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t eqh = {.s.selector = HANDLE_EQ_CODE };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
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
#endif
    assert(eqs[ni.s.ni] != NULL);
    eqh.s.ni = ni.s.ni;
    /* make count the next highest power of two (fast algorithm modified from
     * http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2) */
    if (count == 0)
	count = 2;
    else {
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
	volatile uint64_t *rc = eq_refcounts[ni.s.ni];
	for (uint32_t offset = 0; offset < nit_limits.max_eqs; ++offset) {
	    if (rc[offset] == 0) {
		if (PtlInternalAtomicCas64(&(rc[offset]), 0, 1) == 0) {
		    ptl_internal_event_t *tmp =
			calloc(count, sizeof(ptl_internal_event_t));
		    if (tmp == NULL) {
			rc[offset] = 0;
			return PTL_NO_SPACE;
		    }
		    eqh.s.code = offset;
		    ni_eqs[offset].head.s.offset =
			ni_eqs[offset].leading_tail.s.offset = 0;
		    ni_eqs[offset].head.s.sequence += 7;
		    ni_eqs[offset].leading_tail.s.sequence += 11;
		    ni_eqs[offset].lagging_tail = ni_eqs[offset].leading_tail;
		    ni_eqs[offset].size = count;
		    ni_eqs[offset].ring = tmp;
		    *eq_handle = eqh.a.eq;
		    return PTL_OK;
		}
	    }
	}
	*eq_handle = PTL_INVALID_HANDLE.eq;
	return PTL_NO_SPACE;
    }
}

int API_FUNC PtlEQFree(
    ptl_handle_eq_t eq_handle)
{
    const ptl_internal_handle_converter_t eqh = { eq_handle };
    ptl_internal_event_t *tmp;
    ptl_internal_eq_t *eq;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
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
    if (eq->leading_tail.s.offset != eq->lagging_tail.s.offset) {	// this EQ is busy
	return PTL_ARG_INVALID;
    }
    // should probably enqueue a death-event
    while (eq_refcounts[eqh.s.ni][eqh.s.code] != 1) ;
    tmp = eq->ring;
    eq->ring = NULL;
    free(tmp);
    PtlInternalAtomicInc(&(eq_refcounts[eqh.s.ni][eqh.s.code]), -1);
    return PTL_OK;
}

#define ASSIGN_EVENT(e,ie,ni) do { \
    e->type = (ptl_event_kind_t)(ie.type); \
    switch (e->type) { \
	case PTL_EVENT_GET: case PTL_EVENT_PUT: case PTL_EVENT_PUT_OVERFLOW: case PTL_EVENT_ATOMIC: \
	case PTL_EVENT_ATOMIC_OVERFLOW: case PTL_EVENT_DROPPED: case PTL_EVENT_PT_DISABLED: \
	case PTL_EVENT_UNLINK: case PTL_EVENT_FREE: case PTL_EVENT_PROBE: /* target */ \
	    e->event.tevent.match_bits = ie.event.tevent.match_bits; \
	    e->event.tevent.start = ie.event.tevent.start; \
	    e->event.tevent.user_ptr = ie.event.tevent.user_ptr; \
	    e->event.tevent.hdr_data = ie.event.tevent.hdr_data; \
	    e->event.tevent.rlength = ie.event.tevent.rlength; \
	    e->event.tevent.mlength = ie.event.tevent.mlength; \
	    if (ni <= 1) { /* logical */ \
		e->event.tevent.initiator.rank = ie.event.tevent.initiator.rank; \
	    } else { /* physical */ \
		e->event.tevent.initiator.phys.pid = ie.event.tevent.initiator.phys.pid; \
		e->event.tevent.initiator.phys.nid = ie.event.tevent.initiator.phys.nid; \
	    } \
	    e->event.tevent.initiator.phys.nid = ie.event.tevent.initiator.phys.nid; /* this handles rank too */ \
	    e->event.tevent.initiator.phys.pid = ie.event.tevent.initiator.phys.pid; \
	    e->event.tevent.uid = ie.event.tevent.uid; \
	    e->event.tevent.jid = ie.event.tevent.jid; \
	    e->event.tevent.remote_offset = ie.event.tevent.remote_offset; \
	    e->event.tevent.pt_index = ie.event.tevent.pt_index; \
	    e->event.tevent.ni_fail_type = ie.event.tevent.ni_fail_type; \
	    e->event.tevent.atomic_operation = ie.event.tevent.atomic_operation; \
	    e->event.tevent.atomic_type = ie.event.tevent.atomic_type; \
	    break; \
	case PTL_EVENT_REPLY: case PTL_EVENT_SEND: case PTL_EVENT_ACK: /* initiator */ \
	    e->event.ievent = ie.event.ievent; \
	    break; \
    } \
} while (0)

int API_FUNC PtlEQGet(
    ptl_handle_eq_t eq_handle,
    ptl_event_t * event)
{
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
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
#endif
    const ptl_internal_handle_converter_t eqh = { eq_handle };
    ptl_internal_eq_t *const eq = &(eqs[eqh.s.ni][eqh.s.code]);
    const uint32_t mask = eq->size - 1;
    eq_off_t readidx, curidx, newidx;

    curidx = eq->head;
    do {
	readidx = curidx;
	if (readidx.s.offset == eq->lagging_tail.s.offset) {
	    return PTL_EQ_EMPTY;
	}
	ASSIGN_EVENT(event, eq->ring[readidx.s.offset], eqh.s.ni);
	newidx.s.sequence = (uint16_t) (readidx.s.sequence + 23);	// a prime number
	newidx.s.offset = (uint16_t) ((readidx.s.offset + 1) & mask);
    } while ((curidx.u =
	      PtlInternalAtomicCas32(&eq->head.u, readidx.u,
				     newidx.u)) != readidx.u);
    return PTL_OK;
}

int API_FUNC PtlEQWait(
    ptl_handle_eq_t eq_handle,
    ptl_event_t * event)
{
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
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
#endif
    const ptl_internal_handle_converter_t eqh = { eq_handle };
    ptl_internal_eq_t *const eq = &(eqs[eqh.s.ni][eqh.s.code]);
    const uint32_t mask = eq->size - 1;
    volatile uint64_t *rc = &(eq_refcounts[eqh.s.ni][eqh.s.code]);
    eq_off_t readidx, curidx, newidx;

    PtlInternalAtomicInc(rc, 1);
    curidx = eq->head;
    do {
	readidx = curidx;
	if (readidx.s.offset >= eq->size) {
	    PtlInternalAtomicInc(rc, -1);
	    return PTL_INTERRUPTED;
	} else if (readidx.s.offset == eq->lagging_tail.s.offset) {
	    curidx = eq->head;
	    continue;
	}
	ASSIGN_EVENT(event, eq->ring[readidx.s.offset], eqh.s.ni);
	newidx.s.sequence = (uint16_t) (readidx.s.sequence + 23);	// a prime number
	newidx.s.offset = (uint16_t) ((readidx.s.offset + 1) & mask);
    } while ((curidx.u =
	      PtlInternalAtomicCas32(&eq->head.u, readidx.u,
				     newidx.u)) != readidx.u);
    PtlInternalAtomicInc(rc, -1);
    return PTL_OK;
}

int API_FUNC PtlEQPoll(
    ptl_handle_eq_t * eq_handles,
    int size,
    ptl_time_t timeout,
    ptl_event_t * event,
    int *which)
{
    ptl_size_t eqidx, offset;
    size_t nstart;
    TIMER_TYPE tp;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized\n");
	return PTL_NO_INIT;
    }
    if (size < 0) {
	VERBOSE_ERROR("nonsensical size (%i)", size);
	return PTL_ARG_INVALID;
    }
    if (event == NULL && which == NULL) {
	VERBOSE_ERROR("null event or null which\n");
	return PTL_ARG_INVALID;
    }
    for (eqidx = 0; eqidx < size; ++eqidx) {
	if (PtlInternalEQHandleValidator(eq_handles[eqidx], 0)) {
	    VERBOSE_ERROR("invalid EQ handle\n");
	    return PTL_ARG_INVALID;
	}
    }
#endif
    ptl_internal_eq_t *eqs[size];
    uint32_t masks[size];
    volatile uint64_t *rcs[size];
    int ni = 0;
    for (eqidx = 0; eqidx < size; ++eqidx) {
	const ptl_internal_handle_converter_t eqh = { eq_handles[eqidx] };
	ni = eqh.s.ni;
	eqs[eqidx] = &(eqs[eqh.s.ni][eqh.s.code]);
	masks[eqidx] = eqs[eqidx]->size - 1;
	rcs[eqidx] = &(eq_refcounts[eqh.s.ni][eqh.s.code]);
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
	uint16_t t = (uint16_t) (size - 1);
	t = (uint16_t) (t | (t >> 1));
	t = (uint16_t) (t | (t >> 2));
	t = (uint16_t) (t | (t >> 4));
	t = (uint16_t) (t | (t >> 8));
	offset = nstart & t;	       // pseudo-random
    }
    do {
	for (eqidx = 0; eqidx < size; ++eqidx) {
	    const ptl_size_t ridx = (eqidx + offset) % size;
	    ptl_internal_eq_t *const eq = eqs[ridx];
	    const uint32_t mask = masks[ridx];
	    eq_off_t readidx, curidx, newidx;
	    int found = 1;

	    curidx = eq->head;
	    do {
		readidx = curidx;
		if (readidx.s.offset >= eq->size) {
		    for (size_t idx = 0; idx < size; ++idx)
			PtlInternalAtomicInc(rcs[idx], -1);
		    return PTL_INTERRUPTED;
		} else if (readidx.s.offset == eq->lagging_tail.s.offset) {
		    found = 0;
		    break;
		}
		ASSIGN_EVENT(event, eq->ring[readidx.s.offset], ni);
		newidx.s.sequence = (uint16_t) (readidx.s.sequence + 23);	// a prime number
		newidx.s.offset = (uint16_t) ((readidx.s.offset + 1) & mask);
	    } while ((curidx.u =
		      PtlInternalAtomicCas32(&eq->head.u, readidx.u,
					     newidx.u)) != readidx.u);
	    if (found) {
		for (size_t idx = 0; idx < size; ++idx)
		    PtlInternalAtomicInc(rcs[idx], -1);
		return PTL_OK;
	    }
	}
	MARK_TIMER(tp);
    } while (timeout == PTL_TIME_FOREVER ||
	     (TIMER_INTS(tp) - nstart) < timeout);
    for (size_t idx = 0; idx < size; ++idx)
	PtlInternalAtomicInc(rcs[idx], -1);
    return PTL_EQ_EMPTY;
}

void INTERNAL PtlInternalEQPush(
    ptl_handle_eq_t eq_handle,
    ptl_event_t * event)
{
    const ptl_internal_handle_converter_t eqh = { eq_handle };
    ptl_internal_eq_t *const eq = &(eqs[eqh.s.ni][eqh.s.code]);
    const uint32_t mask = eq->size - 1;
    eq_off_t writeidx, curidx, newidx;

    // first, get a location from the leading_tail
    curidx = eq->leading_tail;
    do {
	writeidx = curidx;
	newidx.s.sequence = (uint16_t) (writeidx.s.sequence + 23);
	newidx.s.offset = (uint16_t) ((writeidx.s.offset + 1) & mask);
    } while ((curidx.u =
	      PtlInternalAtomicCas32(&eq->leading_tail.u, writeidx.u,
				     newidx.u)) != writeidx.u);
    // at this point, we have a writeidx offset to fill
    eq->ring[writeidx.s.offset].type = (uint8_t) (event->type);
    switch (event->type) {
	case PTL_EVENT_GET:
	case PTL_EVENT_PUT:
	case PTL_EVENT_PUT_OVERFLOW:
	case PTL_EVENT_ATOMIC:
	case PTL_EVENT_ATOMIC_OVERFLOW:
	case PTL_EVENT_DROPPED:
	case PTL_EVENT_PT_DISABLED:
	case PTL_EVENT_UNLINK:
	case PTL_EVENT_FREE:
	case PTL_EVENT_PROBE:	       /* target */
	    eq->ring[writeidx.s.offset].event.tevent.match_bits =
		event->event.tevent.match_bits;
	    eq->ring[writeidx.s.offset].event.tevent.start =
		event->event.tevent.start;
	    eq->ring[writeidx.s.offset].event.tevent.user_ptr =
		event->event.tevent.user_ptr;
	    eq->ring[writeidx.s.offset].event.tevent.hdr_data =
		event->event.tevent.hdr_data;
	    eq->ring[writeidx.s.offset].event.tevent.rlength =
		event->event.tevent.rlength;
	    eq->ring[writeidx.s.offset].event.tevent.mlength =
		event->event.tevent.mlength;
	    if (eqh.s.ni <= 1) {       /* logical */
		eq->ring[writeidx.s.offset].event.tevent.initiator.rank =
		    event->event.tevent.initiator.rank;
	    } else {		       /* physical */
		eq->ring[writeidx.s.offset].event.tevent.initiator.phys.pid =
		    (uint16_t) event->event.tevent.initiator.phys.pid;
		eq->ring[writeidx.s.offset].event.tevent.initiator.phys.nid =
		    (uint16_t) event->event.tevent.initiator.phys.nid;
	    }
	    eq->ring[writeidx.s.offset].event.tevent.uid =
		(uint16_t) event->event.tevent.uid;
	    eq->ring[writeidx.s.offset].event.tevent.jid =
		(uint16_t) event->event.tevent.jid;
	    eq->ring[writeidx.s.offset].event.tevent.remote_offset =
		event->event.tevent.remote_offset;
	    eq->ring[writeidx.s.offset].event.tevent.pt_index =
		event->event.tevent.pt_index;
	    eq->ring[writeidx.s.offset].event.tevent.ni_fail_type =
		event->event.tevent.ni_fail_type;
	    eq->ring[writeidx.s.offset].event.tevent.atomic_operation =
		event->event.tevent.atomic_operation;
	    eq->ring[writeidx.s.offset].event.tevent.atomic_type =
		event->event.tevent.atomic_type;
	    break;
	case PTL_EVENT_REPLY:
	case PTL_EVENT_SEND:
	case PTL_EVENT_ACK:	       /* initiator */
	    eq->ring[writeidx.s.offset].event.ievent = event->event.ievent;
	    break;
    }
    // now, wait for our neighbor to finish
    while (eq->lagging_tail.u != writeidx.u) ;
    // now, update the lagging_tail
    eq->lagging_tail = newidx;
}
