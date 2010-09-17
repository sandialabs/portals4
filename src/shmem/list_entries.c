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

#include <stdio.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_LE.h"
#include "ptl_internal_nemesis.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_PT.h"
#include "ptl_internal_error.h"
#include "ptl_internal_performatomic.h"

#define LE_FREE		0
#define LE_ALLOCATED	1
#define LE_IN_USE	2

typedef struct {
    void *next; // for nemesis
    void *user_ptr;
    ptl_internal_handle_converter_t le_handle;
} ptl_internal_appendLE_t;

typedef struct {
    ptl_internal_appendLE_t Qentry;
    ptl_le_t visible;
    volatile uint32_t status;	// 0=free, 1=allocated, 2=in-use
    ptl_pt_index_t pt_index;
    ptl_list_t ptl_list;
} ptl_internal_le_t;

static ptl_internal_le_t *les[4] = { NULL, NULL, NULL, NULL };

void INTERNAL PtlInternalLENISetup(
	unsigned int ni,
    ptl_size_t limit)
{
    ptl_internal_le_t *tmp;
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(les[ni]), NULL, (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
	tmp = calloc(limit, sizeof(ptl_internal_le_t));
	assert(tmp != NULL);
	__sync_synchronize();
	les[ni] = tmp;
    }
}

void INTERNAL PtlInternalLENITeardown(
    unsigned int ni)
{
    ptl_internal_le_t *tmp;
    tmp = les[ni];
    les[ni] = NULL;
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
    ptl_internal_handle_converter_t leh = { .s.selector = HANDLE_LE_CODE };
    ptl_internal_appendLE_t *Qentry = NULL;
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
    if (ni.s.ni == 0 || ni.s.ni == 2) {	// must be a non-matching NI
	VERBOSE_ERROR("must be a non-matching NI\n");
	return PTL_ARG_INVALID;
    }
    if (nit.tables[ni.s.ni] == NULL) { // this should never happen
	assert(nit.tables[ni.s.ni] != NULL);
	return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits.max_pt_index) {
	VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index, nit_limits.max_pt_index);
	return PTL_ARG_INVALID;
    }
    {
	int ptv = PtlInternalPTValidate(&nit.tables[ni.s.ni][pt_index]);
	if (ptv == 1 || ptv == 3) { // Unallocated or bad EQ (enabled/disabled both allowed)
	    VERBOSE_ERROR("LEAppend sees an invalid PT\n");
	    return PTL_ARG_INVALID;
	}
    }
#endif
    assert(les[ni.s.ni] != NULL);
    leh.s.ni = ni.s.ni;
    /* find an LE handle */
    for (uint32_t offset = 0; offset < nit_limits.max_mes; ++offset) {
	if (les[ni.s.ni][offset].status == 0) {
	    if (PtlInternalAtomicCas32
		(&(les[ni.s.ni][offset].status), LE_FREE, LE_ALLOCATED) == LE_FREE) {
		leh.s.code = offset;
		les[ni.s.ni][offset].visible = le;
		les[ni.s.ni][offset].pt_index = pt_index;
		les[ni.s.ni][offset].ptl_list = ptl_list;
		Qentry = &(les[ni.s.ni][offset].Qentry);
		break;
	    }
	}
    }
    if (Qentry == NULL) {
	return PTL_FAIL;
    }
    Qentry->user_ptr = user_ptr;
    Qentry->le_handle = leh;
    *le_handle = leh.a.le;
    /* append to associated list */
    assert(nit.tables[ni.s.ni] != NULL);
    t = &(nit.tables[ni.s.ni][pt_index]);
    assert(pthread_mutex_lock(&t->lock) == 0);
    switch (ptl_list) {
	case PTL_PRIORITY_LIST:
	    if (t->overflow.head == NULL) {
		ptl_internal_appendLE_t* prev = (ptl_internal_appendLE_t*)(t->priority.tail);
		t->priority.tail = Qentry;
		if (prev == NULL) {
		    t->priority.head = Qentry;
		} else {
		    prev->next = Qentry;
		}
	    } else {
#warning PtlLEAppend() does not check the overflow receives
		abort();
	    }
	    break;
	case PTL_OVERFLOW:
	    {
		ptl_internal_appendLE_t* prev = (ptl_internal_appendLE_t*)(t->overflow.tail);
		t->overflow.tail = Qentry;
		if (prev == NULL) {
		    t->overflow.head = Qentry;
		} else {
		    prev->next = Qentry;
		}
	    }
	    break;
	case PTL_PROBE_ONLY:
#warning PtlLEAppend() does not check the overflow receives
	    fprintf(stderr, "PTL_PROBE_ONLY not yet implemented\n");
	    abort();
	    break;
    }
    assert(pthread_mutex_unlock(&t->lock) == 0);
    return PTL_OK;
}

int API_FUNC PtlLEUnlink(
    ptl_handle_le_t le_handle)
{
    const ptl_internal_handle_converter_t le = { le_handle };
    ptl_table_entry_t *t;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized\n");
	return PTL_NO_INIT;
    }
    if (le.s.ni > 3 || le.s.code > nit_limits.max_mes || (nit.refcount[le.s.ni] == 0)) {
	VERBOSE_ERROR("LE Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n", le.s.ni, le.s.code, nit_limits.max_mes);
	return PTL_ARG_INVALID;
    }
    if (les[le.s.ni] == NULL) {
	VERBOSE_ERROR("LE array uninitialized\n");
	return PTL_ARG_INVALID;
    }
    if (les[le.s.ni][le.s.code].status == LE_FREE) {
	VERBOSE_ERROR("LE appears to be free already\n");
	return PTL_ARG_INVALID;
    }
#endif
    t = &(nit.tables[le.s.ni][les[le.s.ni][le.s.code].pt_index]);
    assert(pthread_mutex_lock(&t->lock) == 0);
    switch(les[le.s.ni][le.s.code].ptl_list) {
	case PTL_PRIORITY_LIST:
	    {
		ptl_internal_appendLE_t *dq = (ptl_internal_appendLE_t*)(t->priority.head);
		if (dq == &(les[le.s.ni][le.s.code].Qentry)) {
		    if (dq->next != NULL) {
			t->priority.head = dq->next;
		    } else {
			t->priority.head = t->priority.tail = NULL;
		    }
		} else {
		    ptl_internal_appendLE_t *prev = NULL;
		    while (dq != &(les[le.s.ni][le.s.code].Qentry) && dq != NULL) {
			prev = dq;
			dq = dq->next;
		    }
		    if (dq == NULL) {
			fprintf(stderr, "attempted to unlink an un-queued LE\n");
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
		ptl_internal_appendLE_t *dq = (ptl_internal_appendLE_t*)(t->overflow.head);
		if (dq == &(les[le.s.ni][le.s.code].Qentry)) {
		    if (dq->next != NULL) {
			t->overflow.head = dq->next;
		    } else {
			t->overflow.head = t->overflow.tail = NULL;
		    }
		} else {
		    ptl_internal_appendLE_t *prev = NULL;
		    while (dq != &(les[le.s.ni][le.s.code].Qentry) && dq != NULL) {
			prev = dq;
			dq = dq->next;
		    }
		    if (dq == NULL) {
			fprintf(stderr, "attempted to unlink an un-queued LE\n");
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
    switch (PtlInternalAtomicCas32(&(les[le.s.ni][le.s.code].status), LE_ALLOCATED, LE_FREE)) {
	case LE_IN_USE:
	    return PTL_IN_USE;
	case LE_ALLOCATED:
	    return PTL_OK;
#ifndef NO_ARG_VALIDATION
	case LE_FREE:
	    VERBOSE_ERROR("LE unexpectedly became free");
	    return PTL_ARG_INVALID;
#endif
    }
    return PTL_OK;
}

ptl_pid_t INTERNAL PtlInternalLEDeliver(
    ptl_table_entry_t *restrict t,
    ptl_internal_header_t *restrict hdr)
{
    ptl_size_t mlength;
    assert(t);
    assert(hdr);
    ptl_event_t e = { .event.tevent = {
	.pt_index = hdr->pt_index,
	.uid = 0,
	.jid = PTL_JID_NONE,
	.match_bits = 0,
	.mlength = 0,
	.rlength = hdr->length,
	.remote_offset = hdr->dest_offset,
	.user_ptr = hdr->user_ptr,
	.ni_fail_type = PTL_NI_OK
    } };
    if (hdr->ni <= 1) { // Logical
	e.event.tevent.initiator.rank = hdr->src;
    } else { // Physical
	e.event.tevent.initiator.phys.pid = hdr->src;
	e.event.tevent.initiator.phys.nid = 0;
    }
    switch (hdr->type) {
	case HDR_TYPE_PUT:
	    e.type = PTL_EVENT_PUT;
	    e.event.tevent.hdr_data = hdr->info.put.hdr_data;
	    break;
	case HDR_TYPE_ATOMIC:
	    e.type = PTL_EVENT_ATOMIC;
	    e.event.tevent.hdr_data = hdr->info.atomic.hdr_data;
	    break;
	case HDR_TYPE_FETCHATOMIC:
	    e.type = PTL_EVENT_ATOMIC;
	    e.event.tevent.hdr_data = hdr->info.fetchatomic.hdr_data;
	case HDR_TYPE_SWAP:
	    e.type = PTL_EVENT_ATOMIC;
	    e.event.tevent.hdr_data = hdr->info.swap.hdr_data;
	    break;
	case HDR_TYPE_GET:
	    e.type = PTL_EVENT_GET;
	    break;
    }
    //printf("%u ~~> t->priority.head = %p, t->overflow.head = %p\n", (unsigned)proc_number, t->priority.head, t->overflow.head);
    if (t->priority.head) {
	ptl_internal_appendLE_t *entry = t->priority.head;
	const ptl_le_t le = *(ptl_le_t*)(((char*)entry) + offsetof(ptl_internal_le_t, visible));
	assert(les[hdr->ni][entry->le_handle.s.code].status != LE_FREE);
	assert(entry);
	/*********************************************************
	 * There is an LE present, and 'entry'/'le' points to it *
	 *********************************************************/
	// check the permissions on the LE
	if (le.options & PTL_LE_AUTH_USE_JID) {
	    if (le.ac_id.jid != PTL_JID_ANY) {
		goto permission_violation;
	    }
	} else {
	    if (le.ac_id.uid != PTL_UID_ANY) {
		goto permission_violation;
	    }
	}
	switch (hdr->type) {
	    case HDR_TYPE_PUT:
	    case HDR_TYPE_ATOMIC:
	    case HDR_TYPE_FETCHATOMIC:
	    case HDR_TYPE_SWAP:
		if ((le.options & PTL_LE_OP_PUT) == 0) {
		    goto permission_violation;
		}
	}
	switch (hdr->type) {
	    case HDR_TYPE_GET:
	    case HDR_TYPE_FETCHATOMIC:
	    case HDR_TYPE_SWAP:
		if ((le.options & (PTL_LE_ACK_DISABLE|PTL_LE_OP_GET)) == 0) {
		    goto permission_violation;
		}
	}
	if (0) {
permission_violation:
	    (void)PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_PERMISSIONS_VIOLATIONS], 1);
	    return (ptl_pid_t)((le.options & PTL_LE_ACK_DISABLE)?0:3);
	}
	/*******************************************************************
	 * We have permissions on this LE, now check if it's a use-once LE *
	 *******************************************************************/
	if (le.options & PTL_LE_USE_ONCE) {
	    if (entry->next != NULL) {
		t->priority.head = entry->next;
	    } else {
		t->priority.head = t->priority.tail = NULL;
	    }
	    if (t->EQ != PTL_EQ_NONE && (le.options & (PTL_LE_EVENT_DISABLE|PTL_LE_EVENT_UNLINK_DISABLE)) == 0) {
		e.type = PTL_EVENT_UNLINK;
		e.event.tevent.start = (char*)le.start + hdr->dest_offset;
		PtlInternalEQPush(t->EQ, &e);
	    }
	}
	/* check lengths */
	if (hdr->length > (le.length - hdr->dest_offset)) {
	    mlength = le.length - hdr->dest_offset;
	} else {
	    mlength = hdr->length;
	}
	/*************************
	 * Perform the Operation *
	 *************************/
	switch (hdr->type) {
	    case HDR_TYPE_PUT:
		memcpy((char*)le.start + hdr->dest_offset, hdr->data, mlength);
		break;
	    case HDR_TYPE_ATOMIC:
		PtlInternalPerformAtomic((char*)le.start + hdr->dest_offset, hdr->data, mlength, hdr->info.atomic.operation, hdr->info.atomic.datatype);
		break;
	    case HDR_TYPE_FETCHATOMIC:
		PtlInternalPerformAtomic((char*)le.start + hdr->dest_offset, hdr->data, mlength, hdr->info.fetchatomic.operation, hdr->info.fetchatomic.datatype);
		break;
	    case HDR_TYPE_GET:
		memcpy(hdr->data, (char*)le.start + hdr->dest_offset, mlength);
		break;
	    case HDR_TYPE_SWAP:
		PtlInternalPerformAtomicArg((char*)le.start + hdr->dest_offset, hdr->data+8, *(uint64_t*)hdr->data, mlength, hdr->info.swap.operation, hdr->info.swap.datatype);
		break;
	    default:
		UNREACHABLE;
		*(int*)0 = 0;
	}
	/* now announce it */
	//printf("%u ~~> announcing delivery...\n", (unsigned)proc_number);
	{
	    const ptl_handle_eq_t t_eq = t->EQ;
	    int ct_announce = le.ct_handle != PTL_CT_NONE;
	    if (ct_announce != 0) {
		switch (hdr->type) {
		    case HDR_TYPE_PUT:
			ct_announce = le.options & PTL_LE_EVENT_CT_PUT;
			break;
		    case HDR_TYPE_GET:
			ct_announce = le.options & PTL_LE_EVENT_CT_GET;
			break;
		    case HDR_TYPE_ATOMIC:
		    case HDR_TYPE_FETCHATOMIC:
		    case HDR_TYPE_SWAP:
			ct_announce = le.options & PTL_LE_EVENT_CT_ATOMIC;
			break;
		}
	    }
	    if (ct_announce != 0) {
		//printf("%u ~~> incrementing CT %u...\n", (unsigned)proc_number, le_ct);
		if ((le.options & PTL_LE_EVENT_CT_BYTES) == 0) {
		    ptl_ct_event_t cte = {1, 0};
		    PtlCTInc(le.ct_handle, cte);
		} else {
		    ptl_ct_event_t cte = {mlength, 0};
		    PtlCTInc(le.ct_handle, cte);
		}
	    } else {
		//printf("%u ~~> NOT incrementing CT \n", (unsigned)proc_number);
	    }
	    if (t_eq != PTL_EQ_NONE && (le.options & (PTL_LE_EVENT_DISABLE|PTL_LE_EVENT_SUCCESS_DISABLE)) == 0) {
		e.event.tevent.mlength = mlength;
		e.event.tevent.start = (char*)le.start + hdr->dest_offset;
		PtlInternalEQPush(t_eq, &e);
	    }
	}
	return (ptl_pid_t)((le.options & PTL_LE_ACK_DISABLE)?0:1);
    } else if (t->overflow.head) {
#warning Overflow LE handling is unimplemented
	fprintf(stderr, "overflow LE handling is unimplemented\n");
	abort();
	return (ptl_pid_t)2; // check for ACK_DISABLE
    } else { // nothing posted *at all!*
	if (t->EQ != PTL_EQ_NONE) {
	    e.type = PTL_EVENT_DROPPED;
	    e.event.tevent.start = NULL;
	    PtlInternalEQPush(t->EQ, &e);
	}
	(void)PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_DROP_COUNT], 1);
	return 0; // silent ACK
    }
}
