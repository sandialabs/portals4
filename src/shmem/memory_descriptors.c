/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <limits.h>		       /* for UINT_MAX */
#include <stdlib.h>		       /* for calloc() */
#include <string.h>		       /* for memset() */
#if defined(HAVE_MALLOC_H)
# include <malloc.h>		       /* for memalign() */
#endif

#include <stdio.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_MD.h"

const ptl_handle_any_t PTL_INVALID_HANDLE = { UINT_MAX };

#define MD_FREE	    0
#define MD_IN_USE   1

typedef struct {
    ptl_md_t visible;
    uint32_t in_use;		// 0=free, 1=in_use
} ptl_internal_md_t;

static ptl_internal_md_t *mds[4] = { NULL, NULL, NULL, NULL };

void INTERNAL PtlInternalMDNISetup(unsigned int ni, ptl_size_t limit)
{
    ptl_internal_md_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(mds[ni]), NULL,
				    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
#if defined(HAVE_MEMALIGN)
	tmp = memalign(8, limit * sizeof(ptl_internal_md_t));
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_md_t));
#elif defined(HAVE_POSIX_MEMALIGN)
	assert(posix_memalign
	       ((void **)&tmp, 8, limit * sizeof(ptl_internal_md_t)) == 0);
	memset(tmp, 0, limit * sizeof(ptl_internal_md_t));
#elif defined(HAVE_8ALIGNED_CALLOC)
	tmp = calloc(limit, sizeof(ptl_internal_md_t));
	assert(tmp != NULL);
#elif defined(HAVE_8ALIGNED_MALLOC)
	tmp = malloc(limit * sizeof(ptl_internal_md_t));
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_md_t));
#else
	tmp = valloc(limit * sizeof(ptl_internal_md_t));	/* cross your fingers */
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_md_t));
#endif
	assert((((intptr_t) tmp) & 0x7) == 0);
	mds[ni] = tmp;
    }
}

void INTERNAL PtlInternalMDNITeardown(unsigned int ni)
{
    ptl_internal_md_t *tmp = mds[ni];
    mds[ni] = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    free(tmp);
}

int INTERNAL PtlInternalMDHandleValidator(ptl_handle_md_t handle)
{
    const ptl_internal_handle_converter_t md = { handle };
    if (md.s.selector != HANDLE_MD_CODE) {
	printf("selector not a MD selector (%i)\n", md.s.selector);
	return PTL_ARG_INVALID;
    }
    if (md.s.ni > 3 || md.s.code != 0 || (nit.refcount[md.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (mds[md.s.ni] == NULL) {
	return PTL_ARG_INVALID;
    }
    if (mds[md.s.ni][md.s.code].in_use != MD_IN_USE) {
	return PTL_ARG_INVALID;
    }
    return PTL_OK;
}

int API_FUNC PtlMDBind(
    ptl_handle_ni_t ni_handle,
    ptl_md_t * md,
    ptl_handle_md_t * md_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t mdh;
    size_t offset;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ni.s.ni >= 4 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (md->start == NULL || md->length == 0) {
	return PTL_ARG_INVALID;
    }
    if (PtlInternalCTHandleValidator(md->ct_handle, 1)) {
	return PTL_ARG_INVALID;
    }
    if (PtlInternalEQHandleValidator(md->eq_handle, 1)) {
	return PTL_ARG_INVALID;
    }
#endif
    mdh.s.selector = HANDLE_MD_CODE;
    mdh.s.ni = ni.s.ni;
    for (offset = 0; offset < nit_limits.max_mds; ++offset) {
	if (mds[ni.s.ni][offset].in_use == MD_FREE) {
	    if (PtlInternalAtomicCas32(&(mds[ni.s.ni][offset].in_use), MD_FREE, MD_IN_USE) == MD_FREE) {
		mds[ni.s.ni][offset].visible = *md;
		mdh.s.code = offset;
		break;
	    }
	}
    }
    if (offset >= nit_limits.max_mds) {
	*md_handle = PTL_INVALID_HANDLE.md;
	return PTL_NO_SPACE;
    } else {
	*md_handle = mdh.a.md;
	return PTL_OK;
    }
}

int API_FUNC PtlMDRelease(
    ptl_handle_md_t md_handle)
{
    const ptl_internal_handle_converter_t md = { md_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle)) {
	return PTL_ARG_INVALID;
    }
#endif
    mds[md.s.ni][md.s.code].in_use = MD_FREE;
    return PTL_OK;
}
