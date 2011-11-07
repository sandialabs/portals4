#include "ptl_loc.h"

#define OFF2PTR(ni,off) ((off) == 0 ?									\
						 NULL :											\
						 (buf_t *)((unsigned char *)(ni)->shmem.comm_pad + (off)))

#define PTR2OFF(ni,ptr) ((ptr) == NULL ?								\
						 0 : 											\
						 ((unsigned char *)(ptr) - (unsigned char *)(ni)->shmem.comm_pad))

static void PtlInternalNEMESISInit(NEMESIS_queue *q)
{
    q->head = 0;
	q->tail = 0;
	q->shadow_head = 0;
}

/* Fragment queueing uses the NEMESIS lock-free queue protocol from
 * http://www.mcs.anl.gov/~buntinas/papers/ccgrid06-nemesis.pdf
 * Note: it is NOT SAFE to use with multiple de-queuers, it is ONLY safe to use
 * with multiple enqueuers and a single de-queuer. */
static void PtlInternalNEMESISBlockingInit(ni_t *ni)
{
	pthread_mutexattr_t ma;
	pthread_condattr_t ca;

    PtlInternalNEMESISInit(&ni->shmem.receiveQ->q);

    atomic_set(&ni->shmem.receiveQ->frustration, 0);

	ptl_assert(pthread_mutexattr_init(&ma), 0);
	ptl_assert(pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED), 0);
	ptl_assert(pthread_mutex_init(&ni->shmem.receiveQ->trigger_lock, &ma), 0);
	ptl_assert(pthread_mutexattr_destroy(&ma), 0);

	ptl_assert(pthread_condattr_init(&ca), 0);
	ptl_assert(pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED), 0);
	ptl_assert(pthread_cond_init(&ni->shmem.receiveQ->trigger, &ca), 0);
	ptl_assert(pthread_condattr_destroy(&ca), 0);
}

static void PtlInternalNEMESISOffsetEnqueue(ni_t *ni,
											NEMESIS_queue *restrict q,
											buf_t *restrict f)
{
    unsigned long offset_f = PTR2OFF(ni,f);

    assert(f != NULL && f->obj.next == NULL);

    unsigned long offset_prev =
        (uintptr_t)atomic_swap_ptr((void **)(uintptr_t)&(q->tail),
								   (void *)(uintptr_t)offset_f);
    if (offset_prev == 0) {
		/* Queue was empty. */
        q->head = offset_f;
    } else {
		/* Link last object in queue to the new one. */
        OFF2PTR(ni,offset_prev)->obj.next = (void *)offset_f;
    }
}

static buf_t *PtlInternalNEMESISOffsetDequeue(ni_t *ni, NEMESIS_queue *q)
{
    buf_t *retval;

    if (!q->shadow_head) {
        if (!q->head) {
            return NULL;
        }
        q->shadow_head = q->head;
        q->head        = 0;
    }

    retval = OFF2PTR(ni,q->shadow_head);

    if (retval != NULL) {
        if (retval->obj.next != NULL) {
            q->shadow_head = (uintptr_t)retval->obj.next;
            retval->obj.next   = NULL;
        } else {
            unsigned long old;

            q->shadow_head = 0;
            old = (uintptr_t)__sync_val_compare_and_swap(&(q->tail), PTR2OFF(ni,retval), 0);
            if (old != PTR2OFF(ni,retval)) {
                while (retval->obj.next == NULL) {
                    __asm__ __volatile__ ("pause" ::: "memory");
                }
                q->shadow_head = (uintptr_t)retval->obj.next;
                retval->obj.next = NULL;
            }
        }
    }

    return retval;
}

static void PtlInternalNEMESISBlockingOffsetEnqueue(ni_t *ni, 
													NEMESIS_blocking_queue *restrict q,
													buf_t *f)
{
    assert(f->obj.next == NULL);
    PtlInternalNEMESISOffsetEnqueue(ni, &q->q, f);

#ifdef USE_HARD_POLLING
#else
    /* awake waiter */
	__sync_synchronize();
    if (atomic_read(&q->frustration)) {
        ptl_assert(pthread_mutex_lock(&q->trigger_lock), 0);
        if (atomic_read(&q->frustration)) {
            atomic_set(&q->frustration, 0);
            ptl_assert(pthread_cond_signal(&q->trigger), 0);
        }
        ptl_assert(pthread_mutex_unlock(&q->trigger_lock), 0);
    }
#endif
}

#if 0
static buf_t *PtlInternalNEMESISBlockingOffsetDequeue(ni_t *ni, NEMESIS_blocking_queue *q)
{
    buf_t *retval = PtlInternalNEMESISOffsetDequeue(ni, &q->q);

    if (retval == NULL) {
        while (q->q.shadow_head == 0 && q->q.head == 0) {
#ifdef USE_HARD_POLLING
            SPINLOCK_BODY();
#else
            if (atomic_inc(&q->frustration) > 1000) {
                ptl_assert(pthread_mutex_lock(&q->trigger_lock), 0);
                if (atomic_read(&q->frustration) > 1000) {
                    ptl_assert(pthread_cond_wait
							   (&q->trigger, &q->trigger_lock), 0);
                }
                ptl_assert(pthread_mutex_unlock(&q->trigger_lock), 0);
            }
#endif
        }
        retval = PtlInternalNEMESISOffsetDequeue(ni, &q->q);
        assert(retval != NULL);
    }
    assert(retval);
    assert(retval->obj.next == NULL);
    return retval;
}
#endif

#define C_VALIDPTR(x) assert(((uintptr_t)(x)) >= (uintptr_t)ni->shmem.comm_pad && \
                             ((uintptr_t)(x)) < ((uintptr_t)ni->shmem.comm_pad + ni->shmem.per_proc_comm_buf_size * (ni->shmem.world_size + 1)))

void PtlInternalFragmentSetup(ni_t *ni)
{
    /* first, initialize the receive queue */
    ni->shmem.receiveQ = (NEMESIS_blocking_queue *)(ni->shmem.comm_pad + pagesize + (ni->shmem.per_proc_comm_buf_size * ni->shmem.index));
    PtlInternalNEMESISBlockingInit(ni);
}

/* this enqueues a fragment in the specified receive queue */
void PtlInternalFragmentToss(ni_t *ni,
							 buf_t *buf,
							 ptl_pid_t dest)
{             
    NEMESIS_blocking_queue *destQ =
        (NEMESIS_blocking_queue *)(ni->shmem.comm_pad + pagesize +
                                   (ni->shmem.per_proc_comm_buf_size * dest));

    C_VALIDPTR(buf);
	buf->obj.next = NULL;
    PtlInternalNEMESISBlockingOffsetEnqueue(ni, destQ, buf);
}

/* this dequeues a fragment from my receive queue */
buf_t *PtlInternalFragmentReceive(ni_t *ni)
{                              
#if 0
    buf_t *buf = PtlInternalNEMESISBlockingOffsetDequeue(ni, ni->shmem.receiveQ);

    assert(buf->obj.next == NULL);
    C_VALIDPTR(buf);
#else

    buf_t *buf = PtlInternalNEMESISOffsetDequeue(ni, &ni->shmem.receiveQ->q);

	if (buf) {
		assert(buf->obj.next == NULL);
		C_VALIDPTR(buf);
	}	
#endif

	return buf;
}

/* vim:set expandtab: */
