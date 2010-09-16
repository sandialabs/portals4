/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <stdlib.h>		       /* for calloc() */
#include <string.h>		       /* for memcpy() */
#if defined(HAVE_MALLOC_H)
# include <malloc.h>		       /* for memalign() */
#endif

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_error.h"
#include "ptl_internal_timer.h"

const ptl_handle_ct_t PTL_CT_NONE = 0x5fffffff;	/* (2<<29) & 0x1fffffff */

#define CT_FREE	    0
#define CT_BUSY	    1
#define CT_READY    2

volatile uint64_t global_generation = 0;

static ptl_ct_event_t *ct_events[4] = { NULL, NULL, NULL, NULL };
static uint64_t *ct_event_refcounts[4] = { NULL, NULL, NULL, NULL };
static const ptl_ct_event_t CTERR =
    { 0xffffffffffffffffULL, 0xffffffffffffffffULL };

#define CT_NOT_EQUAL(a,b)   (a.success != b.success || a.failure != b.failure)
#define CT_EQUAL(a,b)	    (a.success == b.success && a.failure == b.failure)

/* 128-bit Atomics */
static inline int PtlInternalAtomicCasCT(
    volatile ptl_ct_event_t * addr,
    const ptl_ct_event_t oldval,
    const ptl_ct_event_t newval)
{
#ifdef HAVE_CMPXCHG16B
    register unsigned char ret;
    assert(((uintptr_t) addr & 0xf) == 0);
    __asm__ __volatile__(
    "lock cmpxchg16b %1\n\t" "sete	     %0":"=q"(ret),
    "+m"    (*addr)
    :"a"    (oldval.success),
    "d"     (oldval.failure),
    "b"     (newval.success),
    "c"     (newval.failure)
    :"cc",
    "memory");
    return ret;
#else
#error No known 128-bit atomic CAS operations are available
#endif
}

static inline void PtlInternalAtomicReadCT(
    ptl_ct_event_t * dest,
    volatile ptl_ct_event_t * src)
{
#if defined(HAVE_READ128_INTRINSIC) && 0	/* potentially (and probably) not atomic */
    *dest = __m128i_mm_load_si128(src);
#elif defined(HAVE_MOVDQA) && 0	       /* not actually atomic */
    __asm__ __volatile__(
    "movdqa (%1), %%xmm0\n\t" "movdqa %%xmm0, (%0)":"=r"(dest)
    :"r"    (src)
    :"xmm0");
#elif defined(HAVE_CMPXCHG16B)
    __asm__ __volatile__(
    "xor %%rax, %%rax\n\t"	// zeroing these out to avoid affecting *addr
    "xor %%rbx, %%rbx\n\t" "xor %%rcx, %%rcx\n\t" "xor %%rdx, %%rdx\n\t"
    "lock cmpxchg16b (%2)\n\t" "mov %%rax, %0\n\t"
    "mov %%rdx, %1\n\t":"=m"(dest->success),
    "=m"    (dest->failure)
    :"r"    (src)
    :"cc",
    "rax",
    "rbx",
    "rcx",
    "rdx");
#else
#error No known 128-bit atomic read operations are available
#endif
}

static inline void PtlInternalAtomicWriteCT(
    volatile ptl_ct_event_t * addr,
    const ptl_ct_event_t newval)
{
#ifdef HAVE_CMPXCHG16B
    __asm__ __volatile__(
    "1:\n\t" "lock cmpxchg16b %0\n\t" "jne 1b":"+m"(*addr)
    :"a"    (addr->success),
    "d"     (addr->failure),
    "b"     (newval.success),
    "c"     (newval.failure)
    :"cc",
    "memory");
#else
#error No known 128-bit atomic write operations are available
#endif
}


void INTERNAL PtlInternalCTNISetup(
    unsigned int ni,
    ptl_size_t limit)
{
    ptl_ct_event_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(ct_events[ni]), NULL,
				    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
#if defined(HAVE_MEMALIGN)
	tmp = memalign(16, limit * sizeof(ptl_ct_event_t));
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_ct_event_t));
#elif defined(HAVE_POSIX_MEMALIGN)
	assert(posix_memalign
	       ((void **)&tmp, 16, limit * sizeof(ptl_ct_event_t)) == 0);
	memset(tmp, 0, limit * sizeof(ptl_ct_event_t));
#elif defined(HAVE_16ALIGNED_CALLOC)
	tmp = calloc(limit, sizeof(ptl_ct_event_t));
	assert(tmp != NULL);
#elif defined(HAVE_16ALIGNED_MALLOC)
	tmp = malloc(limit * sizeof(ptl_ct_event_t));
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_ct_event_t));
#else
	tmp = valloc(limit * sizeof(ptl_ct_event_t));	/* cross your fingers */
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_ct_event_t));
#endif
	assert((((intptr_t) tmp) & 0x7) == 0);
	assert(ct_event_refcounts[ni] == NULL);
	ct_event_refcounts[ni] = calloc(limit, sizeof(uint64_t));
	assert(ct_event_refcounts[ni] != NULL);
	ct_events[ni] = tmp;
    }
}

void INTERNAL PtlInternalCTNITeardown(
    int ni)
{
    ptl_ct_event_t *restrict tmp;
    volatile uint64_t *restrict rc;
    while (ct_events[ni] == (void *)1) ;	// in case its in the middle of being allocated (this should never happen in sane code)
    tmp = PtlInternalAtomicSwapPtr((void *volatile *)&ct_events[ni], NULL);
    rc = PtlInternalAtomicSwapPtr((void *volatile *)&ct_event_refcounts[ni],
				  NULL);
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    assert(rc != NULL);
    for (size_t i = 0; i < nit_limits.max_cts; ++i) {
	if (rc[i] != 0) {
	    PtlInternalAtomicWriteCT(&(tmp[i]), CTERR);
	    PtlInternalAtomicInc(&(rc[i]), -1);
	}
    }
    for (size_t i = 0; i < nit_limits.max_cts; ++i) {
	while (rc[i] != 0) ;
    }
    free(tmp);
    free((void *)rc);
}

int INTERNAL PtlInternalCTHandleValidator(
    ptl_handle_ct_t handle,
    int none_ok)
{
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ct = { handle };
    if (ct.s.selector != HANDLE_CT_CODE) {
	VERBOSE_ERROR("Expected CT handle, but it's not a CT handle\n");
	return PTL_ARG_INVALID;
    }
    if (none_ok == 1 && handle == PTL_CT_NONE) {
	return PTL_OK;
    }
    if (ct.s.ni > 3 || ct.s.code > nit_limits.max_cts ||
	(nit.refcount[ct.s.ni] == 0)) {
	VERBOSE_ERROR
	    ("CT NI too large (%u > 3) or code is wrong (%u > %u) or nit table is uninitialized\n",
	     ct.s.ni, ct.s.code, nit_limits.max_cts);
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
#endif
    return PTL_OK;
}

int API_FUNC PtlCTAlloc(
    ptl_handle_ni_t ni_handle,
    ptl_handle_ct_t * ct_handle)
{
    ptl_ct_event_t *cts;
    ptl_size_t offset;
    volatile uint64_t *rc;
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_handle_encoding_t ct = {.selector = HANDLE_CT_CODE,
	.ni = 0,
	.code = 0
    };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalNIValidator(ni)) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ni.s.ni] == NULL) {
	return PTL_ARG_INVALID;
    }
    if (ct_handle == NULL) {
	return PTL_ARG_INVALID;
    }
#endif
    ct.ni = ni.s.ni;
    cts = ct_events[ni.s.ni];
    rc = ct_event_refcounts[ni.s.ni];
    for (offset = 0; offset < nit_limits.max_cts; ++offset) {
	if (rc[offset] == 0) {
	    if (PtlInternalAtomicCas64(&(rc[offset]), 0, 1) == 0) {
		cts[offset].success = 0;
		cts[offset].failure = 0;
		ct.code = offset;
		break;
	    }
	}
    }
    if (offset >= nit_limits.max_cts) {
	*ct_handle = PTL_INVALID_HANDLE.ct;
	return PTL_NO_SPACE;
    } else {
	memcpy(ct_handle, &ct, sizeof(ptl_handle_ct_t));
	return PTL_OK;
    }
}

int API_FUNC PtlCTFree(
    ptl_handle_ct_t ct_handle)
{
    const ptl_internal_handle_converter_t ct = { ct_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    PtlInternalAtomicWriteCT(&(ct_events[ct.s.ni][ct.s.code]), CTERR);
    PtlInternalAtomicInc(&(ct_event_refcounts[ct.s.ni][ct.s.code]), -1);
    while (ct_event_refcounts[ct.s.ni][ct.s.code] != 0) ;
    return PTL_OK;
}

int API_FUNC PtlCTGet(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t * event)
{
    const ptl_internal_handle_converter_t ct = { ct_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    *event = ct_events[ct.s.ni][ct.s.code];
    return PTL_OK;
}

int API_FUNC PtlCTWait(
    ptl_handle_ct_t ct_handle,
    ptl_size_t test,
    ptl_ct_event_t * event)
{
    const ptl_internal_handle_converter_t ct = { ct_handle };
    ptl_ct_event_t *cte;
    volatile uint64_t *rc;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    cte = &(ct_events[ct.s.ni][ct.s.code]);
    rc = &(ct_event_refcounts[ct.s.ni][ct.s.code]);
    //printf("waiting for CT(%llu) sum to reach %llu\n", (unsigned long
    //long)ct.i, (unsigned long long)test);
    PtlInternalAtomicInc(rc, 1);
    do {
	ptl_ct_event_t tmpread;
	PtlInternalAtomicReadCT(&tmpread, cte);
	if (__builtin_expect(CT_EQUAL(tmpread, CTERR), 0)) {
	    PtlInternalAtomicInc(rc, -1);
	    return PTL_INTERRUPTED;
	} else if ((tmpread.success + tmpread.failure) >= test) {
	    if (event != NULL)
		*event = tmpread;
	    PtlInternalAtomicInc(rc, -1);
	    return PTL_OK;
	}
    } while (1);
}

int API_FUNC PtlCTPoll(
    ptl_handle_ct_t * ct_handles,
    ptl_size_t * tests,
    int size,
    ptl_time_t timeout,
    ptl_ct_event_t * event,
    int *which)
{
    ptl_size_t ctidx, offset;
    ptl_ct_event_t *ctes[size];
    volatile uint64_t *rcs[size];
    size_t nstart;
    TIMER_TYPE tp;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ct_handles == NULL || tests == NULL || size == 0) {
	return PTL_ARG_INVALID;
    }
    for (ctidx = 0; ctidx < size; ++ctidx) {
	if (PtlInternalCTHandleValidator(ct_handles[ctidx], 0)) {
	    return PTL_ARG_INVALID;
	}
    }
#endif
    for (ctidx = 0; ctidx < size; ++ctidx) {
	const ptl_internal_handle_converter_t ct = { ct_handles[ctidx] };
	ctes[ctidx] = &(ct_events[ct.s.ni][ct.s.code]);
	rcs[ctidx] =
	    &(ct_event_refcounts[ct.s.ni][ct.s.code]);
	PtlInternalAtomicInc(rcs[ctidx], 1);
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
	uint32_t t = size - 1;
	t |= t >> 1;
	t |= t >> 2;
	t |= t >> 4;
	t |= t >> 8;
	t |= t >> 16;
	offset = nstart & t; // pseudo-random
    }
    do {
	for (ctidx = 0; ctidx < size; ++ctidx) {
	    const ptl_size_t ridx = (ctidx + offset) % size;
	    ptl_ct_event_t tmpread;
	    PtlInternalAtomicReadCT(&tmpread, ctes[ridx]);
	    if (__builtin_expect(CT_EQUAL(tmpread, CTERR), 0)) {
		for (size_t idx = 0; idx < size; ++idx)
		    PtlInternalAtomicInc(rcs[idx], -1);
		return PTL_INTERRUPTED;
	    } else if ((tmpread.success + tmpread.failure) >= tests[ridx]) {
		if (event != NULL)
		    *event = tmpread;
		if (which != NULL)
		    *which = (int)ridx;
		for (size_t idx = 0; idx < size; ++idx)
		    PtlInternalAtomicInc(rcs[idx], -1);
		return PTL_OK;
	    }
	}
	MARK_TIMER(tp);
    } while (timeout == PTL_TIME_FOREVER ||
	     (TIMER_INTS(tp) - nstart) < timeout);
    return PTL_CT_NONE_REACHED;
}

int API_FUNC PtlCTSet(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t test)
{
    const ptl_internal_handle_converter_t ct = { ct_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    PtlInternalAtomicWriteCT(&(ct_events[ct.s.ni][ct.s.code]), test);
    return PTL_OK;
}

int API_FUNC PtlCTInc(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t increment)
{
    const ptl_internal_handle_converter_t ct = { ct_handle };
    ptl_ct_event_t *cte;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    cte = &(ct_events[ct.s.ni][ct.s.code]);
    if (increment.failure == 0) {
	/* cheaper than a 128-bit atomic increment */
	PtlInternalAtomicInc(&(cte->success), increment.success);
    } else if (increment.success == 0) {
	/* cheaper than a 128-bit atomic increment */
	PtlInternalAtomicInc(&(cte->failure), increment.failure);
    } else {
	/* expensive increment */
	ptl_ct_event_t old, tmp;
	do {
	    old = tmp = *cte;
	    tmp.success += increment.success;
	    tmp.failure += increment.failure;
	} while (PtlInternalAtomicCasCT(cte, old, tmp));
    }
    return PTL_OK;
}
