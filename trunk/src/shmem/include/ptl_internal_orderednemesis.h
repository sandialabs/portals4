#ifndef PTL_INTERNAL_ORDERED_NEMESIS_H
#define PTL_INTERNAL_ORDERED_NEMESIS_H

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

typedef struct ordered_NEMESIS_entry_s ordered_NEMESIS_entry;

typedef struct {
    volatile ordered_NEMESIS_entry *volatile ptr;
    volatile ptl_size_t                      val;
} ordered_NEMESIS_ptr;

struct ordered_NEMESIS_entry_s {
    ordered_NEMESIS_ptr volatile next;
    char                         data[];
};

typedef struct {
    /* The First Cacheline */
    ordered_NEMESIS_ptr head;
    ordered_NEMESIS_ptr tail;
    uint8_t             pad1[CACHELINE_WIDTH - (2 * sizeof(ordered_NEMESIS_ptr))];
    /* The Second Cacheline */
    ordered_NEMESIS_ptr shadow_head;
    uint8_t             pad2[CACHELINE_WIDTH - sizeof(ordered_NEMESIS_ptr)];
} ordered_NEMESIS_queue ALIGNED (CACHELINE_WIDTH);

/***********************************************/
static inline ordered_NEMESIS_ptr PtlInternalAtomicCas128(volatile ordered_NEMESIS_ptr *addr,
                                                          const ordered_NEMESIS_ptr     oldval,
                                                          const ordered_NEMESIS_ptr     newval)
{                                      /*{{{ */
#ifdef HAVE_CMPXCHG16B
    ordered_NEMESIS_ptr ret;
    assert(((uintptr_t)addr & 0xf) == 0);
    __asm__ __volatile__ ("lock cmpxchg16b %0\n\t"
                          : "+m" (*addr),
                          "=a" (ret.ptr),
                          "=d" (ret.val)
                          : "a"  (oldval.ptr),
                          "d"  (oldval.val),
                          "b"  (newval.ptr),
                          "c"  (newval.val)
                          : "cc",
                          "memory");
    return ret;

#else  /* ifdef HAVE_CMPXCHG16B */
# error No known 128-bit atomic CAS operations are available
#endif  /* ifdef HAVE_CMPXCHG16B */
}                                      /*}}} */

static inline void PtlInternalAtomicRead128(ordered_NEMESIS_ptr          *dest,
                                            volatile ordered_NEMESIS_ptr *src)
{                                      /*{{{ */
#ifdef HAVE_CMPXCHG16B
    __asm__ __volatile__ ("xor %%rax, %%rax\n\t"     // zero rax out to avoid affecting *addr
                          "xor %%rbx, %%rbx\n\t"     // zero rbx out to avoid affecting *addr
                          "xor %%rcx, %%rcx\n\t"     // zero rcx out to avoid affecting *addr
                          "xor %%rdx, %%rdx\n\t"     // zero rdx out to avoid affecting *addr
                          "lock cmpxchg16b (%2)\n\t" // atomic swap
                          "mov %%rax, %0\n\t"        // put rax into dest->success
                          "mov %%rdx, %1\n\t"        // put rdx into dest->failure
                          : "=m"   (dest->ptr),
                          "=m"     (dest->val)
                          : "r"    (src)
                          : "cc",
                          "rax",
                          "rbx",
                          "rcx",
                          "rdx");
#else /* ifdef HAVE_CMPXCHG16B */
# error No known 128-bit atomic read operations are available
#endif /* ifdef HAVE_CMPXCHG16B */
}                                      /*}}} */

static inline void PtlInternalAtomicWrite128(volatile ordered_NEMESIS_ptr *addr,
                                             const ordered_NEMESIS_ptr     newval)
{                                      /*{{{ */
#ifdef HAVE_CMPXCHG16B
    __asm__ __volatile__ ("1:\n\t"
                          "lock cmpxchg16b %0\n\t"
                          "jne 1b"
                          : "+m" (*addr)
                          : "a"  (addr->ptr),
                          "d"    (addr->val),
                          "b"    (newval.ptr),
                          "c"    (newval.val)
                          : "cc",
                          "memory");
#else /* ifdef HAVE_CMPXCHG16B */
# error No known 128-bit atomic write operations are available
#endif /* ifdef HAVE_CMPXCHG16B */
}                                      /*}}} */

static inline ordered_NEMESIS_ptr PtlInternalAtomicSwap128(volatile ordered_NEMESIS_ptr *addr,
                                                           const ordered_NEMESIS_ptr     newval)
{   /*{{{*/
    ordered_NEMESIS_ptr oldval = *addr;
    ordered_NEMESIS_ptr tmp;

    if (oldval.val > newval.val) {
        return oldval;
    }
    tmp = PtlInternalAtomicCas128(addr, oldval, newval);
    while (tmp.ptr != oldval.ptr || tmp.val != oldval.val) {
        oldval = tmp;
        if (tmp.val > newval.val) {
            break;
        }
        tmp = PtlInternalAtomicCas128(addr, oldval, newval);
    }
    return oldval;
} /*}}}*/

static inline void PtlInternalOrderedNEMESISInit(ordered_NEMESIS_queue *q)
{
    assert(sizeof(ordered_NEMESIS_ptr) == 16);
    q->head.ptr    = q->tail.ptr = NULL;
    q->head.val    = q->tail.val = 0;
    q->shadow_head = q->head;
}

static inline int PtlInternalOrderedNEMESISEnqueue(ordered_NEMESIS_queue *restrict q,
                                                   void                           *e,
                                                   ptl_size_t                      v)
{
    ordered_NEMESIS_ptr f = { .ptr = e, .val = v };

    assert(f.ptr->next.ptr == NULL);
    ordered_NEMESIS_ptr prev = PtlInternalAtomicSwap128(&(q->tail), f);

    /* Did the swap happen? */
    if (prev.val > f.val) {
        /* no */
        return 0;
    }

    if (prev.ptr == NULL) {
        PtlInternalAtomicWrite128(&q->head, f);
    } else {
        PtlInternalAtomicWrite128(&prev.ptr->next, f);
    }
    return 1;
}

static inline void *PtlInternalOrderedNEMESISDequeue(ordered_NEMESIS_queue *q,
                                                     ptl_size_t             upper_bound)
{
    ordered_NEMESIS_ptr       retval;
    const ordered_NEMESIS_ptr nil = { .ptr = NULL, .val = 0 };

    PtlInternalAtomicRead128(&retval, &q->head);
    if (retval.ptr != NULL) {
        if (retval.val > upper_bound) {
            return NULL;
        }
        if (retval.ptr->next.ptr != NULL) {
            PtlInternalAtomicWrite128(&q->head, retval.ptr->next);
            retval.ptr->next.ptr = NULL;
        } else {
            ordered_NEMESIS_ptr old;
            q->head.ptr = NULL;
            old         = PtlInternalAtomicCas128(&(q->tail), retval, nil);
            if ((old.ptr != retval.ptr) || (old.val != retval.val)) {
                while (retval.ptr->next.ptr == NULL) SPINLOCK_BODY();
                PtlInternalAtomicWrite128(&q->head, retval.ptr->next);
                retval.ptr->next.ptr = NULL;
            }
        }
        return (void*)(retval.ptr);
    } else {
        return NULL;
    }
}

#endif /* ifndef PTL_INTERNAL_NEMESIS_H */
/* vim:set expandtab: */
