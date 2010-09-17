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
#include "ptl_internal_EQ.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_performatomic.h"

#define ME_FREE		0
#define ME_ALLOCATED	1
#define ME_IN_USE	2

typedef struct {
    void *next;			// for nemesis
    void *user_ptr;
    ptl_internal_handle_converter_t me_handle;
    size_t local_offset;
    ptl_match_bits_t dont_ignore_bits;
} ptl_internal_appendME_t;

typedef struct {
    ptl_internal_appendME_t Qentry;
    ptl_me_t visible;
    volatile uint32_t status;	// 0=free, 1=allocated, 2=in-use
    ptl_pt_index_t pt_index;
    ptl_list_t ptl_list;
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
    ptl_internal_handle_converter_t meh = { .s.selector = HANDLE_ME_CODE };
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
    meh.s.ni = ni.s.ni;
    /* find an ME handle */
    for (uint32_t offset = 0; offset < nit_limits.max_mes; ++offset) {
	if (mes[ni.s.ni][offset].status == 0) {
	    if (PtlInternalAtomicCas32
		(&(mes[ni.s.ni][offset].status), ME_FREE,
		 ME_ALLOCATED) == ME_FREE) {
		meh.s.code = offset;
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
    Qentry->me_handle = meh;
    Qentry->local_offset = 0;
    Qentry->dont_ignore_bits = ~me.ignore_bits;
    *me_handle = meh.a.me;
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
    switch (PtlInternalAtomicCas32
	    (&(mes[me.s.ni][me.s.code].status), ME_ALLOCATED, ME_FREE)) {
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

static void PtlInternalWalkMatchList(
    const unsigned int incoming_bits,
    const unsigned char ni,
    const ptl_process_t target,
    const ptl_size_t length,
    const ptl_size_t offset,
    ptl_internal_appendME_t ** matchlist,
    ptl_internal_appendME_t ** mprev,
    ptl_me_t ** mme)
{
    ptl_internal_appendME_t *current = *matchlist;
    ptl_internal_appendME_t *prev = *mprev;
    ptl_me_t *me = *mme;

    for (; current != NULL; prev = current, current = current->next) {
	me = (ptl_me_t *) (((char *)current) +
			   offsetof(ptl_internal_me_t, visible));

	/* check the match_bits */
	if (((incoming_bits ^ me->match_bits) & current->dont_ignore_bits) != 0)
	    continue;
	/* check for forbidden truncation */
	if ((me->options & PTL_ME_NO_TRUNCATE) != 0 &&
	    length > (me->length - offset))
	    continue;
	/* check for match_id */
	if (ni <= 1) {		       // Logical
	    if (me->match_id.rank != PTL_RANK_ANY &&
		me->match_id.rank != target.rank)
		continue;
	} else {		       // Physical
	    if (me->match_id.phys.nid != PTL_NID_ANY &&
		me->match_id.phys.nid != target.phys.nid)
		continue;
	    if (me->match_id.phys.pid != PTL_PID_ANY &&
		me->match_id.phys.pid != target.phys.pid)
		continue;
	}
	break;
    }
    *matchlist = current;
    *mprev = prev;
    *mme = me;
}

ptl_pid_t INTERNAL PtlInternalMEDeliver(
    ptl_table_entry_t * restrict t,
    ptl_internal_header_t * restrict hdr)
{
    assert(t);
    assert(hdr);
    enum { PRIORITY, OVERFLOW } foundin = PRIORITY;
    ptl_internal_appendME_t *prev = NULL, *priority_list = t->priority.head;
    ptl_me_t *me = NULL;
    ptl_event_t e = {.event.tevent = {
				      .pt_index = hdr->pt_index,
				      .uid = 0,
				      .jid = PTL_JID_NONE,
				      .match_bits = hdr->match_bits,
				      .rlength = hdr->length,
				      .mlength = 0,
				      .remote_offset = hdr->dest_offset,
				      .user_ptr = hdr->user_ptr,
				      .ni_fail_type = PTL_NI_OK}
    };
    if (hdr->ni <= 1) {		       // Logical
	e.event.tevent.initiator.rank = hdr->src;
    } else {			       // Physical
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
	    break;
	case HDR_TYPE_SWAP:
	    e.type = PTL_EVENT_ATOMIC;
	    e.event.tevent.hdr_data = hdr->info.swap.hdr_data;
	    break;
	case HDR_TYPE_GET:
	    e.type = PTL_EVENT_GET;
	    break;
    }
    /* To match, one must check, in order:
     * 1. The match_bits (with the ignore_bits) against hdr->match_bits
     * 2. if notruncate, length
     * 3. the match_id against hdr->target_id
     */
    PtlInternalWalkMatchList(hdr->match_bits, hdr->ni, hdr->target_id,
			     hdr->length, hdr->dest_offset, &priority_list,
			     &prev, &me);
    if (priority_list == NULL && hdr->type != HDR_TYPE_GET) {	// check overflow list
	prev = NULL;
	priority_list = t->overflow.head;
	PtlInternalWalkMatchList(hdr->match_bits, hdr->ni, hdr->target_id,
				 hdr->length, hdr->dest_offset,
				 &priority_list, &prev, &me);
	if (priority_list != NULL) {
	    foundin = OVERFLOW;
	}
    }
    if (priority_list != NULL) {       // Match
	/*************************************************************************
	 * There is a matching ME present, and 'priority_list'/'me' points to it *
	 *************************************************************************/
	ptl_size_t mlength = 0;
	const ptl_me_t mec = *me;
	// check permissions on the ME
	if (mec.options & PTL_ME_AUTH_USE_JID) {
	    if (mec.ac_id.jid != PTL_JID_ANY) {
		goto permission_violation;
	    }
	} else {
	    if (mec.ac_id.uid != PTL_UID_ANY) {
		goto permission_violation;
	    }
	}
	switch (hdr->type) {
	    case HDR_TYPE_PUT:
	    case HDR_TYPE_ATOMIC:
	    case HDR_TYPE_FETCHATOMIC:
	    case HDR_TYPE_SWAP:
		if ((mec.options & PTL_ME_OP_PUT) == 0) {
		    goto permission_violation;
		}
	}
	switch (hdr->type) {
	    case HDR_TYPE_GET:
	    case HDR_TYPE_FETCHATOMIC:
	    case HDR_TYPE_SWAP:
		if ((mec.options & (PTL_ME_ACK_DISABLE | PTL_ME_OP_GET)) == 0) {
		    goto permission_violation;
		}
	}
	if (0) {
	  permission_violation:
	    (void)PtlInternalAtomicInc(&nit.
				       regs[hdr->
					    ni]
				       [PTL_SR_PERMISSIONS_VIOLATIONS], 1);
	    return (ptl_pid_t)((mec.options & PTL_ME_ACK_DISABLE) ? 0 : 3);
	}
	/*******************************************************************
	 * We have permissions on this ME, now check if it's a use-once ME *
	 *******************************************************************/
	if ((mec.options & PTL_ME_USE_ONCE) ||
	    ((mec.options & (PTL_ME_MIN_FREE | PTL_ME_MANAGE_LOCAL)) &&
	     (mec.length - priority_list->local_offset < mec.min_free))) {
	    // unlink ME
	    if (prev != NULL) {
		prev->next = priority_list->next;
	    } else {
		if (foundin == PRIORITY) {	// priority_list
		    t->priority.head = priority_list->next;
		} else {
		    t->overflow.head = priority_list->next;
		}
	    }
	    if (t->EQ != PTL_EQ_NONE &&
		(mec.
		 options & (PTL_ME_EVENT_DISABLE |
			    PTL_ME_EVENT_UNLINK_DISABLE)) == 0) {
		e.type = PTL_EVENT_UNLINK;
		e.event.tevent.start = (char *)mec.start + hdr->dest_offset;
		PtlInternalEQPush(t->EQ, &e);
	    }
	}
	/* check lengths */
	if (hdr->length > (mec.length - hdr->dest_offset)) {
	    mlength = mec.length - hdr->dest_offset;	// truncate
	} else {
	    mlength = hdr->length;
	}
	/*************************
	 * Perform the Operation *
	 *************************/
	switch (hdr->type) {
	    case HDR_TYPE_PUT:
		memcpy((char *)mec.start + hdr->dest_offset, hdr->data,
		       mlength);
		break;
	    case HDR_TYPE_ATOMIC:
		PtlInternalPerformAtomic((char *)mec.start + hdr->dest_offset,
					 hdr->data, mlength,
					 hdr->info.atomic.operation,
					 hdr->info.atomic.datatype);
		break;
	    case HDR_TYPE_FETCHATOMIC:
		PtlInternalPerformAtomic((char *)mec.start + hdr->dest_offset,
					 hdr->data, mlength,
					 hdr->info.fetchatomic.operation,
					 hdr->info.fetchatomic.datatype);
		break;
	    case HDR_TYPE_GET:
		memcpy(hdr->data, (char *)mec.start + hdr->dest_offset,
		       mlength);
		break;
	    case HDR_TYPE_SWAP:
		PtlInternalPerformAtomic((char *)mec.start + hdr->dest_offset,
					 hdr->data, mlength,
					 hdr->info.swap.operation,
					 hdr->info.swap.datatype);
		break;
	    default:
		UNREACHABLE;
		*(int *)0 = 0;
	}
	{
	    const ptl_handle_eq_t t_eq = t->EQ;
	    int ct_announce = mec.ct_handle != PTL_CT_NONE;
	    if (ct_announce != 0) {
		switch (hdr->type) {
		    case HDR_TYPE_PUT:
			ct_announce = mec.options & PTL_ME_EVENT_CT_PUT;
			break;
		    case HDR_TYPE_GET:
			ct_announce = mec.options & PTL_ME_EVENT_CT_GET;
			break;
		    case HDR_TYPE_ATOMIC:
		    case HDR_TYPE_FETCHATOMIC:
		    case HDR_TYPE_SWAP:
			ct_announce = mec.options & PTL_ME_EVENT_CT_ATOMIC;
			break;
		}
	    }
	    if (ct_announce != 0) {
		// increment counter
		if ((mec.options & PTL_ME_EVENT_CT_BYTES) == 0) {
		    ptl_ct_event_t cte = { 1, 0 };
		    PtlCTInc(mec.ct_handle, cte);
		} else {
		    ptl_ct_event_t cte = { mlength, 0 };
		    PtlCTInc(mec.ct_handle, cte);
		}
	    } else {
		//printf("NOT incrementing CT\n");
	    }
	    /* PT has EQ & msg/ME has events enabled? */
	    if (t_eq != PTL_EQ_NONE &&
		(mec.
		 options & (PTL_ME_EVENT_DISABLE |
			    PTL_ME_EVENT_SUCCESS_DISABLE)) == 0) {
		// record event
		e.event.tevent.mlength = mlength;
		e.event.tevent.start = (char *)mec.start + hdr->dest_offset;
		PtlInternalEQPush(t_eq, &e);
	    }
	}
	return (ptl_pid_t)((mec.options & (PTL_ME_ACK_DISABLE)) ? 0 : 1);
    }
    // post dropped message event
    if (t->EQ != PTL_EQ_NONE) {
	e.type = PTL_EVENT_DROPPED;
	e.event.tevent.start = NULL;
	PtlInternalEQPush(t->EQ, &e);
    }
    (void)PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_DROP_COUNT], 1);
    return 0;			       // silent ack
}
