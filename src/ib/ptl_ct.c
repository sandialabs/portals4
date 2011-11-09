/**
 * @file ptl_ct.c
 *
 * @brief This file implements the counting event class.
 */

#include "ptl_loc.h"

/* TODO make these unnecessary by queuing deferred operations */
static void __ct_check(ct_t *ct);
static void __post_trig_ct(struct buf *buf, ct_t *trig_ct);
static void do_trig_ct_op(struct buf *buf);

/**
 * @brief Initialize a ct object once when created.
 *
 * @param[in] arg opaque ct address
 * @param[in] unused unused
 */
int ct_init(void *arg, void *unused)
{
	ct_t *ct = arg;

	pthread_mutex_init(&ct->mutex, NULL);
	pthread_cond_init(&ct->cond, NULL);
	INIT_LIST_HEAD(&ct->trig_list);

	return PTL_OK;
}

/**
 * @brief Cleanup a ct object once when destroyed.
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
 * @brief Initialize ct each time when allocated from free list.
 *
 * @param[in] arg opaque ct address
 *
 * @return PTL_OK Indicates success.
 */
int ct_new(void *arg)
{
	ct_t *ct = arg;

	assert(list_empty(&ct->trig_list));

	ct->waiters = 0;
	ct->interrupt = 0;
	ct->event.failure = 0;
	ct->event.success = 0;

	return PTL_OK;
}

/**
 * @brief Cleanup ct each time when freed.
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
 * @brief Allocate a new counting event.
 *
 * @post on success leaves holding one reference on the ct
 *
 * @param[in] ni_handle the handle of the ni for which to allocate ct
 * @param[out] ct_handle_p a pointer to the returned ct_handle
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that an invalid argument
 * was passed.
 * @return PTL_NO_SPACE Indicates that there is insufficient
 * memory to allocate the counting event.
 */
int PtlCTAlloc(ptl_handle_ni_t ni_handle, ptl_handle_ct_t *ct_handle_p)
{
	int err;
	ni_t *ni;
	ct_t *ct;

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ni(ni_handle, &ni);
	if (err)
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ni = fast_to_obj(ni_handle);
#endif

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
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Free a counting event.
 *
 * The PtlCTFree() function releases the resources associated with a
 * counting event. It is up to the user to ensure that no memory
 * descriptors or match list entries are associated with the counting
 * event once it is freed. On a successful return, the counting event
 * has been released and is ready to be reallocated. As a side-effect
 * of PtlCTFree(), any triggered operations waiting on the freed
 * counting event whose thresholds have not been met will be deleted.
 *
 * @param[in] ct_handle The handle of the counting event to free.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that ct_handle is not a valid
 * counting event handle.
 */
int PtlCTFree(ptl_handle_ct_t ct_handle)
{
	int err;
	ct_t *ct;
	ni_t *ni;

	/* convert handle to pointer */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = fast_to_obj(ct_handle);
#endif

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
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Cancel any pending triggered move or ct operations.
 *
 * @param[in] ct_handle the handle of the ct to cancel
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that ct_handle is not a valid
 * counting event handle.
 */
int PtlCTCancelTriggered(ptl_handle_ct_t ct_handle)
{
	int err;
	ct_t *ct;
	ni_t *ni;
	struct list_head *l;
	struct list_head *t;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = fast_to_obj(ct_handle);
#endif

	ni = obj_to_ni(ct);

	pthread_mutex_lock(&ct->mutex);

	list_for_each_prev_safe(l, t, &ct->trig_list) {
		buf_t *buf = list_entry(l, buf_t, list);
		list_del(l);
		if (buf->type == BUF_INIT) {
			buf->init_state = STATE_INIT_CLEANUP;
			process_init(buf);
		} else {
			assert(buf->type == BUF_TRIGGERED);
			ct_put(buf->ct);
			buf_put(buf);
		}
	}

	pthread_mutex_unlock(&ct->mutex);

	err = PTL_OK;
	ct_put(ct);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Return the current state of a counting event.
 *
 * PtlCTGet()must be as close to the speed of a simple variable access as
 * possible; hence, PtlCTGet() is not atomic relative to PtlCTSet() or
 * PtlCTInc() operations that occur in a separate thread and is undefined if
 * PtlCTFree() or PtlNIFini() is called during the execution of the function.
 *
 * @param[in] ct_handle the handle of the ct object
 * @param[out] event_p a pointer to the returned ct
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that ct_handle is not a valid
 * counting event handle.
 */
int PtlCTGet(ptl_handle_ct_t ct_handle, ptl_ct_event_t *event_p)
{
	int err;
	ct_t *ct;

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = fast_to_obj(ct_handle);
#endif

	*event_p = ct->event;

	err = PTL_OK;
	ct_put(ct);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Wait until counting event exceeds threshold or has a failure.
 *
 * Will spin and then block waiting for a pthreads condition.
 *
 * @param[in] ct_handle The handle of the counting event to wait on.
 * @param[in] threshold The threshold the event must reach.
 * @param[out] event_p The address of the returned event.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that ct_handle is not a valid
 * counting event handle.
 * @return PTL_INTERRUPTED Indicates that PtlCTFree() or PtlNIFini()
 * was called by another thread while this thread * was waiting in
 * PtlCTWait().
 */
int PtlCTWait(ptl_handle_ct_t ct_handle, uint64_t threshold,
	      ptl_ct_event_t *event_p)
{
	int err;
	ct_t *ct;
	int nloops = get_param(PTL_CT_WAIT_LOOP_COUNT);

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = fast_to_obj(ct_handle);
#endif

	/* wait loop */
	while (1) {
		/* check if wait condition satisfied */
		if (unlikely(ct->event.success >= threshold || ct->event.failure)) {
			*event_p = ct->event;
			err = PTL_OK;
			break;
		}
		
		/* someone called PtlCTFree or PtlNIFini, leave */
		if (unlikely(ct->interrupt)) {
			err = PTL_INTERRUPTED;
			break;
		}

		/* spin PTL_CT_WAIT_LOOP_COUNT times */
		if (likely(nloops)) {
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
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
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
 * @brief Wait until one of an array of ct events reaches threshold or has a failure.
 *
 * @param[in] ct_handles array of ct handles to poll
 * @param[in] thresholds array of thresholds to check
 * @param[in] size the size of the arrays
 * @param[in] timeout time in msec to poll or PTL_TIME_FOREVER
 * @param[out] event_p address of returned event
 * @param[out] which_p address of returned index in array
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates an invalid argument (e.g. a
 * bad ct_handle).
 * @return PTL_CT_NONE_REACHED Indicates that none of the counting
 * events reached their test before the timeout was * reached.
 * @return PTL_INTERRUPTED Indicates that PtlCTFree() or PtlNIFini()
 * was called by another thread while this thread * was waiting
 * in PtlCTPoll().
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

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;
#endif

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
#ifndef NO_ARG_VALIDATION
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
#else
	for (i = 0; i < size; i++)
		cts[i] = fast_to_obj(ct_handles[i]);
	i2 = size - 1;
	ni = obj_to_ni(cts[0]);
#endif

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

#ifndef NO_ARG_VALIDATION
err2:
#endif
	for (i = i2; i >= 0; i--)
		ct_put((void *)cts[i]);
	free(cts);
err1:
#ifndef NO_ARG_VALIDATION
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Check to see if current value of ct event will
 * trigger a further action.
 *
 * @pre caller should hold ct->mutex
 *
 * @param[in] ct The counting event to check.
 */
static void __ct_check(ct_t *ct)
{
	struct list_head *l;
	struct list_head *t;
	ni_t *ni = obj_to_ni(ct);

	/* wake up callers to PtlCTWait on this ct if any */
	if (ct->waiters)
		pthread_cond_broadcast(&ct->cond);

	/* wake up callers to PtlCTPoll if any */
	pthread_mutex_lock(&ni->ct_wait_mutex);
	if (ni->ct_waiters)
		pthread_cond_broadcast(&ni->ct_wait_cond);
	pthread_mutex_unlock(&ni->ct_wait_mutex);

	/* check to see if any pending triggered move operations
	 * can now be performed or discarded
	 * TODO this should enqueue the xi to a list and then unwind
	 * all the ct locks before calling process_init */
	list_for_each_prev_safe(l, t, &ct->trig_list) {
		buf_t *buf = list_entry(l, buf_t, list);

		if (buf->type == BUF_INIT) {
			if (ct->interrupt) {
				list_del(l);
				buf->init_state = STATE_INIT_CLEANUP;
				process_init(buf);
			} else if ((ct->event.success + ct->event.failure) >= buf->ct_threshold) {
				list_del(l);
				process_init(buf);
			}
		} else {
			assert(buf->type == BUF_TRIGGERED);
			if (ct->interrupt) {
				list_del(l);
				ct_put(buf->ct);
				buf_put(buf);
			} else if ((ct->event.success + ct->event.failure) >= buf->threshold) {
				list_del(l);
				pthread_mutex_unlock(&ct->mutex);
				do_trig_ct_op(buf);
				pthread_mutex_lock(&ct->mutex);
			}
		}
	}
}

/**
 * @brief Set counting event to new value.
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
 * @brief Set counting event to new value.
 *
 * @param[in] ct_handle The handle of the counting event to set.
 * @param[in] new_ct The new value to set the event to.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that ct_handle is not a valid
 * counting event handle.
 */
int PtlCTSet(ptl_handle_ct_t ct_handle, ptl_ct_event_t new_ct)
{
	int err;
	ct_t *ct;
	ni_t *ni;

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = fast_to_obj(ct_handle);
#endif

	ni = obj_to_ni(ct);

	pthread_mutex_lock(&ct->mutex);

	__ct_set(ct, new_ct);

	pthread_mutex_unlock(&ct->mutex);

	err = PTL_OK;
	ct_put(ct);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Increment counting event by value.
 *
 * @pre caller should hold ct->mutex
 *
 * @param[in] ct The counting event to increment.
 * @param[in] increment The increment value.
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
 * @brief Increment counting event.
 *
 * @param[in] ct_handle The handle of the countin event to increment.
 * @param[in] increment The increment value.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that ct_handle is not a valid
 * counting event handle.
 */
int PtlCTInc(ptl_handle_ct_t ct_handle, ptl_ct_event_t increment)
{
	int err;
	ct_t *ct;
	ni_t *ni;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
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
#else
	ct = fast_to_obj(ct_handle);
#endif

	ni = obj_to_ni(ct);

	pthread_mutex_lock(&ct->mutex);

	__ct_inc(ct, increment);

	pthread_mutex_unlock(&ct->mutex);

	err = PTL_OK;
#ifndef NO_ARG_VALIDATION
err2:
#endif
	ct_put(ct);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Setup a triggered ct increment operation.
 *
 * When the ct with handle trig_ct_handle reaches
 * threshold increment the ct with handle ct_handle
 * by the amount increment.
 *
 * buf holds a reference to ct until it gets processed
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
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was
 * passed.
 */
int PtlTriggeredCTInc(ptl_handle_ct_t ct_handle, ptl_ct_event_t increment,
		      ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	ct_t *ct;
	ct_t *trig_ct;
	ni_t *ni;
	buf_t *buf;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
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
#else
	ct = fast_to_obj(ct_handle);
	trig_ct = fast_to_obj(trig_ct_handle);
	ni = obj_to_ni(trig_ct);
#endif

	err = buf_alloc(ni, &buf);
	if (err) {
		err = PTL_NO_SPACE;
		ct_put(ct);
		goto err2;
	}

	buf->type = BUF_TRIGGERED;
	buf->op = TRIG_CT_INC;
	buf->ct = ct;
	buf->value = increment;
	buf->threshold = threshold;

	pthread_mutex_lock(&trig_ct->mutex);

	__post_trig_ct(buf, trig_ct);

	pthread_mutex_unlock(&trig_ct->mutex);

	err = PTL_OK;
err2:
	ct_put(trig_ct);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Setup a triggered ct set operation.
 *
 * When the ct with handle trig_ct_handle reaches
 * threshold set the ct with handle ct_handle
 * to the value new_ct
 *
 * buf holds a reference to ct until it gets processed
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
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been successfully initialized.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 */
int PtlTriggeredCTSet(ptl_handle_ct_t ct_handle, ptl_ct_event_t new_ct,
		      ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{
	int err;
	ct_t *ct;
	ct_t *trig_ct;
	ni_t *ni;
	buf_t *buf;

	/* convert handles to objects */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
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
#else
	trig_ct = fast_to_obj(trig_ct_handle);
	ct = fast_to_obj(ct_handle);
	ni = obj_to_ni(trig_ct);
#endif

	/* get container for triggered ct op */
	err = buf_alloc(ni, &buf);
	if (err) {
		err = PTL_NO_SPACE;
		ct_put(ct);
		goto err2;
	}

	buf->type = BUF_TRIGGERED;
	buf->op = TRIG_CT_SET;
	buf->ct = ct;
	buf->value = new_ct;
	buf->threshold = threshold;

	pthread_mutex_lock(&trig_ct->mutex);

	__post_trig_ct(buf, trig_ct);

	pthread_mutex_unlock(&trig_ct->mutex);

	err = PTL_OK;
err2:
	ct_put(trig_ct);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Perform triggered ct operation.
 *
 * @param[in] buf
 */
static void do_trig_ct_op(buf_t *buf)
{
	ct_t *ct = buf->ct;

	/* we're a zombie */
	if (ct->interrupt)
		goto done;

	pthread_mutex_lock(&ct->mutex);

	switch(buf->op) {
	case TRIG_CT_SET:
		__ct_set(ct, buf->value);
		break;
	case TRIG_CT_INC:
		__ct_inc(ct, buf->value);
		break;
	default:
		assert(0);
	}

	pthread_mutex_unlock(&ct->mutex);

done:
	ct_put(ct);
	buf_put(buf);
}

/**
 * @brief Add a operation to list of pending triggered operations or
 * go ahead and do it if threshold has been met.
 *
 * @param[in] buf The req buf to post to the counting event's list.
 * @param[in] ct The counting event to add the buf to.
 */
void post_ct(buf_t *buf, ct_t *ct)
{
	pthread_mutex_lock(&ct->mutex);
	if ((ct->event.success + ct->event.failure) >= buf->ct_threshold) {
		pthread_mutex_unlock(&ct->mutex);
		process_init(buf);
		return;
	}
	list_add(&buf->list, &ct->trig_list);
	pthread_mutex_unlock(&ct->mutex);
}

/**
 * @brief Add a triggered ct operation to a list or
 * do it if threshold has already been reached.
 *
 * @pre caller should hold trig_ct->mutex
 *
 * @param[in] buf
 * @param[in] trig_ct
 */
static void __post_trig_ct(buf_t *buf, ct_t *trig_ct)
{
	if ((trig_ct->event.failure + trig_ct->event.success) >=
	    buf->threshold) {
		/* TODO enqueue buf somewhere */
		pthread_mutex_unlock(&trig_ct->mutex);
		do_trig_ct_op(buf);
		pthread_mutex_lock(&trig_ct->mutex);
	} else {
		list_add(&buf->list, &trig_ct->trig_list);
	}
}

/**
 * @brief Update a counting event.
 *
 * @param[in] ct The counting event to update.
 * @param[in] buf The req buf from which to update it.
 * @maram[in] bytes Whether to update evebts or bytes.
 */
void make_ct_event(ct_t *ct, buf_t *buf, enum ct_bytes bytes)
{
	pthread_mutex_lock(&ct->mutex);

	if (unlikely(buf->ni_fail))
		ct->event.failure++;
	else if (likely(bytes == CT_EVENTS))
		ct->event.success++;
	else if (likely(bytes == CT_MBYTES))
		ct->event.success += buf->mlength;
	else {
		assert(bytes == CT_RBYTES);
		ct->event.success +=
			le64_to_cpu(((req_hdr_t *)buf->data)->length);
	}

	__ct_check(ct);

	pthread_mutex_unlock(&ct->mutex);
}
