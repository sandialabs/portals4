/**
 * @file ptl_ct.c
 *
 * This file implements the counting event class.
 */

#include "ptl_loc.h"

/* TODO make these unnecessary by queuing defered xi/xl's */
static void __ct_check(ct_t *ct);
static void __post_trig_ct(xl_t *xl, ct_t *trig_ct);
static void do_trig_ct_op(xl_t *xl);

/**
 * Initialize a ct object once when created.
 *
 * @param[in] arg opaque ct address
 * @param[in] unused unused
 */
int ct_init(void *arg, void *unused)
{
	ct_t *ct = arg;

	pthread_mutex_init(&ct->mutex, NULL);
	pthread_cond_init(&ct->cond, NULL);
	INIT_LIST_HEAD(&ct->xi_list);
	INIT_LIST_HEAD(&ct->xl_list);

	return PTL_OK;
}

/**
 * Cleanup a ct object once when destroyed.
 *
 * @param[in] arg opaque ct address
 */
void ct_fini(void *arg)
{
	ct_t *ct = arg;

	pthread_cond_destroy(&ct->cond);
	pthread_mutex_destroy(&ct->mutex);
}

/**
 * Initialize ct each time when allocated from free list.
 *
 * @param[in] arg opaque ct address
 *
 * @return status
 */
int ct_new(void *arg)
{
	ct_t *ct = arg;

	assert(list_empty(&ct->xi_list));
	assert(list_empty(&ct->xl_list));

	ct->waiters = 0;
	ct->interrupt = 0;
	ct->event.failure = 0;
	ct->event.success = 0;

	return PTL_OK;
}

/**
 * Cleanup ct each time when freed.
 *
 * Called when last reference to ct is dropped.
 *
 * @param[in] arg opaque ct address
 */
void ct_cleanup(void *arg)
{
	ct_t *ct = arg;

	ct->interrupt = 0;
}

/**
 * Allocate a new counting event.
 *
 * @post on success leaves holding one reference on the ct
 *
 * @param[in] ni_handle the handle of the ni for which to allocate ct
 * @param[out] ct_handle_p a pointer to the returned ct_handle
 *
 * @return status
 */
int PtlCTAlloc(ptl_handle_ni_t ni_handle, ptl_handle_ct_t *ct_handle_p)
{
	int err;
	ni_t *ni;
	ct_t *ct;

	/* convert handle to object */
	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_ni(ni_handle, &ni);
		if (err)
			goto err1;

		if (!ni) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	} else {
		ni = fast_to_obj(ni_handle);
	}

	/* check limit resources to see if we can allocate another ct */
	if (unlikely(__sync_add_and_fetch(&ni->current.max_cts, 1) >
	    ni->limits.max_cts)) {
		(void)__sync_fetch_and_sub(&ni->current.max_cts, 1);
		err = PTL_NO_SPACE;
		goto err2;
	}

	/* allocate new ct from free list */
	err = ct_alloc(ni, &ct);
	if (unlikely(err)) {
		(void)__sync_fetch_and_sub(&ni->current.max_cts, 1);
		goto err2;
	}

	/* add ourselves to the ni ct list
	 * this list is used by ni to interrupt
	 * any ct's that are in PtlCTWait at the time
	 * that PtlNIFini is called */
	pthread_spin_lock(&ni->ct_list_lock);
	list_add(&ct->list, &ni->ct_list);
	pthread_spin_unlock(&ni->ct_list_lock);

	*ct_handle_p = ct_to_handle(ct);

	err = PTL_OK;
err2:
	ni_put(ni);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Free a counting event.
 *
 * The PtlCTFree() function releases the resources associated with a
 * counting event. It is up to the user to ensure that no memory
 * descriptors or match list entries are associated with the counting
 * event once it is freed. On a successful return, the counting event
 * has been released and is ready to be reallocated. As a side-effect
 * of PtlCTFree(), any triggered operations waiting on the freed
 * counting event whose thresholds have not been met will be deleted.
 *
 * @param[in] ct_handle
 *
 * @return status
 */
int PtlCTFree(ptl_handle_ct_t ct_handle)
{
	int err;
	ct_t *ct;
	ni_t *ni;

	/* convert handle to pointer */
	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_ct(ct_handle, &ct);
		if (err)
			goto err1;

		if (!ct) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	} else {
		ct = fast_to_obj(ct_handle);
	}

	ni = obj_to_ni(ct);

	/* remove ourselves from ni->ct_list */
	pthread_spin_lock(&ni->ct_list_lock);
	list_del(&ct->list);
	pthread_spin_unlock(&ni->ct_list_lock);

	/* clean up pending operations */
	pthread_mutex_lock(&ct->mutex);

	ct->interrupt = 1;
	__ct_check(ct);

	pthread_mutex_unlock(&ct->mutex);

	ct_put(ct);
	ct_put(ct);

	/* give back the limit resource */
	(void)__sync_sub_and_fetch(&ni->current.max_cts, 1);

	err = PTL_OK;
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Return the current state of a counting event.
 *
 * PtlCTGet()must be as close to the speed of a simple variable access as
 * possible; hence, PtlCTGet() is not atomic relative to PtlCTSet() or
 * PtlCTInc() operations that occur in a separate thread and is undefined if
 * PtlCTFree() or PtlNIFini() is called during the execution of the function.
 *
 * @param[in] ct_handle the handle of the ct object
 * @param[out] event_p a pointer to the returned ct
 *
 * @return status
 */
int PtlCTGet(ptl_handle_ct_t ct_handle, ptl_ct_event_t *event_p)
{
	int err;
	ct_t *ct;

	/* convert handle to object */
	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_ct(ct_handle, &ct);
		if (err)
			goto err1;

		if (!ct) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	} else {
		ct = fast_to_obj(ct_handle);
	}

	*event_p = ct->event;

	err = PTL_OK;
	ct_put(ct);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Wait until counting event exceeds threshold or has a failure.
 *
 * Will spin and then block waiting for a pthreads condition.
 *
 * @param[in] ct_handle
 * @param[in] threshold
 * @param[out] event_p
 *
 * @return status
 */
int PtlCTWait(ptl_handle_ct_t ct_handle, uint64_t threshold,
	      ptl_ct_event_t *event_p)
{
	int err;
	ct_t *ct;
	int nloops = get_param(PTL_CT_WAIT_LOOP_COUNT);

	/* convert handle to object */
	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_ct(ct_handle, &ct);
		if (err)
			goto err1;

		if (!ct) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	} else {
		ct = fast_to_obj(ct_handle);
	}

	/* wait loop */
	while (1) {
		/* check if wait condition satisfied */
		if (ct->event.success >= threshold || ct->event.failure ) {
			*event_p = ct->event;
			err = PTL_OK;
			break;
		}
		
		/* someone called PtlCTFree or PtlNIFini, leave */
		if (ct->interrupt) {
			err = PTL_INTERRUPTED;
			break;
		}

		/* spin PTL_CT_WAIT_LOOP_COUNT times */
		if (nloops) {
			nloops--;
			/* memory barrier */
			SPINLOCK_BODY();
			continue;
		}

		/* block until someone changes the counting event or
		 * removes the ct event */
		pthread_mutex_lock(&ct->mutex);

		/* have to check condition once with the lock to
		 * synchronize with other threads */
		if (!ct->event.failure && !ct->interrupt &&
		    ct->event.success < threshold) {
			ct->waiters++;
			pthread_cond_wait(&ct->cond, &ct->mutex);
			ct->waiters--;
		}

		pthread_mutex_unlock(&ct->mutex);
	}

	ct_put(ct);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * perform one trip around the polling loop
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
static int ct_poll_loop(int size, const ct_t **cts, const ptl_size_t *thresholds,
			ptl_ct_event_t *event_p, unsigned int *which_p)
{
	int i;

	for (i = 0; i < size; i++) {
		const ct_t *ct = cts[i];

		if (ct->event.success >= thresholds[i] || ct->event.failure) {
			*event_p = ct->event;
			*which_p = i;
			return PTL_OK;
		}

		if (ct->interrupt) {
			return PTL_INTERRUPTED;
		}
	}

	return PTL_CT_NONE_REACHED;
}

/**
 * Wait until one of an array of ct events reaches threshold or has a failure.
 *
 * @param[in] ct_handles array of ct handles to poll
 * @param[in] thresholds array of thresholds to check
 * @param[in] size the size of the arrays
 * @param[in] timeout time in msec to poll or PTL_TIME_FOREVER
 * @param[out] event_p address of returned event
 * @param[out] which_p address of returned index in array
 *
 * @return status
 */
int PtlCTPoll(const ptl_handle_ct_t *ct_handles, const ptl_size_t *thresholds,
	      unsigned int size, ptl_time_t timeout, ptl_ct_event_t *event_p,
	      unsigned int *which_p)
{
	int err;
	ni_t *ni = NULL;
	ct_t **cts = NULL;
	int have_timeout = (timeout != PTL_TIME_FOREVER);
	struct timespec expire;
	struct timespec now;
	int i;
	int i2;
	int nloops = get_param(PTL_CT_POLL_LOOP_COUNT);

	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;
	}

	if (size == 0) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* TODO if usage allows can put a small array on
	 * the stack and only call malloc for larger sizes */
	cts = malloc(size*sizeof(*cts));
	if (!cts) {
		err = PTL_NO_SPACE;
		goto err1;
	}

	/* convert handles to pointers */
	if (check_param) {
		i2 = -1;
		for (i = 0; i < size; i++) {
			err = to_ct(ct_handles[i], &cts[i]);
			if (unlikely(err || !cts[i])) {
				err = PTL_ARG_INVALID;
				goto err2;
			}

			i2 = i;

			if (i == 0)
				ni = obj_to_ni(cts[0]);

			if (obj_to_ni(cts[i]) != ni) {
				err = PTL_ARG_INVALID;
				goto err2;
			}
		}
	} else {
		for (i = 0; i < size; i++)
			cts[i] = fast_to_obj(ct_handles[i]);
		i2 = size;
		ni = obj_to_ni(cts[0]);
	}

	/* compute expiration of poll time
	 * if forever just set to some time way in the future */
	clock_gettime(CLOCK_REALTIME, &now);
	expire.tv_nsec = now.tv_nsec + 1000000UL * timeout;
	expire.tv_sec = now.tv_sec + !have_timeout * 9999999UL;

	/* normalize */
	while (expire.tv_nsec >= 1000000000UL) {
		expire.tv_nsec -= 1000000000UL;
		expire.tv_sec++;
	}

	/* poll loop */
	while (1) {
		/* spin PTL_CT_POLL_LOOP_COUNT times */
		if (nloops) {
			/* scan list to see if we can complete one */
			err = ct_poll_loop(size, (const ct_t **)cts, thresholds, event_p, which_p);
			if (err != PTL_CT_NONE_REACHED)
				break;

			/* check to see if we have timed out */
			if (have_timeout) {
				clock_gettime(CLOCK_REALTIME, &now);
				if ((now.tv_sec > expire.tv_sec) ||
				    ((now.tv_sec == expire.tv_sec) &&
				     (now.tv_nsec > expire.tv_nsec))) {
					err = PTL_CT_NONE_REACHED;
					break;
				}
			}

			nloops--;
			SPINLOCK_BODY();
			continue;
		} else {
			/* block until someone changes *any* ct event
			 * or removes a ct event */
			pthread_mutex_lock(&ni->ct_wait_mutex);

			/* check condition while holding lock */
			err = ct_poll_loop(size, (const ct_t **)cts, thresholds, event_p, which_p);
			if (err != PTL_CT_NONE_REACHED) {
				pthread_mutex_unlock(&ni->ct_wait_mutex);
				break;
			}

			ni->ct_waiters++;
			err = pthread_cond_timedwait(&ni->ct_wait_cond,
						     &ni->ct_wait_mutex,
						     &expire);
			ni->ct_waiters--;

			pthread_mutex_unlock(&ni->ct_wait_mutex);

			/* check to see if we timed out */
			if (err == ETIMEDOUT) {
				err = PTL_CT_NONE_REACHED;
				break;
			}
		}
	}

err2:
	for (i = i2; i >= 0; i--)
		ct_put((void *)cts[i]);
	free(cts);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Check to see if current value of ct event will
 * trigger a further action.
 *
 * @pre caller must hold ct->mutex
 *
 * @param[in] ct
 */
static void __ct_check(ct_t *ct)
{
	struct list_head *l;
	struct list_head *t;
	ni_t *ni = obj_to_ni(ct);

	/* wake up callers to PtlCTWait on this ct if any */
	if (ct->waiters)
		pthread_cond_broadcast(&ct->cond);

	/* wake up callers to PollCTPoll if any */
	pthread_mutex_lock(&ni->ct_wait_mutex);
	if (ni->ct_waiters)
		pthread_cond_broadcast(&ni->ct_wait_cond);
	pthread_mutex_unlock(&ni->ct_wait_mutex);

	/* check to see if any pending triggered move operations
	 * can now be performed or discarded
	 * TODO this should enqueue the xi to a list and then unwind
	 * all the ct locks before calling process_init */
	list_for_each_prev_safe(l, t, &ct->xi_list) {
		xi_t *xi = list_entry(l, xi_t, list);
		if (ct->interrupt) {
			list_del(l);
			xi->state = STATE_INIT_CLEANUP;
			process_init(xi);
		} else if ((ct->event.success + ct->event.failure) >= xi->threshold) {
			list_del(l);
			process_init(xi);
		}
	}

	/* check to see if any pending triggered ct operations
	 * can now be performed or discarded
	 * TODO this should enqueue the xl to a list and then unwind
	 * all the ct locks before calling do_trig_ct_op */
	list_for_each_prev_safe(l, t, &ct->xl_list) {
		xl_t *xl = list_entry(l, xl_t, list);
		if (ct->interrupt) {
			list_del(l);
			ct_put(xl->ct);
			free(xl);
		} else if ((ct->event.success + ct->event.failure) >= xl->threshold) {
			list_del(l);
			pthread_mutex_unlock(&ct->mutex);
			do_trig_ct_op(xl);
			pthread_mutex_lock(&ct->mutex);
		}
	}
}

/**
 * Set counting event to new value.
 *
 * @pre caller should hold ct->mutex
 *
 * @param[in] ct
 * @param[in] new_ct
 */
static void __ct_set(ct_t *ct, ptl_ct_event_t new_ct)
{
	/* set new value */
	ct->event = new_ct;

	/* check to see if this triggers any further
	 * actions */
	__ct_check(ct);
}

/**
 * Set counting event to new value.
 *
 * @param[in] ct_handle
 * @param[in] new_ct
 *
 * @return status
 */
int PtlCTSet(ptl_handle_ct_t ct_handle, ptl_ct_event_t new_ct)
{
	int err;
	ct_t *ct;
	ni_t *ni;

	/* convert handle to object */
	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_ct(ct_handle, &ct);
		if (err)
			goto err1;

		if (!ct) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	} else {
		ct = fast_to_obj(ct_handle);
	}

	ni = obj_to_ni(ct);

	pthread_mutex_lock(&ct->mutex);

	__ct_set(ct, new_ct);

	pthread_mutex_unlock(&ct->mutex);

	err = PTL_OK;
	ct_put(ct);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Setup a triggered ct set operation.
 *
 * When the ct with handle trig_ct_handle reaches
 * threshold set the ct with handle ct_handle
 * to the value new_ct
 *
 * xl holds a reference to ct until it gets processed
 *
 * @param ct_handle the handle of the ct on which to
 * perform the triggered ct operation
 * @param new_ct the value to set
 * the counting event to
 * @param trig_ct_handle the handle of the
 * counting event that triggers the ct operation
 * @param threshold the threshold, when reached
 * by trig_ct, that causes the ct operation
 *
 * @return status
 */
int PtlTriggeredCTSet(ptl_handle_ct_t ct_handle, ptl_ct_event_t new_ct,
		      ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	ct_t *ct;
	ct_t *trig_ct;
	ni_t *ni;
	xl_t *xl;

	/* convert handles to objects */
	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_ct(trig_ct_handle, &trig_ct);
		if (err)
			goto err1;

		ni = obj_to_ni(trig_ct);

		err = to_ct(ct_handle, &ct);
		if (err)
			goto err2;

		if (ni != obj_to_ni(ct)) {
			err = PTL_ARG_INVALID;
			ct_put(ct);
			goto err2;
		}
	} else {
		trig_ct = fast_to_obj(trig_ct_handle);
		ct = fast_to_obj(ct_handle);
		ni = obj_to_ni(trig_ct);
	}

	/* get container for triggered ct op */
	xl = xl_alloc();
	if (!xl) {
		err = PTL_NO_SPACE;
		ct_put(ct);
		goto err2;
	}

	xl->op = TRIG_CT_SET;
	xl->ct = ct;
	xl->value = new_ct;
	xl->threshold = threshold;

	pthread_mutex_lock(&trig_ct->mutex);

	__post_trig_ct(xl, trig_ct);

	pthread_mutex_unlock(&trig_ct->mutex);

	err = PTL_OK;
err2:
	ct_put(trig_ct);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Increment counting event by value.
 *
 * @pre caller must hold ct->mutex
 *
 * @param[in] ct
 * @param[in] increment
 */
static void __ct_inc(ct_t *ct, ptl_ct_event_t increment)
{
	/* increment ct by value */
	ct->event.success += increment.success;
	ct->event.failure += increment.failure;

	/* check to see if this triggers any further
	 * actions */
	__ct_check(ct);
}

/**
 * Increment counting event.
 *
 * @param[in] ct_handle
 * @param[in] increment
 *
 * @return status
 */
int PtlCTInc(ptl_handle_ct_t ct_handle, ptl_ct_event_t increment)
{
	int err;
	ct_t *ct;
	ni_t *ni;

	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_ct(ct_handle, &ct);
		if (err)
			goto err1;

		if (!ct) {
			err = PTL_ARG_INVALID;
			goto err1;
		}

		if (increment.failure && increment.success) {
			err = PTL_ARG_INVALID;
			goto err2;
		}
	} else {
		ct = fast_to_obj(ct_handle);
	}

	ni = obj_to_ni(ct);

	pthread_mutex_lock(&ct->mutex);

	__ct_inc(ct, increment);

	pthread_mutex_unlock(&ct->mutex);

	err = PTL_OK;
err2:
	ct_put(ct);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Setup a triggered ct increment operation.
 *
 * When the ct with handle trig_ct_handle reaches
 * threshold increment the ct with handle ct_handle
 * by the amount increment.
 *
 * xl holds a reference to ct until it gets processed
 *
 * @param ct_handle the handle of the ct on which to
 * perform the triggered ct operation
 * @param increment the amount to increment
 * the counting event by
 * @param trig_ct_handle the handle of the
 * counting event that triggers the ct operation
 * @param threshold the threshold, when reached
 * by trig_ct, that causes the ct operation
 *
 * @return status
 */
int PtlTriggeredCTInc(ptl_handle_ct_t ct_handle, ptl_ct_event_t increment,
		      ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	ct_t *ct;
	ct_t *trig_ct;
	ni_t *ni;
	xl_t *xl;

	if (check_param) {
		err = get_gbl();
		if (unlikely(err))
			goto err0;

		err = to_ct(trig_ct_handle, &trig_ct);
		if (unlikely(err))
			goto err1;

		ni = obj_to_ni(trig_ct);

		err = to_ct(ct_handle, &ct);
		if (err)
			goto err2;

		if (ni != obj_to_ni(ct)) {
			err = PTL_ARG_INVALID;
			ct_put(ct);
			goto err2;
		}

		if (increment.failure && increment.success) {
			err = PTL_ARG_INVALID;
			goto err2;
		}
	} else {
		ct = fast_to_obj(ct_handle);
		trig_ct = fast_to_obj(trig_ct_handle);
		ni = obj_to_ni(trig_ct);
	}

	xl = xl_alloc();
	if (!xl) {
		err = PTL_NO_SPACE;
		ct_put(ct);
		goto err2;
	}

	xl->op = TRIG_CT_INC;
	xl->ct = ct;
	xl->value = increment;
	xl->threshold = threshold;

	pthread_mutex_lock(&trig_ct->mutex);

	__post_trig_ct(xl, trig_ct);

	pthread_mutex_unlock(&trig_ct->mutex);

	err = PTL_OK;
err2:
	ct_put(trig_ct);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Cancel any pending triggered move or ct operations.
 *
 * @param[in] ct_handle the handle of the ct to cancel
 *
 * @return status
 */
int PtlCTCancelTriggered(ptl_handle_ct_t ct_handle)
{
	int err;
	ct_t *ct;
	ni_t *ni;
	struct list_head *l;
	struct list_head *t;

	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_ct(ct_handle, &ct);
		if (err)
			goto err1;

		if (!ct) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	} else {
		ct = fast_to_obj(ct_handle);
	}

	ni = obj_to_ni(ct);

	pthread_mutex_lock(&ct->mutex);

	list_for_each_prev_safe(l, t, &ct->xi_list) {
		xi_t *xi = list_entry(l, xi_t, list);
		list_del(l);
		xi->state = STATE_INIT_CLEANUP;
		process_init(xi);
	}

	list_for_each_prev_safe(l, t, &ct->xl_list) {
		xl_t *xl = list_entry(l, xl_t, list);
		list_del(l);
		ct_put(xl->ct);
		free(xl);
	}

	pthread_mutex_unlock(&ct->mutex);

	err = PTL_OK;
	ct_put(ct);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Perform triggered ct operation.
 *
 * @param[in] xl
 */
static void do_trig_ct_op(xl_t *xl)
{
	ct_t *ct = xl->ct;

	/* we're a zombie */
	if (ct->interrupt)
		goto done;

	pthread_mutex_lock(&xl->ct->mutex);

	switch(xl->op) {
	case TRIG_CT_SET:
		__ct_set(ct, xl->value);
		break;
	case TRIG_CT_INC:
		__ct_inc(ct, xl->value);
		break;
	}

	pthread_mutex_unlock(&xl->ct->mutex);

done:
	ct_put(xl->ct);
	free(xl);
}

/**
 * Add a operation to list of pending triggered operations or
 * go ahead and do it if threshold has been met.
 *
 * @param[in] xi
 * @param[in] ct
 */
void post_ct(xi_t *xi, ct_t *ct)
{
	pthread_mutex_lock(&ct->mutex);
	if ((ct->event.success + ct->event.failure) >= xi->threshold) {
		pthread_mutex_unlock(&ct->mutex);
		process_init(xi);
		return;
	}
	list_add(&xi->list, &ct->xi_list);
	pthread_mutex_unlock(&ct->mutex);
}

/**
 * Add a triggered ct operation to a list or
 * do it if threshold has already been reached.
 *
 * @pre caller should hold trig_ct->mutex
 *
 * @param[in] xl
 * @param[in] trig_ct
 */
static void __post_trig_ct(xl_t *xl, ct_t *trig_ct)
{
	if ((trig_ct->event.failure + trig_ct->event.success) >=
	    xl->threshold) {
		/* TODO enqueue xl somewhere */
		pthread_mutex_unlock(&trig_ct->mutex);
		do_trig_ct_op(xl);
		pthread_mutex_lock(&trig_ct->mutex);
	} else {
		list_add(&xl->list, &trig_ct->xl_list);
	}
}

/**
 * Update a counting event.
 *
 * @param[in] ct
 * @param[in] ni_fail
 * @maram[in] length
 * @maram[in] bytes
 */
void make_ct_event(ct_t *ct, ptl_ni_fail_t ni_fail, ptl_size_t length,
		   int bytes)
{
	pthread_mutex_lock(&ct->mutex);

	if (ni_fail)
		ct->event.failure++;
	else
		ct->event.success += bytes ? length : 1;

	__ct_check(ct);

	pthread_mutex_unlock(&ct->mutex);
}
