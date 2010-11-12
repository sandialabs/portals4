/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <limits.h>                    /* for UINT_MAX */
#include <stdlib.h>                    /* for calloc() */
#include <string.h>                    /* for memset() */
#if defined(HAVE_MALLOC_H)
# include <malloc.h>                   /* for memalign() */
#endif

#include <stdio.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_error.h"

const ptl_handle_any_t PTL_INVALID_HANDLE = { UINT_MAX };

#define MD_FREE     0
#define MD_IN_USE   1

typedef struct {
    ptl_md_t visible;
    volatile uint32_t in_use;   // 0=free, 1=in_use
    uint32_t refcount;
} ptl_internal_md_t;

static ptl_internal_md_t *mds[4] = { NULL, NULL, NULL, NULL };

void INTERNAL PtlInternalMDNISetup(
    unsigned int ni,
    ptl_size_t limit)
{
    ptl_internal_md_t *tmp;
    while ((tmp =
            PtlInternalAtomicCasPtr(&(mds[ni]), NULL,
                                    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
        tmp = calloc(limit, sizeof(ptl_internal_md_t));
        assert(tmp != NULL);
        __sync_synchronize();
        mds[ni] = tmp;
    }
}

void INTERNAL PtlInternalMDNITeardown(
    unsigned int ni)
{
    ptl_internal_md_t *tmp = mds[ni];
    mds[ni] = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    for (size_t mdi = 0; mdi < nit_limits.max_mds; ++mdi) {
        while (tmp[mdi].refcount != 0) ;
    }
    free(tmp);
}

int INTERNAL PtlInternalMDHandleValidator(
    ptl_handle_md_t handle,
    int care_about_ct)
{
    const ptl_internal_handle_converter_t md = { handle };
    ptl_internal_md_t *mdptr;
    if (md.s.selector != HANDLE_MD_CODE) {
        VERBOSE_ERROR("selector not a MD selector (%i)\n", md.s.selector);
        return PTL_ARG_INVALID;
    }
    if (md.s.ni > 3 || md.s.code > nit_limits.max_mds ||
        (nit.refcount[md.s.ni] == 0)) {
        VERBOSE_ERROR
            ("MD Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n",
             md.s.ni, md.s.code, nit_limits.max_mds);
        return PTL_ARG_INVALID;
    }
    if (mds[md.s.ni] == NULL) {
        VERBOSE_ERROR("MD aray for NI uninitialized\n");
        return PTL_ARG_INVALID;
    }
    if (mds[md.s.ni][md.s.code].in_use != MD_IN_USE) {
        VERBOSE_ERROR("MD appears to be free already\n");
        return PTL_ARG_INVALID;
    }
    mdptr = &(mds[md.s.ni][md.s.code]);
    if (PtlInternalEQHandleValidator(mdptr->visible.eq_handle, 1)) {
        VERBOSE_ERROR("MD has a bad EQ handle\n");
        return PTL_ARG_INVALID;
    }
    if (care_about_ct) {
        int ct_optional = 1;
        if (mdptr->visible.
            options & (PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_CT_REPLY |
                       PTL_MD_EVENT_CT_ACK)) {
            ct_optional = 0;
        }
        if (PtlInternalCTHandleValidator
            (mdptr->visible.ct_handle, ct_optional)) {
            VERBOSE_ERROR("MD has a bad CT handle\n");
            return PTL_ARG_INVALID;
        }
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
    int ct_optional = 1;
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (ni.s.ni > 3 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR
            ("ni is bad (%u > 3) or code invalid (%u != 0) or nit not initialized\n",
             ni.s.ni, ni.s.code);
        return PTL_ARG_INVALID;
    }
    /*if (md->start == NULL || md->length == 0) {
     * VERBOSE_ERROR("start is NULL (%p) or length is 0 (%u); cannot detect failures!\n", md->start, (unsigned int)md->length);
     * return PTL_ARG_INVALID;
     * } */
    if (PtlInternalEQHandleValidator(md->eq_handle, 1)) {
        VERBOSE_ERROR("MD saw invalid EQ\n");
        return PTL_ARG_INVALID;
    }
    if (md->options &
        (PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_CT_REPLY | PTL_MD_EVENT_CT_ACK))
    {
        ct_optional = 0;
    }
    if (PtlInternalCTHandleValidator(md->ct_handle, ct_optional)) {
        VERBOSE_ERROR("MD saw invalid CT\n");
        return PTL_ARG_INVALID;
    }
#endif
    mdh.s.selector = HANDLE_MD_CODE;
    mdh.s.ni = ni.s.ni;
    for (offset = 0; offset < nit_limits.max_mds; ++offset) {
        if (mds[ni.s.ni][offset].in_use == MD_FREE) {
            if (PtlInternalAtomicCas32
                (&(mds[ni.s.ni][offset].in_use), MD_FREE,
                 MD_IN_USE) == MD_FREE) {
                mds[ni.s.ni][offset].visible = *md;
                mdh.s.code = offset;
                break;
            }
        }
    }
    if (offset >= nit_limits.max_mds) {
        *md_handle = PTL_INVALID_HANDLE;
        return PTL_NO_SPACE;
    } else {
        *md_handle = mdh.a;
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
    if (PtlInternalMDHandleValidator(md_handle, 0)) {
        VERBOSE_ERROR("MD handle invalid\n");
        return PTL_ARG_INVALID;
    }
#endif
    if (mds[md.s.ni][md.s.code].refcount != 0) {
        VERBOSE_ERROR("%u MD handle in use!\n", (unsigned)proc_number);
        return PTL_IN_USE;
    }
    mds[md.s.ni][md.s.code].in_use = MD_FREE;
    return PTL_OK;
}

char INTERNAL *PtlInternalMDDataPtr(
    ptl_handle_md_t handle)
{
    const ptl_internal_handle_converter_t md = { handle };
    return mds[md.s.ni][md.s.code].visible.start;
}

ptl_size_t INTERNAL PtlInternalMDLength(
    ptl_handle_md_t handle)
{
    const ptl_internal_handle_converter_t md = { handle };
    return mds[md.s.ni][md.s.code].visible.length;
}

ptl_md_t INTERNAL *PtlInternalMDFetch(
    ptl_handle_md_t handle)
{
    const ptl_internal_handle_converter_t md = { handle };
    /* this check allows us to process acks from/to dead NI's */
    if (mds[md.s.ni] != NULL) {
        return &(mds[md.s.ni][md.s.code].visible);
    } else {
        return NULL;
    }
}

void INTERNAL PtlInternalMDPosted(
    ptl_handle_md_t handle)
{
    const ptl_internal_handle_converter_t md = { handle };
    //printf("%u MD %u incremented\n", (unsigned)proc_number, md.s.code);
    PtlInternalAtomicInc(&mds[md.s.ni][md.s.code].refcount, 1);
}

void INTERNAL PtlInternalMDCleared(
    ptl_handle_md_t handle)
{
    const ptl_internal_handle_converter_t md = { handle };
    /* this check allows us to process acks from/to dead NI's */
    //printf("%u MD %u needs decremented\n", (unsigned)proc_number, md.s.code);
    if (mds[md.s.ni] != NULL) {
        //printf("%u MD %u decremented\n", (unsigned)proc_number, md.s.code);
        PtlInternalAtomicInc(&mds[md.s.ni][md.s.code].refcount, -1);
    }
}

/* vim:set expandtab: */
