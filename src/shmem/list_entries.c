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

#define LE_FREE		0
#define LE_ALLOCATED	1
#define LE_IN_USE	2

typedef struct {
    ptl_le_t visible;
    volatile uint32_t status;	// 0=free, 1=allocated, 2=in-use
} ptl_internal_le_t;

typedef struct {
    void *next; // for nemesis
    void *user_ptr;
    ptl_internal_handle_converter_t le_handle;
} ptl_internal_appendLE_t;

static ptl_internal_le_t *les = NULL;
static NEMESIS_blocking_queue appends;

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
	PtlInternalNEMESISBlockingInit(&appends);
	__sync_synchronize();
	les = tmp;
	//assert(pthread_create(&LEthread, NULL, LEprocessor, NULL) == 0);
    }
}

void INTERNAL PtlInternalLENITeardown(
    void)
{
    ptl_internal_le_t *tmp;
    //assert(pthread_cancel(LEthread) == 0);
    tmp = les;
    les = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    free(tmp);
    PtlInternalNEMESISBlockingDestroy(&appends);
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
    assert(les != NULL);
    leh.ni = ni.s.ni;
    Qentry = malloc(sizeof(ptl_internal_appendLE_t));
    if (Qentry == NULL) {
	return PTL_NO_SPACE;
    }
    assert(Qentry != NULL);
    Qentry->user_ptr = user_ptr;
    /* find an LE handle */
    for (offset = 0; offset < nit_limits.max_mes; ++offset) {
	if (les[offset].status == 0) {
	    if (PtlInternalAtomicCas32
		(&(les[offset].status), LE_FREE, LE_IN_USE) == LE_FREE) {
		leh.code = offset;
		les[offset].visible = le;
		break;
	    }
	}
    }
    Qentry->le_handle.s = leh;
    memcpy(le_handle, &leh, sizeof(ptl_handle_le_t));
    /* append to associated list */
    t = &(nit.tables[ni.s.ni][pt_index]);
    assert(pthread_mutex_lock(&t->lock) == 0);
    switch (ptl_list) {
	case PTL_PRIORITY_LIST:
	    {
		ptl_internal_appendLE_t* prev = (ptl_internal_appendLE_t*)(t->priority.tail);
		t->priority.tail = Qentry;
		if (prev == NULL) {
		    t->priority.head = Qentry;
		} else {
		    prev->next = Qentry;
		}
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
#ifndef NO_ARG_VALIDATION
    int ct_optional = 0;
    if (comm_pad == NULL) {
	VERBOSE_ERROR("communication pad not initialized\n");
	return PTL_NO_INIT;
    }
    if (le.s.ni > 3 || le.s.code > nit_limits.max_mds || (nit.refcount[le.s.ni] == 0)) {
	VERBOSE_ERROR("LE Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n", le.s.ni, le.s.code, nit_limits.max_mds);
	return PTL_ARG_INVALID;
    }
    if (les == NULL) {
	VERBOSE_ERROR("LE array uninitialized\n");
	return PTL_ARG_INVALID;
    }
    if (les[le.s.code].status == LE_FREE) {
	VERBOSE_ERROR("LE appears to be free already\n");
	return PTL_ARG_INVALID;
    }
    if (les[le.s.code].visible.options & (PTL_LE_EVENT_CT_GET | PTL_LE_EVENT_CT_PUT |
		   PTL_LE_EVENT_CT_PUT_OVERFLOW | PTL_LE_EVENT_CT_ATOMIC | PTL_LE_EVENT_CT_ATOMIC_OVERFLOW)) {
	ct_optional = 0;
    }
    if (PtlInternalCTHandleValidator(les[le.s.code].visible.ct_handle, ct_optional)) {
	VERBOSE_ERROR("LE has a bad CT handle\n");
	return PTL_ARG_INVALID;
    }
#endif
#warning need to fix the LE in-use detection
    les[le.s.code].status = LE_FREE;
    return PTL_OK;
}

int INTERNAL PtlInternalLEDeliver(
    ptl_table_entry_t *restrict t,
    ptl_internal_header_t *restrict hdr)
{
    ptl_event_t e; // for posting what happens here
    assert(t);
    assert(hdr);
    printf("t->priority.head = %p, t->overflow.head = %p\n", t->priority.head, t->overflow.head);
    if (t->priority.head) {
	ptl_internal_appendLE_t *entry = t->priority.head;
	ptl_le_t *le = &(les[entry->le_handle.s.code].visible);
	assert(le != NULL);
	assert(les[entry->le_handle.s.code].status != LE_FREE);
	if (les[entry->le_handle.s.code].visible.options & PTL_LE_USE_ONCE) {
	    if (entry->next != NULL) {
		t->priority.head = entry->next;
	    } else {
		t->priority.head = t->priority.tail = NULL;
	    }
	    if ((les[entry->le_handle.s.code].visible.options & (PTL_LE_EVENT_DISABLE|PTL_LE_EVENT_UNLINK_DISABLE)) == 0) {
		e.type = PTL_EVENT_UNLINK;
		e.event.tevent.initiator.phys.pid = hdr->src;
		e.event.tevent.initiator.phys.nid = 0;
		e.event.tevent.pt_index = hdr->pt_index;
		e.event.tevent.uid = 0;
		e.event.tevent.match_bits = 0;
		e.event.tevent.rlength = hdr->length;
		e.event.tevent.mlength = hdr->length;
		e.event.tevent.remote_offset = hdr->dest_offset;
		e.event.tevent.start = (char*)le->start + hdr->dest_offset;
		e.event.tevent.user_ptr = hdr->user_ptr;
		switch (hdr->type) {
		    case HDR_TYPE_PUT:
			e.event.tevent.hdr_data = hdr->info.put.hdr_data;
			break;
		    case HDR_TYPE_ATOMIC:
			e.event.tevent.hdr_data = hdr->info.atomic.hdr_data;
			break;
		    case HDR_TYPE_FETCHATOMIC:
			e.event.tevent.hdr_data = hdr->info.fetchatomic.hdr_data;
			break;
		    case HDR_TYPE_SWAP:
			e.event.tevent.hdr_data = hdr->info.swap.hdr_data;
			break;
		}
		e.event.tevent.ni_fail_type = PTL_NI_OK;
		PtlInternalEQPush(t->EQ, &e);
	    }
	}
	assert(entry);
	printf("checking protections on the LE (%p)\n", le);
	// check the protections on the le
	if (le->options & PTL_LE_AUTH_USE_JID) {
	    if (le->ac_id.jid != PTL_JID_ANY) {
		fprintf(stderr, "BAD AC_ID! 1 (I should probably enqueue an event of some kind and free the memory)\n");
		PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_PERMISSIONS_VIOLATIONS], 1);
		return (les[entry->le_handle.s.code].visible.options & PTL_LE_ACK_DISABLE)?0:3;
	    }
	} else {
	    if (le->ac_id.uid != PTL_UID_ANY) {
		fprintf(stderr, "BAD AC_ID! 2 (I should probably enqueue an event of some kind and free the memory)\n");
		PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_PERMISSIONS_VIOLATIONS], 1);
		return (les[entry->le_handle.s.code].visible.options & PTL_LE_ACK_DISABLE)?0:3;
	    }
	}
	switch (hdr->type) {
	    case HDR_TYPE_PUT:
		if ((le->options & PTL_LE_OP_PUT) == 0) {
		    fprintf(stderr, "LE not labelled for PUT\n");
		    abort();
		}
		if (hdr->length > (le->length - hdr->dest_offset)) {
		    fprintf(stderr, "LE is too short!\n");
		    abort();
		}
		/* drumroll please... */
		printf("calling memcpy(%p + %lu, %p, %lu)\n", le->start, (unsigned long)hdr->dest_offset, hdr->data, (unsigned long)hdr->length);
		memcpy((char*)le->start + hdr->dest_offset, hdr->data, hdr->length);
		printf("memcopy returned!\n");
		/* now announce it */
		if (le->options & PTL_LE_EVENT_CT_PUT) {
		    ptl_ct_event_t cte = {1, 0};
		    printf("incrementing CT\n");
		    PtlCTInc(le->ct_handle, cte);
		}
		printf("EQ?\n");
		if ((le->options & (PTL_LE_EVENT_DISABLE|PTL_LE_EVENT_SUCCESS_DISABLE)) == 0) {
		    e.type = PTL_EVENT_PUT;
		    e.event.tevent.initiator.phys.pid = hdr->src;
		    e.event.tevent.initiator.phys.nid = 0;
		    e.event.tevent.pt_index = hdr->pt_index;
		    e.event.tevent.uid = 0;
		    e.event.tevent.match_bits = 0;
		    e.event.tevent.rlength = hdr->length;
		    e.event.tevent.mlength = hdr->length;
		    e.event.tevent.remote_offset = hdr->dest_offset;
		    e.event.tevent.start = (char*)le->start + hdr->dest_offset;
		    e.event.tevent.user_ptr = hdr->user_ptr;
		    switch (hdr->type) {
			case HDR_TYPE_PUT:
			    e.event.tevent.hdr_data = hdr->info.put.hdr_data;
			    break;
			case HDR_TYPE_ATOMIC:
			    e.event.tevent.hdr_data = hdr->info.atomic.hdr_data;
			    break;
			case HDR_TYPE_FETCHATOMIC:
			    e.event.tevent.hdr_data = hdr->info.fetchatomic.hdr_data;
			    break;
			case HDR_TYPE_SWAP:
			    e.event.tevent.hdr_data = hdr->info.swap.hdr_data;
			    break;
		    }
		    e.event.tevent.ni_fail_type = PTL_NI_OK;
		    printf("push to EQ\n");
		    PtlInternalEQPush(t->EQ, &e);
		    printf("pushed\n");
		}
		break;
	    default:
		fprintf(stderr, "non-put LE handling is unimplemented\n");
		abort();
	}
	return (les[entry->le_handle.s.code].visible.options & PTL_LE_ACK_DISABLE)?0:1;
    } else if (t->overflow.head) {
	fprintf(stderr, "overflow LE handling is unimplemented\n");
	abort();
	return 2; // check for ACK_DISABLE
    } else { // nothing posted *at all!*
	PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_DROP_COUNT], 1);
	if (t->EQ != PTL_EQ_NONE) {
	    e.type = PTL_EVENT_DROPPED;
	    e.event.tevent.initiator.phys.pid = hdr->src;
	    e.event.tevent.initiator.phys.nid = 0;
	    e.event.tevent.pt_index = hdr->pt_index;
	    e.event.tevent.uid = 0;
	    e.event.tevent.jid = PTL_JID_NONE;
	    e.event.tevent.match_bits = 0;
	    e.event.tevent.rlength = hdr->length;
	    e.event.tevent.mlength = 0;
	    e.event.tevent.remote_offset = hdr->dest_offset;
	    e.event.tevent.start = NULL;
	    e.event.tevent.user_ptr = hdr->user_ptr;
	    switch (hdr->type) {
		case HDR_TYPE_PUT:
		    e.event.tevent.hdr_data = hdr->info.put.hdr_data;
		    break;
		case HDR_TYPE_ATOMIC:
		    e.event.tevent.hdr_data = hdr->info.atomic.hdr_data;
		    break;
		case HDR_TYPE_FETCHATOMIC:
		    e.event.tevent.hdr_data = hdr->info.fetchatomic.hdr_data;
		    break;
		case HDR_TYPE_SWAP:
		    e.event.tevent.hdr_data = hdr->info.swap.hdr_data;
		    break;
	    }
	    e.event.tevent.ni_fail_type = PTL_NI_OK;
	    PtlInternalEQPush(t->EQ, &e);
	}
	return 4;
    }
}
