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
#include "ptl_internal_alignment.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_locks.h"

typedef struct {
    void *volatile next;
    char           data[];
} NEMESIS_entry;

typedef struct {
    /* The First Cacheline */
    void *volatile head;
    void *volatile tail;
    uint8_t        pad1[CACHELINE_WIDTH - (2 * sizeof(void *))];
    /* The Second Cacheline */
    void *volatile shadow_head;
    uint8_t        pad2[CACHELINE_WIDTH - sizeof(void *)];
} NEMESIS_queue ALIGNED (CACHELINE_WIDTH);

typedef struct {
    NEMESIS_queue q;
#ifndef USE_HARD_POLLING
# ifdef HAVE_PTHREAD_SHMEM_LOCKS
    volatile uint32_t frustration;
    pthread_cond_t    trigger;
    pthread_mutex_t   trigger_lock;
# else
    int               pipe[2];
# endif
#endif
} NEMESIS_blocking_queue;

/***********************************************/

static inline void PtlInternalNEMESISInit(NEMESIS_queue *q)
{
    q->head        = q->tail = NULL;
    q->shadow_head = NULL;
}

static inline void PtlInternalNEMESISEnqueue(NEMESIS_queue *restrict q,
                                             NEMESIS_entry *restrict f)
{
    NEMESIS_entry *prev =
        PtlInternalAtomicSwapPtr((void *volatile *)&(q->tail), f);

    if (prev == NULL) {
        q->head = f;
    } else {
        prev->next = f;
    }
}

static inline NEMESIS_entry *PtlInternalNEMESISDequeue(NEMESIS_queue *q)
{
    NEMESIS_entry *retval = q->head;

    if ((retval != NULL) && (retval != (void *)1)) {
        if (retval->next != NULL) {
            q->head      = retval->next;
            retval->next = NULL;
        } else {
            NEMESIS_entry *old;
            q->head = NULL;
            old     = PtlInternalAtomicCasPtr(&(q->tail), retval, NULL);
            if (old != retval) {
                while (retval->next == NULL) SPINLOCK_BODY();
                q->head      = retval->next;
                retval->next = NULL;
            }
        }
    }
    return retval;
}

#define OFF2PTR(off) (((uintptr_t)off ==                                    \
                       0) ? NULL : ((NEMESIS_entry *)((uintptr_t)comm_pad + \
                                                      (uintptr_t)off)))
#define PTR2OFF(ptr) ((ptr == NULL) ? 0 : ((uintptr_t)ptr - \
                                           (uintptr_t)comm_pad))

static inline int PtlInternalNEMESISOffsetEnqueue(NEMESIS_queue *restrict q,
                                                  NEMESIS_entry *restrict f)
{
    void *offset_f = (void *)PTR2OFF(f);

    assert(f != NULL && f->next == NULL);
    uintptr_t offset_prev =
        (uintptr_t)PtlInternalAtomicSwapPtr((void *volatile *)&(q->tail),
                                            offset_f);
    if (offset_prev == 0) {
        q->head = offset_f;
        return 0;
    } else {
        OFF2PTR(offset_prev)->next = offset_f;
    }
    return 1;
}

static inline NEMESIS_entry *PtlInternalNEMESISOffsetDequeue(NEMESIS_queue *q)
{
    if (!q->shadow_head) {
        if (!q->head) {
            return NULL;
        }
        q->shadow_head = q->head;
        q->head        = NULL;
    }
    NEMESIS_entry *retval = OFF2PTR(q->shadow_head);

    if (retval != NULL) {
        if (retval->next != NULL) {
            q->shadow_head = retval->next;
            retval->next   = NULL;
        } else {
            uintptr_t old;
            q->shadow_head = NULL;
            old            = (uintptr_t)PtlInternalAtomicCasPtr(&(q->tail), PTR2OFF(retval), NULL);
            if (old != PTR2OFF(retval)) {
                while (retval->next == NULL) {
                    __asm__ __volatile__ ("pause" ::: "memory");
                }
                q->shadow_head = retval->next;
                retval->next   = NULL;
            }
        }
    }
    return retval;
}

void           PtlInternalNEMESISBlockingInit(NEMESIS_blocking_queue *q);
NEMESIS_entry *PtlInternalNEMESISBlockingDequeue(NEMESIS_blocking_queue *q);
void           PtlInternalNEMESISBlockingOffsetEnqueue(NEMESIS_blocking_queue *restrict q,
                                                       NEMESIS_entry *restrict          e);
NEMESIS_entry *PtlInternalNEMESISBlockingOffsetDequeue(NEMESIS_blocking_queue *q);

#endif /* ifndef PTL_INTERNAL_NEMESIS_H */
/* vim:set expandtab: */
