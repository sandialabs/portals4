/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <stdlib.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_error.h"
#ifndef NO_ARG_VALIDATION
#include "ptl_internal_commpad.h"
#endif

const ptl_handle_eq_t PTL_EQ_NONE = 0x3fffffff;	/* (1<<29) & 0x1fffffff */

typedef union {
    struct {
	uint16_t sequence;
	uint16_t offset;
    } s;
    uint32_t u;
} eq_off_t;

typedef struct {
    ptl_event_t *ring;
    uint32_t size;
    volatile eq_off_t head, tail, written_tail;
} ptl_internal_eq_t;

static ptl_internal_eq_t *eqs[4] = { NULL, NULL, NULL, NULL };

void INTERNAL PtlInternalEQNISetup(
	unsigned int ni)
{
    ptl_internal_eq_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(eqs[ni]), NULL, (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
	tmp = calloc(nit_limits.max_eqs, sizeof(ptl_internal_eq_t));
	assert(tmp != NULL);
	__sync_synchronize();
	eqs[ni] = tmp;
    }
}

void INTERNAL PtlInternalEQNITeardown(
    unsigned int ni)
{
    ptl_internal_eq_t *tmp;
    tmp = eqs[ni];
    eqs[ni] = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    for (size_t i=0; i<nit_limits.max_eqs; ++i) {
	if (tmp[i].ring != NULL) {
	    while (tmp[i].ring == (void*)1) ; // just in case (this should never, realistically happen)
#warning need to deal with allocated EQs at Teardown
	    free(tmp[i].ring);
	    tmp[i].ring = NULL;
	}
    }
    free(tmp);
}


int INTERNAL PtlInternalEQHandleValidator(
    ptl_handle_eq_t handle,
    int none_ok)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t eq = { handle };
    if (eq.s.selector != HANDLE_EQ_CODE) {
	VERBOSE_ERROR("Expected EQ handle, but it's not one\n");
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
    if (eqs[eq.s.ni][eq.s.code].ring == NULL) {
	VERBOSE_ERROR("EQ appears to be deallocated\n");
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
    ptl_internal_handle_converter_t eqh = { .s.selector = HANDLE_EQ_CODE };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized\n");
	return PTL_NO_INIT;
    }
    if (ni.s.ni >= 4 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
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
    if (count == 0) count = 2;
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
	for (uint32_t offset = 0; offset < nit_limits.max_eqs; ++offset) {
	    if (ni_eqs[offset].ring == NULL) {
		if (PtlInternalAtomicCasPtr
			(&(ni_eqs[offset].ring), NULL, (void *)1) == NULL) {
		    ptl_event_t *tmp = calloc(count, sizeof(ptl_event_t));
		    if (tmp == NULL) {
			ni_eqs[offset].ring = NULL;
			return PTL_NO_SPACE;
		    }
		    eqh.s.code = offset;
		    ni_eqs[offset].head.s.offset = ni_eqs[offset].tail.s.offset = ni_eqs[offset].written_tail.s.offset = 0;
		    ni_eqs[offset].head.s.sequence += 7;
		    ni_eqs[offset].tail.s.sequence += 11;
		    ni_eqs[offset].written_tail.s.sequence += 13;
		    ni_eqs[offset].size = count;
		    ni_eqs[offset].ring = tmp;
		    break;
		}
	    }
	}
    }
    *eq_handle = eqh.a.eq;
    return PTL_OK;
}

int API_FUNC PtlEQFree(
    ptl_handle_eq_t eq_handle)
{
    const ptl_internal_handle_converter_t eqh = { eq_handle };
    ptl_event_t *tmp;
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
    assert(eq->head.s.offset == eq->tail.s.offset && eq->tail.s.offset == eq->written_tail.s.offset);
    if (eq->head.s.offset != eq->tail.s.offset || eq->tail.s.offset != eq->written_tail.s.offset) { // this EQ is busy
	return PTL_ARG_INVALID;
    }
    tmp = eq->ring;
    eq->ring = NULL;
    free(tmp);
    return PTL_OK;
}

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
    ptl_internal_eq_t * const eq = &(eqs[eqh.s.ni][eqh.s.code]);
    const uint32_t mask = eq->size - 1;
    eq_off_t readidx, curidx, newidx;

    curidx = eq->head;
    do {
	readidx = curidx;
	if (readidx.s.offset == eq->tail.s.offset) {
	    return PTL_EQ_EMPTY;
	}
	*event = eq->ring[readidx.s.offset];
	newidx.s.sequence = readidx.s.sequence + 23; // a prime number
	newidx.s.offset = (readidx.s.offset+1)&mask;
    } while ((curidx.u = PtlInternalAtomicCas32(&eq->head.u, readidx.u, newidx.u)) != readidx.u);
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
    eq_off_t readidx, curidx, newidx;

    curidx = eq->head;
    do {
	readidx = curidx;
	if (readidx.s.offset == eq->tail.s.offset) {
	    curidx = eq->head;
	    continue;
	}
	*event = eq->ring[readidx.s.offset];
	newidx.s.sequence = readidx.s.sequence + 23; // a prime number
	newidx.s.offset = (readidx.s.offset+1)&mask;
    } while ((curidx.u = PtlInternalAtomicCas32(&eq->head.u, readidx.u, newidx.u)) != readidx.u);
    return PTL_OK;
}

int API_FUNC PtlEQPoll(
    ptl_handle_eq_t * eq_handles,
    int size,
    ptl_time_t timeout,
    ptl_event_t * event,
    int *which)
{
    return PTL_FAIL;
}

void INTERNAL PtlInternalEQPush(
    ptl_handle_eq_t eq_handle,
    ptl_event_t * event)
{
}
