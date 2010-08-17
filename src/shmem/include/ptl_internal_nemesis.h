#ifndef PTL_INTERNAL_NEMESIS_H
#define PTL_INTERNAL_NEMESIS_H

/* System headers */
#include <pthread.h>		       /* for pthread_*_t */
#include <stdint.h>		       /* for uint32_t */

/* Internal headers */
#include "ptl_internal_atomic.h"
#include "ptl_internal_commpad.h"

typedef struct {
    void *volatile next;
    char data[];
} NEMESIS_entry;

typedef struct {
    void *volatile head;
    void *volatile tail;
} NEMESIS_queue;

typedef struct {
    NEMESIS_queue q;
    volatile uint32_t frustration;
    pthread_cond_t trigger;
    pthread_mutex_t trigger_lock;
} NEMESIS_blocking_queue;

/***********************************************/

static inline void PtlInternalNEMESISInit(NEMESIS_queue *q)
{
    q->head = q->tail = NULL;
}

static inline void PtlInternalNEMESISEnqueue(
    NEMESIS_queue * restrict q,
    NEMESIS_entry * restrict f)
{
    NEMESIS_entry *prev =
	PtlInternalAtomicSwapPtr((void *volatile *)&(q->tail), f);
    if (prev == NULL) {
	q->head = f;
    } else {
	prev->next = f;
    }
}

static inline NEMESIS_entry *PtlInternalNEMESISDequeue(
    NEMESIS_queue * q)
{
    NEMESIS_entry *retval = q->head;
    if (retval != NULL) {
	if (retval->next != NULL) {
	    q->head = retval->next;
	    retval->next = NULL;
	} else {
	    NEMESIS_entry *old;
	    q->head = NULL;
	    old = PtlInternalAtomicCasPtr(&(q->tail), retval, NULL);
	    if (old != retval) {
		while (retval->next == NULL) ;
		q->head = retval->next;
	    }
	}
    }
    return retval;
}

#define OFF2PTR(off) (((intptr_t)off==0)?NULL:((NEMESIS_entry*)((intptr_t)comm_pad+(intptr_t)off)))
#define PTR2OFF(ptr) ((ptr == NULL)?0:((intptr_t)ptr-(intptr_t)comm_pad))

static inline void PtlInternalNEMESISOffsetEnqueue(
    NEMESIS_queue * restrict q,
    NEMESIS_entry * restrict f)
{
    void *offset_f = (void*)PTR2OFF(f);
    assert(f->next == NULL);
    intptr_t offset_prev =
	(intptr_t)PtlInternalAtomicSwapPtr((void *volatile *)&(q->tail), offset_f);
    if (offset_prev == 0) {
	q->head = offset_f;
    } else {
	OFF2PTR(offset_prev)->next = offset_f;
    }
}

static inline NEMESIS_entry *PtlInternalNEMESISOffsetDequeue(
    NEMESIS_queue * q)
{
    NEMESIS_entry *retval = OFF2PTR(q->head);
    if (retval != NULL) {
	if (retval->next != NULL) {
	    q->head = retval->next;
	    retval->next = NULL;
	} else {
	    intptr_t old;
	    q->head = NULL;
	    old = (intptr_t)PtlInternalAtomicCasPtr(&(q->tail), PTR2OFF(retval), NULL);
	    if (old != PTR2OFF(retval)) {
		while (retval->next == NULL) ;
		q->head = retval->next;
	    }
	}
    }
    return retval;
}


void PtlInternalNEMESISBlockingInit(
    NEMESIS_blocking_queue * q);
void PtlInternalNEMESISBlockingDestroy(
    NEMESIS_blocking_queue * q);
void PtlInternalNEMESISBlockingEnqueue(
    NEMESIS_blocking_queue * restrict q,
    NEMESIS_entry * restrict e);
NEMESIS_entry *PtlInternalNEMESISBlockingDequeue(
    NEMESIS_blocking_queue * q);
void PtlInternalNEMESISBlockingOffsetEnqueue(
    NEMESIS_blocking_queue * restrict q,
    NEMESIS_entry * restrict e);
NEMESIS_entry *PtlInternalNEMESISBlockingOffsetDequeue(
    NEMESIS_blocking_queue * q);

#endif
