#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>

/* Internal headers */
#include "ptl_internal_nemesis.h"
#include "ptl_internal_atomic.h"
#include "ptl_visibility.h"

/* Fragment queueing uses the NEMESIS lock-free queue protocol from
 * http://www.mcs.anl.gov/~buntinas/papers/ccgrid06-nemesis.pdf
 * Note: it is NOT SAFE to use with multiple de-queuers, it is ONLY safe to use
 * with multiple enqueuers and a single de-queuer. */
void INTERNAL PtlInternalNEMESISBlockingInit(NEMESIS_blocking_queue *q)
{
    PtlInternalNEMESISInit(&q->q);
    q->frustration = 0;
    {
	pthread_mutexattr_t ma;
	assert(pthread_mutexattr_init(&ma) == 0);
	assert(pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED) == 0);
	assert(pthread_mutex_init(&q->trigger_lock, &ma) == 0);
	assert(pthread_mutexattr_destroy(&ma) == 0);
    }
    {
	pthread_condattr_t ca;
	assert(pthread_condattr_init(&ca) == 0);
	assert(pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED) == 0);
	assert(pthread_cond_init(&q->trigger, &ca) == 0);
	assert(pthread_condattr_destroy(&ca) == 0);
    }
}

void INTERNAL PtlInternalNEMESISBlockingDestroy(NEMESIS_blocking_queue *q)
{
    assert(pthread_cond_destroy(&q->trigger) == 0);
    assert(pthread_mutex_destroy(&q->trigger_lock) == 0);
}

void INTERNAL PtlInternalNEMESISBlockingEnqueue(
    NEMESIS_blocking_queue * restrict q,
    NEMESIS_entry * restrict f)
{
    PtlInternalNEMESISEnqueue(&q->q, f);
    /* awake waiter */
    if (q->frustration) {
	assert(pthread_mutex_lock(&q->trigger_lock) == 0);
	if (q->frustration) {
	    q->frustration = 0;
	    assert(pthread_cond_signal(&q->trigger) == 0);
	}
	assert(pthread_mutex_unlock(&q->trigger_lock) == 0);
    }
}

NEMESIS_entry INTERNAL *PtlInternalNEMESISBlockingDequeue(
    NEMESIS_blocking_queue * q)
{
    NEMESIS_entry *retval = PtlInternalNEMESISDequeue(&q->q);
    if (retval == NULL) {
	while (q->q.head == NULL) {
	    if (PtlInternalAtomicInc(&q->frustration, 1) > 1000) {
		assert(pthread_mutex_lock(&q->trigger_lock) == 0);
		//do { // this loop is unnecessary for only 1 dequeuer
		assert(pthread_cond_wait(&q->trigger, &q->trigger_lock) == 0);
		//} while (q->frustration > 1000);
		assert(pthread_mutex_unlock(&q->trigger_lock) == 0);
	    }
	}
	retval = PtlInternalNEMESISDequeue(&q->q);
	assert(retval != NULL);
    }
    return retval;
}

void INTERNAL PtlInternalNEMESISBlockingOffsetEnqueue(
    NEMESIS_blocking_queue * restrict q,
    NEMESIS_entry * restrict f)
{
    assert(f->next == NULL);
    PtlInternalNEMESISOffsetEnqueue(&q->q, f);
    /* awake waiter */
    if (q->frustration) {
	assert(pthread_mutex_lock(&q->trigger_lock) == 0);
	if (q->frustration) {
	    q->frustration = 0;
	    assert(pthread_cond_signal(&q->trigger) == 0);
	}
	assert(pthread_mutex_unlock(&q->trigger_lock) == 0);
    }
}

NEMESIS_entry INTERNAL *PtlInternalNEMESISBlockingOffsetDequeue(
    NEMESIS_blocking_queue * q)
{
    NEMESIS_entry *retval = PtlInternalNEMESISOffsetDequeue(&q->q);
    if (retval == NULL) {
	while (q->q.head == NULL) {
	    if (PtlInternalAtomicInc(&q->frustration, 1) > 1000) {
		assert(pthread_mutex_lock(&q->trigger_lock) == 0);
		//do { // this loop is unnecessary for only 1 dequeuer
		assert(pthread_cond_wait(&q->trigger, &q->trigger_lock) == 0);
		//} while (q->frustration > 1000);
		assert(pthread_mutex_unlock(&q->trigger_lock) == 0);
	    }
	}
	retval = PtlInternalNEMESISOffsetDequeue(&q->q);
	assert(retval != NULL);
    }
    return retval;
}
