/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <stdlib.h>		       /* for NULL & calloc() */
#include <string.h>		       /* for memcpy() */

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_ME.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"

#define ME_FREE		0
#define ME_ALLOCATED	1
#define ME_IN_USE	2

typedef struct {
    void *next;			// for nemesis
    void *user_ptr;
    ptl_internal_handle_converter_t me_handle;
} ptl_internal_appendME_t;

typedef struct {
    ptl_me_t visible;
    volatile uint32_t status;	// 0=free, 1=allocated, 2=in-use
    ptl_pt_index_t pt_index;
    ptl_list_t ptl_list;
    ptl_internal_appendME_t Qentry;
} ptl_internal_me_t;

static ptl_internal_me_t *mes[4] = { NULL, NULL, NULL, NULL };

void INTERNAL PtlInternalMENISetup(
    unsigned int ni,
    ptl_size_t limit)
{
    ptl_internal_me_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(mes[ni]), NULL,
				    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
	tmp = calloc(limit, sizeof(ptl_internal_me_t));
	assert(tmp != NULL);
	__sync_synchronize();
	mes[ni] = tmp;
    }
}

void INTERNAL PtlInternalMENITeardown(
    unsigned int ni)
{
    ptl_internal_me_t *tmp;
    tmp = mes[ni];
    mes[ni] = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    free(tmp);
}

int API_FUNC PtlMEAppend(
    ptl_handle_ni_t ni_handle,
    ptl_pt_index_t pt_index,
    ptl_me_t me,
    ptl_list_t ptl_list,
    void *user_ptr,
    ptl_handle_me_t * me_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_handle_encoding_t meh = { HANDLE_ME_CODE, 0, 0 };
    ptl_internal_appendME_t *Qentry = NULL;
    ptl_table_entry_t *t;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized\n");
	return PTL_NO_INIT;
    }
    if (ni.s.ni >= 4 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
	VERBOSE_ERROR("ni code wrong\n");
	return PTL_ARG_INVALID;
    }
    if (ni.s.ni == 1 || ni.s.ni == 3) {	// must be a non-matching NI
	VERBOSE_ERROR("must be a matching NI\n");
	return PTL_ARG_INVALID;
    }
    if (nit.tables[ni.s.ni] == NULL) { // this should never happen
	assert(nit.tables[ni.s.ni] != NULL);
	return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits.max_pt_index) {
	VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
		      nit_limits.max_pt_index);
	return PTL_ARG_INVALID;
    }
    {
	int ptv = PtlInternalPTValidate(&nit.tables[ni.s.ni][pt_index]);
	if (ptv == 1 || ptv == 3) {    // Unallocated or bad EQ (enabled/disabled both allowed)
	    VERBOSE_ERROR("MEAppend sees an invalid PT\n");
	    return PTL_ARG_INVALID;
	}
    }
#endif
    assert(mes[ni.s.ni] != NULL);
    meh.ni = ni.s.ni;
    /* find an ME handle */
    for (uint32_t offset = 0; offset < nit_limits.max_mes; ++offset) {
	if (mes[ni.s.ni][offset].status == 0) {
	    if (PtlInternalAtomicCas32
		(&(mes[ni.s.ni][offset].status), ME_FREE,
		 ME_ALLOCATED) == ME_FREE) {
		meh.code = offset;
		mes[ni.s.ni][offset].visible = me;
		mes[ni.s.ni][offset].pt_index = pt_index;
		mes[ni.s.ni][offset].ptl_list = ptl_list;
		Qentry = &(mes[ni.s.ni][offset].Qentry);
		break;
	    }
	}
    }
    if (Qentry == NULL) {
	return PTL_FAIL;
    }
    Qentry->user_ptr = user_ptr;
    Qentry->me_handle.s = meh;
    memcpy(me_handle, &meh, sizeof(ptl_handle_me_t));
    /* append to associated list */
    assert(nit.tables[ni.s.ni] != NULL);
    t = &(nit.tables[ni.s.ni][pt_index]);
    assert(pthread_mutex_lock(&t->lock) == 0);
    switch (ptl_list) {
	case PTL_PRIORITY_LIST:
	    if (t->overflow.head == NULL) {
		ptl_internal_appendME_t *prev =
		    (ptl_internal_appendME_t *) (t->priority.tail);
		t->priority.tail = Qentry;
		if (prev == NULL) {
		    t->priority.head = Qentry;
		} else {
		    prev->next = Qentry;
		}
	    } else {
#warning PtlMEAppend() does not check the overflow receives
		abort();
	    }
	    break;
	case PTL_OVERFLOW:
	{
	    ptl_internal_appendME_t *prev =
		(ptl_internal_appendME_t *) (t->overflow.tail);
	    t->overflow.tail = Qentry;
	    if (prev == NULL) {
		t->overflow.head = Qentry;
	    } else {
		prev->next = Qentry;
	    }
	}
	    break;
	case PTL_PROBE_ONLY:
#warning PTL_PROBE_ONLY not implemented
	    fprintf(stderr, "PTL_PROBE_ONLY not yet implemented\n");
	    abort();
	    break;
    }
    assert(pthread_mutex_unlock(&t->lock) == 0);
    return PTL_OK;
}

int API_FUNC PtlMEUnlink(
    ptl_handle_me_t me_handle)
{
    const ptl_internal_handle_converter_t me = { me_handle };
    ptl_table_entry_t *t;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized");
	return PTL_NO_INIT;
    }
    if (me.s.ni > 3 || me.s.code > nit_limits.max_mes ||
	nit.refcount[me.s.ni] == 0) {
	VERBOSE_ERROR
	    ("ME Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n",
	     me.s.ni, me.s.code, nit_limits.max_mes);
	return PTL_ARG_INVALID;
    }
    if (mes[me.s.ni] == NULL) {
	VERBOSE_ERROR("ME array uninitialized\n");
	return PTL_ARG_INVALID;
    }
    if (mes[me.s.ni][me.s.code].status == ME_FREE) {
	VERBOSE_ERROR("ME appears to be free already\n");
	return PTL_ARG_INVALID;
    }
#endif
    t = &(nit.tables[me.s.ni][mes[me.s.ni][me.s.code].pt_index]);
    assert(pthread_mutex_lock(&t->lock) == 0);
    switch (mes[me.s.ni][me.s.code].ptl_list) {
	case PTL_PRIORITY_LIST:
	{
	    ptl_internal_appendME_t *dq =
		(ptl_internal_appendME_t *) (t->priority.head);
	    if (dq == &(mes[me.s.ni][me.s.code].Qentry)) {
		if (dq->next != NULL) {
		    t->priority.head = dq->next;
		} else {
		    t->priority.head = t->priority.tail = NULL;
		}
	    } else {
		ptl_internal_appendME_t *prev = NULL;
		while (dq != &(mes[me.s.ni][me.s.code].Qentry) && dq != NULL) {
		    prev = dq;
		    dq = dq->next;
		}
		if (dq == NULL) {
		    fprintf(stderr, "attempted to link an un-queued ME\n");
		    abort();
		}
		prev->next = dq->next;
		if (dq->next == NULL) {
		    assert(t->priority.tail == dq);
		    t->priority.tail = prev;
		}
	    }
	}
	    break;
	case PTL_OVERFLOW:
	{
	    ptl_internal_appendME_t *dq =
		(ptl_internal_appendME_t *) (t->overflow.head);
	    if (dq == &(mes[me.s.ni][me.s.code].Qentry)) {
		if (dq->next != NULL) {
		    t->overflow.head = dq->next;
		} else {
		    t->overflow.head = t->overflow.tail = NULL;
		}
	    } else {
		ptl_internal_appendME_t *prev = NULL;
		while (dq != &(mes[me.s.ni][me.s.code].Qentry) && dq != NULL) {
		    prev = dq;
		    dq = dq->next;
		}
		if (dq == NULL) {
		    fprintf(stderr, "attempted to link an un-queued ME\n");
		    abort();
		}
		prev->next = dq->next;
		if (dq->next == NULL) {
		    assert(t->overflow.tail == dq);
		    t->overflow.tail = prev;
		}
	    }
	}
	    break;
	case PTL_PROBE_ONLY:
	    fprintf(stderr, "how on earth did this happen?\n");
	    abort();
	    break;
    }
    assert(pthread_mutex_unlock(&t->lock) == 0);
    switch(PtlInternalAtomicCas32(&(mes[me.s.ni][me.s.code].status), ME_ALLOCATED, ME_FREE)) {
	case ME_IN_USE:
	    return PTL_IN_USE;
	case ME_ALLOCATED:
	    return PTL_OK;
#ifndef NO_ARG_VALIDATION
	case ME_FREE:
	    VERBOSE_ERROR("ME unexpectedly became free");
	    return PTL_ARG_INVALID;
#endif
    }
    return PTL_OK;
}

int INTERNAL PtlInternalMEDeliver(
    ptl_table_entry_t * restrict t,
    ptl_internal_header_t * restrict h)
{
    return PTL_FAIL;
}
