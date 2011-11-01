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
#include "ptl_internal_locks.h"
#include "ptl_internal_shm.h"

typedef struct {
    void *next;
    char  data[];
} NEMESIS_entry;

typedef union {
    void    *p;
    uint64_t u;
    struct {
        uint32_t pid;
        uint32_t off;
    } s;
} ptl_offset_t;

typedef struct {
    /* The First Cacheline */
    void   *head;
    void   *tail;
    uint8_t pad1[CACHELINE_WIDTH - (2 * sizeof(void *))];
    /* The Second Cacheline */
    void   *shadow_head;
    uint8_t pad2[CACHELINE_WIDTH - sizeof(void *)];
} NEMESIS_queue ALIGNED (CACHELINE_WIDTH);

typedef struct {
    NEMESIS_queue q;
#ifndef USE_HARD_POLLING
# ifdef HAVE_PTHREAD_SHMEM_LOCKS
    uint32_t        frustration;
    pthread_cond_t  trigger;
    pthread_mutex_t trigger_lock;
# else
    int             pipe[2];
# endif
#endif
} NEMESIS_blocking_queue;

/***********************************************/

static inline void PtlInternalNEMESISInit(NEMESIS_queue *q)
{
    q->head        = NULL;
    q->tail        = NULL;
    q->shadow_head = NULL;
}

static inline void PtlInternalNEMESISEnqueue(NEMESIS_queue *restrict q,
                                             NEMESIS_entry *restrict f)
{
    NEMESIS_entry *prev = PtlInternalAtomicSwapPtr((void **)&(q->tail), f);

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

extern struct rank_comm_pad *comm_pads[PTL_PID_MAX];

static inline NEMESIS_entry *PtlInternalNEMESISGetPtr(ptl_offset_t off)
{
    if (off.u == 0) { return NULL; }
    if (comm_pads[off.s.pid] == NULL) { PtlInternalMapInPid(off.s.pid); }
    return (NEMESIS_entry *)((uintptr_t)comm_pads[off.s.pid] + off.s.off);
}

#define PTR2OFF(src, ptr) ((ptr == NULL) ?          \
                           ((ptl_offset_t) { 0 }) : \
                           ((ptl_offset_t) { .s.pid = src, .s.off = ((uint64_t)ptr - (uint64_t)comm_pads[src]) }))

extern ptl_pid_t proc_number;

static inline int PtlInternalNEMESISOffsetEnqueue(NEMESIS_queue *restrict q,
                                                  ptl_offset_t            entry)
{
    ptl_offset_t prev_entry;

    prev_entry.u = (uint64_t)PtlInternalAtomicSwap64((uint64_t *)&(q->tail), entry.u);
    if (prev_entry.u == 0) {
        q->head = entry.p;
        return 0;
    } else {
        PtlInternalNEMESISGetPtr(prev_entry)->next = entry.p;
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
    ptl_offset_t   ret_off = (ptl_offset_t)q->shadow_head;
    NEMESIS_entry *retval  = PtlInternalNEMESISGetPtr(ret_off);

    if (retval != NULL) {
        if (retval->next != NULL) {
            q->shadow_head = retval->next;
            retval->next   = NULL;
        } else {
            uintptr_t old;
            q->shadow_head = NULL;
            old            = (uintptr_t)PtlInternalAtomicCasPtr(&(q->tail), ret_off.p, NULL);
            if (old != ret_off.u) {
                while (retval->next == NULL) SPINLOCK_BODY();
                q->shadow_head = retval->next;
                retval->next   = NULL;
            }
        }
    }
    return retval;
}

void           PtlInternalNEMESISBlockingInit(NEMESIS_blocking_queue *q);
NEMESIS_entry *PtlInternalNEMESISBlockingDequeue(NEMESIS_blocking_queue *q);
void INTERNAL  PtlInternalNEMESISBlockingOffsetEnqueue(struct rank_comm_pad *restrict dest,
                                                       ptl_pid_t                      src_pid,
                                                       NEMESIS_entry *restrict        f);
NEMESIS_entry *PtlInternalNEMESISBlockingOffsetDequeue(NEMESIS_blocking_queue *q);

#endif /* ifndef PTL_INTERNAL_NEMESIS_H */
/* vim:set expandtab: */
