/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <stdlib.h>		       /* for malloc() */
#include <string.h>		       /* for memset() */

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_LE.h"
#include "ptl_internal_queues.h"

typedef struct {
    ptl_le_t visible;
    volatile uint32_t in_use;
} ptl_internal_le_t;

typedef struct {
    ptl_internal_handle_converter_t ni_handle;
    ptl_pt_index_t pt_index;
    ptl_le_t le;
    ptl_list_t ptl_list;
    void *user_ptr;
    ptl_internal_handle_converter_t le_handle;
    void *next;
} ptl_internal_appendLE_t;

static ptl_internal_le_t *les = NULL;
static ptl_internal_q_t appends;
static pthread_t LEthread;

static void *LEprocessor(void*junk)
{
    while (1) {
	ptl_internal_appendLE_t *tmp = PtlInternalQueuePop(&appends);
	if (tmp == NULL) {
#ifdef HAVE_PTHREAD_YIELD
	    pthread_yield();
#elif HAVE_SCHED_YIELD
	    sched_yield();
#endif
	    continue;
	}
	assert(tmp);
	switch(tmp->ptl_list) {
	    case PTL_PRIORITY_LIST:
		break;
	    case PTL_OVERFLOW:
		break;
	    case PTL_PROBE_ONLY:
		break;
	}
    }
    return NULL;
}

void INTERNAL PtlInternalLENISetup(
    unsigned int ni,
    ptl_size_t limit)
{
    ptl_internal_le_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(les), NULL, (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
#if defined(HAVE_MEMALIGN)
	tmp = memalign(8, nit_limits.max_mes * sizeof(ptl_internal_le_t));
	assert(tmp != NULL);
	memset(tmp, 0, nit_limits.max_mes * sizeof(ptl_internal_le_t));
#elif defined(HAVE_POSIX_MEMALIGN)
	assert(posix_memalign
	       ((void **)&tmp, 8,
		nit_limits.max_mes * sizeof(ptl_internal_le_t)) == 0);
	memset(tmp, 0, nit_limits.max_mes * sizeof(ptl_internal_le_t));
#elif defined(HAVE_8ALIGNED_CALLOC)
	tmp = calloc(nit_limits.max_mes, sizeof(ptl_internal_le_t));
	assert(tmp != NULL);
#elif defined(HAVE_8ALIGNED_MALLOC)
	tmp = malloc(nit_limits.max_mes * sizeof(ptl_internal_le_t));
	assert(tmp != NULL);
	memset(tmp, 0, nit_limits.max_mes * sizeof(ptl_internal_le_t));
#else
	tmp = valloc(nit_limits.max_mes * sizeof(ptl_internal_le_t));	/* cross your fingers */
	assert(tmp != NULL);
	memset(tmp, 0, nit_limits.max_mes * sizeof(ptl_internal_le_t));
#endif
	assert((((intptr_t) tmp) & 0x7) == 0);
	les = tmp;
	PtlInternalQueueInit(&appends);
	assert(pthread_create(&LEthread, NULL, LEprocessor, NULL) == 0);
    }
}

void INTERNAL PtlInternalLENITeardown(
    unsigned int ni)
{
    ptl_internal_le_t *tmp = les;
    les = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    free(tmp);
}

int API_FUNC PtlLEAppend(
    ptl_handle_ni_t ni_handle,
    ptl_pt_index_t pt_index,
    ptl_le_t le,
    ptl_list_t ptl_list,
    void *user_ptr,
    ptl_handle_le_t * le_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_handle_encoding_t leh = { HANDLE_LE_CODE, 0, 0 };
    ptl_internal_appendLE_t *Qentry;
    uint32_t offset;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ni.s.ni >= 4 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits.max_pt_index) {
	return PTL_ARG_INVALID;
    }
#endif
    assert(les != NULL);
    leh.ni = ni.s.ni;
    Qentry = malloc(sizeof(ptl_internal_appendLE_t));
    if (Qentry == NULL) {
	return PTL_NO_SPACE;
    }
    assert(Qentry != NULL);
    Qentry->ni_handle.a.ni = ni_handle;
    Qentry->pt_index = pt_index;
    Qentry->le = le;
    Qentry->ptl_list = ptl_list;
    Qentry->user_ptr = user_ptr;
    /* find an LE handle */
    for (offset = 0; offset < nit_limits.max_mes; ++offset) {
	if (les[offset].in_use == 0) {
	    if (PtlInternalAtomicCas32(&(les[offset].in_use), 0, 1) == 0) {
		leh.code = offset;
		break;
	    }
	}
    }
    Qentry->le_handle.s = leh;
    memcpy(le_handle, &leh, sizeof(ptl_handle_le_t));
    /* append Qentry to the appends queue */
    PtlInternalQueueAppend(&appends, Qentry);
    return PTL_OK;
}

int API_FUNC PtlLEUnlink(
    ptl_handle_le_t le_handle)
{
    return PTL_FAIL;
}
