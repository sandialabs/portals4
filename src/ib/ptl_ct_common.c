#ifdef IS_LIGHT_LIB
#include <pthread.h>

#include "portals4.h"

#include "ptl_locks.h"
#include "ptl_ct_common.h"
#include "ptl_sync.h"
#include "ptl_timer.h"

#else
#include "ptl_loc.h"
#include "ptl_timer.h"
#endif

atomic_t keep_polling;
int PtlCTWait_work(struct ct_info *ct_info, uint64_t threshold,
				   ptl_ct_event_t *event_p)
{
	int err;
	atomic_inc(&keep_polling);

	/* wait loop */
	while (1) {
		/* check if wait condition satisfied */
		if (unlikely(ct_info->event.success >= threshold || ct_info->event.failure)) {
			*event_p = ct_info->event;
			err = PTL_OK;
			break;
		}

		/* someone called PtlCTFree or PtlNIFini, leave */
		if (unlikely(ct_info->interrupt)) {
			err = PTL_INTERRUPTED;
			break;
		}

		/* memory barrier */
		sched_yield();
	}
	atomic_dec(&keep_polling);

	return err;
}

/**
 * @brief Perform one trip around the polling loop
 *
 * @see PtlCTPoll
 *
 * @param size number of elements in the array
 * @param cts array of ct objects
 * @param thresholds array of thresholds
 * @param event_p address of returned event
 * @param which_p address of returned which
 *
 * @return PTL_OK if found an event
 * @return PTL_INTERRUPTED if someone is tearing down a ct
 * @return PTL_CT_NONE_REACHED if did not find an event
 */
static int ct_poll_loop(int size, struct ct_info *cts_info[], const ptl_size_t *thresholds,
			ptl_ct_event_t *event_p, unsigned int *which_p)
{
	int i;

	atomic_inc(&keep_polling);
	for (i = 0; i < size; i++) {
		const struct ct_info *ct_info = cts_info[i];

		if (ct_info->event.success >= thresholds[i] || ct_info->event.failure) {
			*event_p = ct_info->event;
			*which_p = i;
			atomic_dec(&keep_polling);
			return PTL_OK;
		}

		if (ct_info->interrupt) {
			atomic_dec(&keep_polling);
			return PTL_INTERRUPTED;
		}
	}

	atomic_dec(&keep_polling);
	return PTL_CT_NONE_REACHED;
}

int PtlCTPoll_work(struct ct_info *cts_info[], const ptl_size_t *thresholds,
				   unsigned int size, ptl_time_t timeout, ptl_ct_event_t *event_p,
				   unsigned int *which_p)
{
	int err;
	int have_timeout = (timeout != PTL_TIME_FOREVER);
	uint64_t timeout_ns;
	uint64_t nstart;
	TIMER_TYPE start;
	atomic_inc(&keep_polling);

	/* compute expiration of poll time */
	MARK_TIMER(start);
	nstart = TIMER_INTS(start);

	timeout_ns = MILLI_TO_TIMER_INTS(timeout);

	/* poll loop */
	while (1) {
		/* spin PTL_CT_POLL_LOOP_COUNT times */
		/* scan list to see if we can complete one */
		err = ct_poll_loop(size, cts_info, thresholds, event_p, which_p);
		if (err != PTL_CT_NONE_REACHED)
			break;

		/* check to see if we have timed out */
		if (have_timeout) {
		    TIMER_TYPE tp;
		    MARK_TIMER(tp);
		    if ((TIMER_INTS(tp) - nstart) >= timeout_ns) {
				err = PTL_CT_NONE_REACHED;
				break;
		    }
		}

		SPINLOCK_BODY();
	}

	atomic_dec(&keep_polling);
	return err;
}
