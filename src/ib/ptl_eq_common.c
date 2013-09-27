/**
 * @file ptl_eq.h
 *
 * @brief Event queue implementation.
 */

#ifdef IS_LIGHT_LIB
#include <pthread.h>

#include "portals4.h"

#include "ptl_locks.h"
#include "ptl_sync.h"
#include "ptl_eq_common.h"
#include "ptl_sync.h"
#include "ptl_timer.h"

#else
#include "ptl_loc.h"
#include "ptl_timer.h"
#endif

extern atomic_t keep_polling;
/**
 * @brief Find whether a queue is empty.
 *
 * @param[in] eq the event queue
 *
 * @return non-zero if the queue is empty. The result is not
 * guaranteed unless the eq->lock is taken. However it is sufficient
 * to give an idea whether get_event() can be called. This avoids
 * taking a lock.
 */
static int inline is_queue_empty(struct eqe_list *eqe_list)
{
	return ((eqe_list->producer == eqe_list->consumer) &&
			(eqe_list->prod_gen == eqe_list->cons_gen));
}

/**
 * @brief Find next event in event queue.
 *
 * @param[in] eq the event queue
 * @param[out] event_p the address of the returned event
 *
 * @return PTL_EQ_EMPTY if there are no events in the queue
 * @return PTL_EQ_DROPPED if there was an event but there was a
 * gap since the last event returned
 * @return PTL_EQ_OK if there was an event and no gap
 */
static int get_event(struct eqe_list * restrict eqe_list, ptl_event_t * restrict event_p)
{
	int dropped = 0;

	PTL_FASTLOCK_LOCK(&eqe_list->lock);

	/* check to see if the queue is empty */
	if (is_queue_empty(eqe_list)) {
		PTL_FASTLOCK_UNLOCK(&eqe_list->lock);
		return PTL_EQ_EMPTY;
	}

	/* if we have been lapped by the producer advance the
	 * consumer pointer until we catch up */
	while (eqe_list->cons_gen < eqe_list->eqe[eqe_list->consumer].generation) {
		eqe_list->consumer++;
		if (eqe_list->consumer == eqe_list->count) {
			eqe_list->consumer = 0;
			eqe_list->cons_gen++;
		}
		dropped = 1;
		eqe_list->used --;
	}

	/* return the next valid event and update the consumer pointer */
	*event_p = eqe_list->eqe[eqe_list->consumer++].event;
	if (eqe_list->consumer >= eqe_list->count) {
		eqe_list->consumer = 0;
		eqe_list->cons_gen++;
	}

	eqe_list->used --;

	PTL_FASTLOCK_UNLOCK(&eqe_list->lock);

	return dropped ? PTL_EQ_DROPPED : PTL_OK;
}

/**
 * Do the work for PtlEQGet
 */
int PtlEQGet_work(struct eqe_list *eqe_list,
				  ptl_event_t *event_p)
{
	int err;

	err = get_event(eqe_list, event_p);

	return err;
}

static inline int check_eq(struct eqe_list *eqe_list, ptl_event_t *event_p)
{
	int err;

	if (!is_queue_empty(eqe_list)) {
		err = get_event(eqe_list, event_p);
		if (err != PTL_EQ_EMPTY)
			return err;
	}

	if (eqe_list->interrupt) {
		return PTL_INTERRUPTED;
	}

	return PTL_EQ_EMPTY;
}

/**
 * Do the work for PtlEQWait
 */
int PtlEQWait_work(struct eqe_list *eqe_list,
				   ptl_event_t *event_p)
{
	int err;
	atomic_inc(&keep_polling);
	atomic_inc(&eqe_list->waiter);

	while(1) {
		err = check_eq(eqe_list, event_p);
		if (err != PTL_EQ_EMPTY) {
			break;
		}
	
		sched_yield();


	}
	atomic_dec(&eqe_list->waiter);
	atomic_dec(&keep_polling);

	return err;
}

/**
 * Do the work for PtlEQPoll.
 */
int PtlEQPoll_work(struct eqe_list *eqe_list_in[], unsigned int size,
				   ptl_time_t timeout, ptl_event_t *event_p, unsigned int *which_p)
{
	int err;
	uint64_t nstart;
	uint64_t timeout_ns;
	TIMER_TYPE start;
	int i;
	const int forever = (timeout == PTL_TIME_FOREVER);

	/* compute expiration of poll time */
	MARK_TIMER(start);
	nstart = TIMER_INTS(start);

	timeout_ns = MILLI_TO_TIMER_INTS(timeout);
	atomic_inc(&keep_polling);

	while (1) {
		for (i = 0; i < size; i++) {
			struct eqe_list *eqe_list = eqe_list_in[i];

			if (!is_queue_empty(eqe_list)) {
				err = get_event(eqe_list, event_p);

				if (err != PTL_EQ_EMPTY) {
					*which_p = i;
					goto out;
				}
			}

			if (eqe_list->interrupt) {
				err = PTL_INTERRUPTED;
				goto out;
			}
		}

		if (!forever) {
			TIMER_TYPE tp;
			MARK_TIMER(tp);
			if ((TIMER_INTS(tp) - nstart) >= timeout_ns) {
			    err = PTL_EQ_EMPTY;
			    goto out;
			}
		}

		sched_yield();
	}

 out:
	atomic_dec(&keep_polling);
	return err;
}
