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

const ptl_handle_ct_t PTL_CT_NONE = 0x5fffffff;	/* (2<<29) & 0x1fffffff */

#define CT_FREE	    0
#define CT_BUSY	    1
#define CT_READY    2

typedef struct {		/* this is ordered to encourage optimal packing */
    volatile ptl_ct_event_t data;	/* 128-bits (16 bytes) */
    volatile uint64_t generation;	/* (8 bytes) */
    volatile uint32_t enabled;	/* (4 bytes) 0=free, 1=busy, 2=allocated */
    ptl_ct_type_t type;		/* enum... (4 bytes?) */
} ptl_internal_ct_t;

static ptl_internal_ct_t *ct_events[4] = { NULL, NULL, NULL, NULL };
static uint64_t waiters = 0;

#define CT_NOT_EQUAL(a,b)   (a.success != b.success || a.failure != b.failure)
#define CT_EQUAL(a,b)	    (a.success == b.success && a.failure == b.failure)

/* 128-bit Atomics */
static inline ptl_ct_event_t PtlInternalAtomicCasCT(
    volatile ptl_ct_event_t * addr,
    const ptl_ct_event_t oldval,
    const ptl_ct_event_t newval)
{
#ifdef HAVE_CMPXCHG16B
    ptl_ct_event_t ret;
    __asm__ __volatile__(
    "lock cmpxchg16b %2"
    :"=a"(ret.success), "=d" (ret.failure), "=m"(*addr)
    :"a"     (oldval.success),
    "d"     (oldval.failure),
    "b"     (newval.success),
    "c"     (newval.failure)
    :"cc", "memory");
    return ret;
#else
#error No known 128-bit atomic CAS operations are available
#endif
}

static inline void PtlInternalAtomicReadCT(
    ptl_ct_event_t *dest, volatile ptl_ct_event_t * src)
{
#if defined(HAVE_READ128_INTRINSIC) && 0	/* potentially (and probably) not atomic */
    *dest = __m128i_mm_load_si128(src);
#elif defined(HAVE_MOVDQA) && 0	       /* not actually atomic */
    __asm__ __volatile__(
    "movdqa (%1), %%xmm0\n\t"
    "movdqa %%xmm0, (%0)"
    :"=r"   (dest)
    :"r"    (src)
    :"xmm0");
#elif defined(HAVE_CMPXCHG16B)
    __asm__ __volatile__(
    "xor %%rax, %%rax\n\t"	// zeroing these out to avoid affecting *addr
    "xor %%rbx, %%rbx\n\t"
    "xor %%rcx, %%rcx\n\t"
    "xor %%rdx, %%rdx\n\t"
    "lock cmpxchg16b (%2)\n\t"
    "mov %%rax, %0\n\t"
    "mov %%rdx, %1\n\t"
    :"=m"   (dest->success),
    "=m"    (dest->failure)
    :"r"    (src)
    :"cc", "rax", "rbx", "rcx", "rdx");
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
    "1:\n\t"
    "lock cmpxchg16b %0\n\t"
    "jne 1b"
    :"=m" (*addr)
    :"a"     (addr->success),
    "d"     (addr->failure),
    "b"     (newval.success),
    "c"     (newval.failure)
    :"cc", "memory");
#else
#error No known 128-bit atomic write operations are available
#endif
}


void INTERNAL PtlInternalCTNISetup(
    unsigned int ni,
    ptl_size_t limit)
{
    ptl_internal_ct_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(ct_events[ni]), NULL,
				    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
#if defined(HAVE_MEMALIGN)
	tmp = memalign(16, limit * sizeof(ptl_internal_ct_t));
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_ct_t));
#elif defined(HAVE_POSIX_MEMALIGN)
	assert(posix_memalign
	       ((void **)&tmp, 16, limit * sizeof(ptl_internal_ct_t)) == 0);
	memset(tmp, 0, limit * sizeof(ptl_internal_ct_t));
#elif defined(HAVE_16ALIGNED_CALLOC)
	tmp = calloc(limit, sizeof(ptl_internal_ct_t));
	assert(tmp != NULL);
#elif defined(HAVE_16ALIGNED_MALLOC)
	tmp = malloc(limit * sizeof(ptl_internal_ct_t));
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_ct_t));
#else
	tmp = valloc(limit * sizeof(ptl_internal_ct_t));	/* cross your fingers */
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_ct_t));
#endif
	assert((((intptr_t) tmp) & 0x7) == 0);
	ct_events[ni] = tmp;
    }
}

void INTERNAL PtlInternalCTNITeardown(
    int ni)
{
    ptl_internal_ct_t *tmp = ct_events[ni];
    ct_events[ni] = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    for (size_t i = 0; i < nit_limits.max_cts; ++i) {
	tmp[i].data.success = 0xffffffffffffffffULL;
	tmp[i].data.failure = 0xffffffffffffffffULL;
	tmp[i].generation = 0xffffffffffffffffULL;
	tmp[i].enabled = CT_FREE;
    }
    while(waiters > 0) ;
    free(tmp);
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
    if (ct.s.ni > 3 || ct.s.code > nit_limits.max_cts || (nit.refcount[ct.s.ni] == 0)) {
	VERBOSE_ERROR("CT NI too large (%u > 3) or code is wrong (%u > %u) or nit table is uninitialized\n", ct.s.ni, ct.s.code, nit_limits.max_cts);
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni] == NULL) {
	VERBOSE_ERROR("CT table for NI uninitialized\n");
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni][ct.s.code].enabled != CT_READY) {
	VERBOSE_ERROR("CT appears to be deallocated\n");
	return PTL_ARG_INVALID;
    }
#endif
    return PTL_OK;
}

int API_FUNC PtlCTAlloc(
    ptl_handle_ni_t ni_handle,
    ptl_ct_type_t ct_type,
    ptl_handle_ct_t * ct_handle)
{
    ptl_internal_ct_t *cts;
    ptl_size_t offset;
    ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_handle_encoding_t ct = {.selector = HANDLE_CT_CODE,.ni = 0,.code =
	    0 };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ni.s.selector != HANDLE_NI_CODE) {
	return PTL_ARG_INVALID;
    }
    if (ni.s.ni > 3 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
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
    for (offset = 0; offset < nit_limits.max_cts; ++offset) {
	if (cts[offset].enabled == CT_FREE) {
	    if (PtlInternalAtomicCas32
		(&(cts[offset].enabled), CT_FREE, CT_BUSY) == CT_FREE) {
		cts[offset].type = ct_type;
		cts[offset].data.success = 0;
		cts[offset].data.failure = 0;
		__sync_synchronize();
		cts[offset].enabled = CT_READY;
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
    ++ct_events[ct.s.ni][ct.s.code].generation;
    ct_events[ct.s.ni][ct.s.code].enabled = CT_FREE;
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
    *event = ct_events[ct.s.ni][ct.s.code].data;
    return PTL_OK;
}

int API_FUNC PtlCTWait(
    ptl_handle_ct_t ct_handle,
    ptl_size_t test,
    ptl_ct_event_t *event)
{
    const ptl_internal_handle_converter_t ct = { ct_handle };
    ptl_internal_ct_t *cte;
    uint64_t my_generation;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    cte = &(ct_events[ct.s.ni][ct.s.code]);
    my_generation = cte->generation;
    //printf("waiting for CT(%llu) sum to reach %llu\n", (unsigned long long)ct.i, (unsigned long long)test);
    /* I wish this loop were tighter, but because CT's can be
     * destroyed/reallocated unexpectedly, it can't be */
    while (cte->generation == my_generation) {
	ptl_ct_event_t tmpread;
	PtlInternalAtomicReadCT(&tmpread, &(cte->data));
	if ((tmpread.success + tmpread.failure) >= test) {
	    if (cte->generation == my_generation) {
		assert(cte->enabled == CT_READY);
		if (event != NULL) *event = tmpread;
		return PTL_OK;
	    } else {
		return PTL_FAIL;
	    }
	}
    }
    return PTL_FAIL;
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
    PtlInternalAtomicWriteCT(&(ct_events[ct.s.ni][ct.s.code].data), test);
    return PTL_FAIL;
}

int API_FUNC PtlCTInc(
    ptl_handle_ct_t ct_handle,
    ptl_ct_event_t increment)
{
    const ptl_internal_handle_converter_t ct = { ct_handle };
    ptl_internal_ct_t *cte;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalCTHandleValidator(ct_handle, 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    cte = &(ct_events[ct.s.ni][ct.s.code]);
    if (increment.success == 0 && increment.failure != 0) {
	/* cheaper than a 128-bit atomic increment */
	PtlInternalAtomicInc(&(cte->data.failure), increment.failure);
    } else if (increment.success != 0 && increment.failure == 0) {
	/* cheaper than a 128-bit atomic increment */
	PtlInternalAtomicInc(&(cte->data.success), increment.success);
    } else if (increment.success != 0 && increment.failure != 0) {
	/* expensive increment */
	ptl_ct_event_t old, tmp;
	PtlInternalAtomicReadCT(&tmp, &(cte->data));
	do {
	    old = tmp;
	    tmp.success += increment.success;
	    tmp.failure += increment.failure;
	    tmp = PtlInternalAtomicCasCT(&(cte->data), old, tmp);
	} while (CT_NOT_EQUAL(tmp, old));
    }
    return PTL_OK;
}
