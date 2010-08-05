/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <stdlib.h>		       /* for malloc() */
#include <string.h>		       /* for memset() */
#if defined(HAVE_MALLOC_H)
# include <malloc.h>		       /* for memalign() */
#endif

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
    volatile uint32_t status;	// 0=free, 1=allocated, 2=in-use
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

static void *LEprocessor(
    void * __attribute__ ((unused)) junk)
{
    while (1) {
	ptl_internal_appendLE_t *append_me;
	ptl_table_entry_t *t;
	ptl_internal_le_t *entries;
#warning LEprocessor needs a better poll/awaken implementation
	while ((append_me = PtlInternalQueuePop(&appends)) == NULL) {
#if defined(HAVE_PTHREAD_YIELD)
	    pthread_yield();
#elif defined(HAVE_SCHED_YIELD)
	    sched_yield();
#endif
	}
	assert(append_me);
	t = &(nit.tables[append_me->ni_handle.s.ni][append_me->pt_index]);
	/* check memory allocation */
	if (t->priority == NULL || t->overflow == NULL) {
	    /* this is done with mutexes to avoid cache-flushing
	     * atomics/volatiles in the common case */
	    assert(pthread_mutex_lock(&(t->lock)) == 0);
	    if (t->priority == NULL) {
		t->priority =
		    malloc((nit_limits.max_me_list + 1) * sizeof(uint64_t));
		assert(t->priority != NULL);
		assert((((uintptr_t) t->priority) & 0x7) == 0);	// 8-byte alignment
		memset(t->priority, 0,
		       (nit_limits.max_me_list + 1) * sizeof(uint64_t));
	    }
	    if (t->overflow == NULL) {
		t->overflow =
		    malloc((nit_limits.max_me_list +
			    1) * sizeof(ptl_internal_le_t));
		assert(t->overflow != NULL);
		assert((((uintptr_t) t->overflow) & 0x7) == 0);	// 8-byte alignment
		memset(t->overflow, 0,
		       (nit_limits.max_me_list + 1) * sizeof(uint64_t));
	    }
	    assert(pthread_mutex_unlock(&(t->lock)) == 0);
	}
	switch (append_me->ptl_list) {
	    case PTL_PRIORITY_LIST:
		assert(t->priority != NULL);
		{
		    int found = 1;
		    /* first, search the overflow list */
		    found = 0;
#warning LEAppend() does not handle the overflow list, which may be tricky.
		    /* second, if (not_found || !(append_me->le.options & PTL_LE_USE_ONCE)), append to priority list */
		    if (!found || !(append_me->le.options & PTL_LE_USE_ONCE)) {
			entries = (ptl_internal_le_t *) t->priority;
			for (size_t i = 0; i < nit_limits.max_me_list; ++i) {
			    if (PtlInternalAtomicCas32
				(&(entries[i].status), 0, 2) == 0) {
				entries[i].visible = append_me->le;
				__sync_synchronize();
				entries[i].status = 1;
				break;
			    }
			}
		    }
		    free(append_me);
		}
		break;
	    case PTL_OVERFLOW:
		assert(t->overflow != NULL);
		entries = (ptl_internal_le_t *) t->overflow;
		for (size_t i = 0; i < nit_limits.max_me_list; ++i) {
		    if (PtlInternalAtomicCas32(&(entries[i].status), 0, 2) ==
			0) {
			entries[i].visible = append_me->le;
			__sync_synchronize();
			entries[i].status = 1;
			break;
		    }
		}
		free(append_me);
		break;
	    case PTL_PROBE_ONLY:
		abort();
# warning PTL_PROBE_ONLY is not implemented
		break;
	}
    }
}

void INTERNAL PtlInternalLENISetup(
    ptl_size_t limit)
{
    ptl_internal_le_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(les), NULL, (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
#if defined(HAVE_MEMALIGN)
	tmp = memalign(8, limit * sizeof(ptl_internal_le_t));
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_le_t));
#elif defined(HAVE_POSIX_MEMALIGN)
	assert(posix_memalign
	       ((void **)&tmp, 8, limit * sizeof(ptl_internal_le_t)) == 0);
	memset(tmp, 0, limit * sizeof(ptl_internal_le_t));
#elif defined(HAVE_8ALIGNED_CALLOC)
	tmp = calloc(limit, sizeof(ptl_internal_le_t));
	assert(tmp != NULL);
#elif defined(HAVE_8ALIGNED_MALLOC)
	tmp = malloc(limit * sizeof(ptl_internal_le_t));
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_le_t));
#else
	tmp = valloc(limit * sizeof(ptl_internal_le_t));	/* cross your fingers */
	assert(tmp != NULL);
	memset(tmp, 0, limit * sizeof(ptl_internal_le_t));
#endif
	assert((((intptr_t) tmp) & 0x7) == 0);
	PtlInternalQueueInit(&appends);
	__sync_synchronize();
	les = tmp;
	assert(pthread_create(&LEthread, NULL, LEprocessor, NULL) == 0);
    }
}

void INTERNAL PtlInternalLENITeardown(
    void)
{
    ptl_internal_le_t *tmp;
    assert(pthread_cancel(LEthread) == 0);
    tmp = les;
    les = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    free(tmp);
    PtlInternalQueueDestroy(&appends);
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
    if (ni.s.ni == 0 || ni.s.ni == 2) {	// must be a non-matching NI
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
	if (les[offset].status == 0) {
	    if (PtlInternalAtomicCas32(&(les[offset].status), 0, 2) == 0) {
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
