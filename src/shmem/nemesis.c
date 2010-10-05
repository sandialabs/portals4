#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#ifdef HAVE_PTHREAD_SHMEM_LOCKS
# include <pthread.h>
#else
# include <unistd.h>
#endif

/* Internal headers */
#include "ptl_internal_assert.h"
#include "ptl_internal_nemesis.h"
#include "ptl_internal_atomic.h"
#include "ptl_visibility.h"

/* Fragment queueing uses the NEMESIS lock-free queue protocol from
 * http://www.mcs.anl.gov/~buntinas/papers/ccgrid06-nemesis.pdf
 * Note: it is NOT SAFE to use with multiple de-queuers, it is ONLY safe to use
 * with multiple enqueuers and a single de-queuer. */
void INTERNAL PtlInternalNEMESISBlockingInit(
    NEMESIS_blocking_queue * q)
{
    PtlInternalNEMESISInit(&q->q);
#ifdef HAVE_PTHREAD_SHMEM_LOCKS
    q->frustration = 0;
    {
	pthread_mutexattr_t ma;
	ptl_assert(pthread_mutexattr_init(&ma), 0);
	ptl_assert(pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED),
	       0);
	ptl_assert(pthread_mutex_init(&q->trigger_lock, &ma), 0);
	ptl_assert(pthread_mutexattr_destroy(&ma), 0);
    }
    {
	pthread_condattr_t ca;
	ptl_assert(pthread_condattr_init(&ca), 0);
	ptl_assert(pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED), 0);
	ptl_assert(pthread_cond_init(&q->trigger, &ca), 0);
	ptl_assert(pthread_condattr_destroy(&ca), 0);
    }
    //printf("init q=%p(%u)\n", q, (unsigned)((uintptr_t)q - (uintptr_t)comm_pad));
#else
    /* for the pipe to work, it has to be created by yod */
    //assert(pipe(q->pipe) == 0);
    /* I'm leaving open both ends of the pipe, so that I can both receive
     * messages AND send myself messages */
#endif
}

void INTERNAL PtlInternalNEMESISBlockingOffsetEnqueue(
    NEMESIS_blocking_queue * restrict q,
    NEMESIS_entry * restrict f)
{
    assert((f == (void *)1) || (f->next == NULL));
    PtlInternalNEMESISOffsetEnqueue(&q->q, f);
    /* awake waiter */
#ifdef HAVE_PTHREAD_SHMEM_LOCKS
    if (q->frustration) {
	ptl_assert(pthread_mutex_lock(&q->trigger_lock), 0);
	if (q->frustration) {
	    q->frustration = 0;
	    ptl_assert(pthread_cond_signal(&q->trigger), 0);
	}
	ptl_assert(pthread_mutex_unlock(&q->trigger_lock), 0);
    }
#else
    ptl_assert(write(q->pipe[1], "", 1), 1);
#endif
}

NEMESIS_entry INTERNAL *PtlInternalNEMESISBlockingOffsetDequeue(
    NEMESIS_blocking_queue * q)
{
#ifndef HAVE_PTHREAD_SHMEM_LOCKS
    char junk;
    ptl_assert(read(q->pipe[0], &junk, 1), 1);
#endif
    NEMESIS_entry *retval = PtlInternalNEMESISOffsetDequeue(&q->q);
    if (retval == NULL) {
	while (q->q.head == NULL) {
#ifdef HAVE_PTHREAD_SHMEM_LOCKS
	    if (PtlInternalAtomicInc(&q->frustration, 1) > 1000) {
		ptl_assert(pthread_mutex_lock(&q->trigger_lock), 0);
		if (q->frustration > 1000) {
		    ptl_assert(pthread_cond_wait(&q->trigger, &q->trigger_lock),
			   0);
		}
		ptl_assert(pthread_mutex_unlock(&q->trigger_lock), 0);
	    }
#else
	    ptl_assert(read(q->pipe[0], &junk, 1), 1);
#endif
	}
	retval = PtlInternalNEMESISOffsetDequeue(&q->q);
	assert(retval != NULL);
    }
    assert(retval);
    assert(retval == (void *)1 || retval->next == NULL);
    return retval;
}
