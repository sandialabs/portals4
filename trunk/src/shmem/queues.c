#include <stdlib.h>                    /* for calloc() */
#include <stdint.h>                    /* for uintptr_t (C99) */

#include "ptl_visibility.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_queues.h"
#include "ptl_internal_atomic.h"

#define QCTR_MASK (15)
#define QPTR(x) ((volatile ptl_internal_qnode_t*)(((uintptr_t)(x))&~(uintptr_t)QCTR_MASK))
#define QCTR(x) (((uintptr_t)(x))&QCTR_MASK)
#define QCOMPOSE(x,y) (void*)(((uintptr_t)QPTR(x))|((QCTR(y)+1)&QCTR_MASK))

// This lock-free algorithm borrowed from qthreads, which borrowed it from
// http://www.research.ibm.com/people/m/michael/podc-1996.pdf

void INTERNAL PtlInternalQueueInit(
    ptl_internal_q_t * q)
{
    q->head = q->tail = calloc(1, sizeof(ptl_internal_qnode_t));
}

void INTERNAL PtlInternalQueueDestroy(
    ptl_internal_q_t * q)
{
    assert(q->head == q->tail);
    assert(q->head != NULL);
    free((void *)q->head);
    q->head = q->tail = NULL;
}

void INTERNAL PtlInternalQueueAppend(
    ptl_internal_q_t * q,
    void *t)
{
    volatile ptl_internal_qnode_t *tail;
    volatile ptl_internal_qnode_t *next;
    ptl_internal_qnode_t *node;

    assert(t != NULL);
    assert(q != NULL);

    node = malloc(sizeof(ptl_internal_qnode_t));
    assert(node != NULL);
    assert(QCTR(node) == 0);           // node MUST be aligned

    node->value = t;
    // set to NULL without disturbing the ctr
    node->next = (ptl_internal_qnode_t *) (uintptr_t) QCTR(node->next);

    while (1) {
        tail = q->tail;
        next = QPTR(tail)->next;
        if (tail == q->tail) {         // are tail and next consistent?
            if (QPTR(next) == NULL) {  // was tail pointing to the last node ?
                if (PtlInternalAtomicCasPtr
                    (&(QPTR(tail)->next), next, QCOMPOSE(node, next)) == next)
                    break;             // success!
            } else {
                (void)PtlInternalAtomicCasPtr(&(q->tail), tail,
                                              QCOMPOSE(next, tail));
            }
        }
    }
    (void)PtlInternalAtomicCasPtr(&(q->tail), tail, QCOMPOSE(node, tail));
}

void INTERNAL *PtlInternalQueuePop(
    ptl_internal_q_t * q)
{
    void *p;
    volatile ptl_internal_qnode_t *head;
    volatile ptl_internal_qnode_t *tail;
    volatile ptl_internal_qnode_t *next_ptr;

    assert(q != NULL);
    while (1) {
        head = q->head;
        tail = q->tail;
        next_ptr = QPTR(QPTR(head)->next);
        if (head == q->head) {         // are head, tail, and next consistent?
            if (head == tail) {        // is queue empty or tail falling behind?
                if (next_ptr == NULL) { // is queue empty?
                    return NULL;
                }
                (void)PtlInternalAtomicCasPtr(&(q->tail), tail,
                                              QCOMPOSE(next_ptr, tail));
            } else {                   // no need to deal with the tail
                // read value before CAS, otherwise another dequeue might free the next node
                p = next_ptr->value;
                if (PtlInternalAtomicCasPtr
                    (&(q->head), head, QCOMPOSE(next_ptr, head)) == head) {
                    break;             // success!
                }
            }
        }
    }
    free((void *)QPTR(head));
    return p;
}
/* vim:set expandtab */
