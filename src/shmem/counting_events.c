/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <stdlib.h>		       /* for calloc() */
#include <string.h>		       /* for memcpy() */

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_CT.h"

const ptl_handle_ct_t PTL_CT_NONE = 0x5fffffff;	/* (2<<29) & 0x1fffffff */

typedef struct {
    ptl_ct_type_t type;
    volatile ptl_ct_event_t data;
    volatile uint32_t enabled;
} ptl_internal_ct_t;

static ptl_internal_ct_t *ct_events[4];

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
	tmp = memalign(8, nit_limits.max_cts * sizeof(ptl_internal_ct_t));
	assert(tmp != NULL);
	memset(tmp, 0, nit_limits.max_cts * sizeof(ptl_internal_ct_t));
#elif defined(HAVE_POSIX_MEMALIGN)
	assert(posix_memalign((void**)&tmp, 8, nit_limits.max_cts * sizeof(ptl_internal_ct_t)) == 0);
	memset(tmp, 0, nit_limits.max_cts * sizeof(ptl_internal_ct_t));
#elif defined(HAVE_8ALIGNED_CALLOC)
	tmp = calloc(nit_limits.max_cts, sizeof(ptl_internal_ct_t));
	assert(tmp != NULL);
#elif defined(HAVE_8ALIGNED_MALLOC)
	tmp = malloc(nit_limits.max_cts * sizeof(ptl_internal_ct_t));
	assert(tmp != NULL);
	memset(tmp, 0, nit_limits.max_cts * sizeof(ptl_internal_ct_t));
#else
	tmp = valloc(nit_limits.max_cts * sizeof(ptl_internal_ct_t)); /* cross your fingers */
	assert(tmp != NULL);
	memset(tmp, 0, nit_limits.max_cts * sizeof(ptl_internal_ct_t));
#endif
	assert((((intptr_t)tmp) & 0x7) == 0);
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
    free(tmp);
}

int API_FUNC PtlCTAlloc(
    ptl_handle_ni_t ni_handle,
    ptl_ct_type_t ct_type,
    ptl_handle_ct_t * ct_handle)
{
    ptl_internal_ct_t *cts;
    ptl_size_t offset;
    ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_handle_encoding_t ct = { HANDLE_CT_CODE, 0, 0 };
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
	if (PtlInternalAtomicCas32(&(cts[offset].enabled), 0, 1) == 0) {
	    ct.code = offset;
	    break;
	}
    }
    if (offset >= nit_limits.max_cts) {
	memcpy(ct_handle, &PTL_CT_NONE, sizeof(ptl_handle_ct_t));
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
    if (ct.s.selector != HANDLE_CT_CODE) {
	return PTL_ARG_INVALID;
    }
    if (ct.s.ni > 3 || ct.s.code != 0 || (nit.refcount[ct.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni] == NULL) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni][ct.s.code].enabled == 0) {
	return PTL_ARG_INVALID;
    }
#endif
    ct_events[ct.s.ni][ct.s.code].enabled = 0;
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
    if (ct.s.selector != HANDLE_CT_CODE) {
	return PTL_ARG_INVALID;
    }
    if (ct.s.ni > 3 || ct.s.code != 0 || (nit.refcount[ct.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni] == NULL) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni][ct.s.code].enabled == 0) {
	return PTL_ARG_INVALID;
    }
#endif
    *event = ct_events[ct.s.ni][ct.s.code].data;
    return PTL_FAIL;
}

int API_FUNC PtlCTWait(
    ptl_handle_ct_t ct_handle,
    ptl_size_t test)
{
    const ptl_internal_handle_converter_t ct = { ct_handle };
    ptl_internal_ct_t *cte;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ct.s.selector != HANDLE_CT_CODE) {
	return PTL_ARG_INVALID;
    }
    if (ct.s.ni > 3 || ct.s.code != 0 || (nit.refcount[ct.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni] == NULL) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni][ct.s.code].enabled == 0) {
	return PTL_ARG_INVALID;
    }
#endif
    cte = &(ct_events[ct.s.ni][ct.s.code]);
    while (cte->enabled == 1) {
	if ((cte->data.success + cte->data.failure) >= test)
	    return PTL_OK;
    }
#warning This case is not specified in the spec, but needs to be figured out by the Portals4 Committee
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
    if (ct.s.selector != HANDLE_CT_CODE) {
	return PTL_ARG_INVALID;
    }
    if (ct.s.ni > 3 || ct.s.code != 0 || (nit.refcount[ct.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni] == NULL) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni][ct.s.code].enabled == 0) {
	return PTL_ARG_INVALID;
    }
#endif
    ct_events[ct.s.ni][ct.s.code].data = test;
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
    if (ct.s.selector != HANDLE_CT_CODE) {
	return PTL_ARG_INVALID;
    }
    if (ct.s.ni > 3 || ct.s.code != 0 || (nit.refcount[ct.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni] == NULL) {
	return PTL_ARG_INVALID;
    }
    if (ct_events[ct.s.ni][ct.s.code].enabled == 0) {
	return PTL_ARG_INVALID;
    }
#endif
    cte = &(ct_events[ct.s.ni][ct.s.code]);
#warning This ordering is technically not in the spec, but probably should be
    if (increment.success != 0) {
	PtlInternalAtomicInc(&(cte->data.success), increment.success);
    }
    if (increment.failure != 0) {
	PtlInternalAtomicInc(&(cte->data.failure), increment.failure);
    }
    return PTL_FAIL;
}
