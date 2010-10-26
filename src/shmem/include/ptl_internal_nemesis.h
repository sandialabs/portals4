#ifndef PTL_INTERNAL_NEMESIS_H
#define PTL_INTERNAL_NEMESIS_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#ifdef HAVE_PTHREAD_SHMEM_LOCKS
# include <pthread.h>                  /* for pthread_*_t */
#endif
#include <stdint.h>                    /* for uint32_t */

/* Internal headers */
#include "ptl_internal_assert.h"
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
#ifndef USE_HARD_POLLING
# ifdef HAVE_PTHREAD_SHMEM_LOCKS
    volatile uint32_t frustration;
    pthread_cond_t trigger;
    pthread_mutex_t trigger_lock;
# else
    int pipe[2];
# endif
#endif
} NEMESIS_blocking_queue;

/***********************************************/

static inline void PtlInternalNEMESISInit(
    NEMESIS_queue * q)
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
    if (retval != NULL && retval != (void *)1) {
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
                retval->next = NULL;
            }
        }
    }
    return retval;
}

#define OFF2PTR(off) (((intptr_t)off==0)?NULL:((NEMESIS_entry*)((intptr_t)comm_pad+(intptr_t)off)))
#define PTR2OFF(ptr) ((ptr == NULL)?0:((intptr_t)ptr-(intptr_t)comm_pad))

static inline int PtlInternalNEMESISOffsetEnqueue(
    NEMESIS_queue * restrict q,
    NEMESIS_entry * restrict f)
{
    void *offset_f = (void *)PTR2OFF(f);
    assert(f == (void *)1 || f->next == NULL);
    intptr_t offset_prev =
        (intptr_t) PtlInternalAtomicSwapPtr((void *volatile *)&(q->tail),
                                            offset_f);
    if (offset_prev == 0) {
        q->head = offset_f;
        return 0;
    } else if (offset_prev > 0) {
        /* less than zero is (almost certainly) the termination sigil;
         * we CANNOT lose the termination sigil, but we also cannot dereference it. */
        OFF2PTR(offset_prev)->next = offset_f;
    }
    return 1;
}

static inline NEMESIS_entry *PtlInternalNEMESISOffsetDequeue(
    NEMESIS_queue * q)
{
    NEMESIS_entry *retval = OFF2PTR(q->head);
    if (retval != NULL && retval != (void *)1) {
        if (retval->next != NULL) {
            q->head = retval->next;
            retval->next = NULL;
        } else {
            intptr_t old;
            q->head = NULL;
            old =
                (intptr_t) PtlInternalAtomicCasPtr(&(q->tail),
                                                   PTR2OFF(retval), NULL);
            if (old != PTR2OFF(retval)) {
                while (retval->next == NULL) ;
                q->head = retval->next;
                retval->next = NULL;
            }
        }
    }
    return retval;
}


void PtlInternalNEMESISBlockingInit(
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
