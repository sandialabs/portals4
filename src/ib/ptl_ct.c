/**
 * @file ptl_ct.c
 *
 * @brief This file implements the counting event class.
 */

#include "ptl_loc.h"
#include "ptl_timer.h"

/* TODO make these unnecessary by queuing deferred operations */
void ct_check(ct_t *ct);
static void post_trig_ct(struct buf *buf, ct_t *trig_ct);
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

	PTL_FASTLOCK_INIT(&ct->lock);
	INIT_LIST_HEAD(&ct->trig_list);
	atomic_set(&ct->list_size, 0);

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

	PTL_FASTLOCK_DESTROY(&ct->lock);
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

	ct->info.interrupt = 0;
	ct->info.event.failure = 0;
	ct->info.event.success = 0;

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

	ct->info.interrupt = 0;
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
int _PtlCTAlloc(PPEGBL ptl_handle_ni_t ni_handle, ptl_handle_ct_t *ct_handle_p)
{
	int err;
	ni_t *ni;
	ct_t *ct;

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ni(MYGBL_ ni_handle, &ni);
	if (err)
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ni = to_obj(MYGBL_ POOL_ANY, ni_handle);
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
	PTL_FASTLOCK_LOCK(&ni->ct_list_lock);
	list_add(&ct->list, &ni->ct_list);
	PTL_FASTLOCK_UNLOCK(&ni->ct_list_lock);

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
int _PtlCTFree(PPEGBL ptl_handle_ct_t ct_handle)
{
	int err;
	ct_t *ct;
	ni_t *ni;

	/* convert handle to pointer */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(MYGBL_ ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = to_obj(MYGBL_ POOL_ANY, ct_handle);
#endif

	ni = obj_to_ni(ct);

	/* remove ourselves from ni->ct_list */
	PTL_FASTLOCK_LOCK(&ni->ct_list_lock);
	list_del(&ct->list);
	if (atomic_read(&ct->list_size) > 0){
		atomic_dec(&ct->list_size);
	}
	PTL_FASTLOCK_UNLOCK(&ni->ct_list_lock);	

	/* clean up pending operations */
	ct->info.interrupt = 1;
	ct_check(ct);

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
int _PtlCTCancelTriggered(PPEGBL ptl_handle_ct_t ct_handle)
{
	int err;
	ct_t *ct;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(MYGBL_ ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = to_obj(MYGBL_ POOL_ANY, ct_handle);
#endif

	ct->info.interrupt = 1;
	ct_check(ct);

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
int _PtlCTGet(PPEGBL ptl_handle_ct_t ct_handle, ptl_ct_event_t *event_p)
{
	int err;
	ct_t *ct;

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(MYGBL_ ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = to_obj(MYGBL_ POOL_ANY, ct_handle);
#endif

	*event_p = ct->info.event;

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
int _PtlCTWait(PPEGBL ptl_handle_ct_t ct_handle, uint64_t threshold,
	      ptl_ct_event_t *event_p)
{
	int err;
	ct_t *ct;

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(MYGBL_ ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = to_obj(MYGBL_ POOL_ANY, ct_handle);
#endif

	err = PtlCTWait_work(&ct->info, threshold, event_p);

	ct_put(ct);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
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
int _PtlCTPoll(PPEGBL const ptl_handle_ct_t *ct_handles, const ptl_size_t *thresholds,
			  unsigned int size, ptl_time_t timeout, ptl_ct_event_t *event_p,
			  unsigned int *which_p)
{
	int err;
	struct ct_info *cts_info[size];
	ct_t *cts[size];
	int i;
	int i2;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;
#endif

	if (size == 0) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* convert handles to pointers */
#ifndef NO_ARG_VALIDATION
	i2 = -1;
	for (i = 0; i < size; i++) {
		ni_t *ni = NULL;

		err = to_ct(MYGBL_ ct_handles[i], &cts[i]);
		if (unlikely(err || !cts[i])) {
			err = PTL_ARG_INVALID;
			goto err2;
		}
		cts_info[i] = &cts[i]->info;

		i2 = i;

		if (NULL == ni)
			ni = obj_to_ni(cts[0]);

		if (obj_to_ni(cts[i]) != ni) {
			err = PTL_ARG_INVALID;
			goto err2;
		}
	}
#else
	for (i = 0; i < size; i++) {
		cts[i] = to_obj(MYGBL_ POOL_ANY, ct_handles[i]);
		cts_info[i] = &cts[i]->info;
	}
	i2 = size - 1;
#endif

	err = PtlCTPoll_work(cts_info, thresholds, size, timeout, event_p, which_p);

#ifndef NO_ARG_VALIDATION
 err2:
#endif
	for (i = i2; i >= 0; i--)
		ct_put((void *)cts[i]);
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
 * @param[in] ct The counting event to check.
 */
void ct_check(ct_t *ct)
{
	struct list_head *l;
	struct list_head *t;

	PTL_FASTLOCK_LOCK(&ct->lock);

	/* check to see if any pending triggered move operations
	 * can now be performed or discarded
	 * TODO this should enqueue the xi to a list and then unwind
	 * all the ct locks before calling process_init */
	list_for_each_prev_safe(l, t, &ct->trig_list) {
		buf_t *buf = list_entry(l, buf_t, list);

		if (buf->type == BUF_INIT) {
			if (ct->info.interrupt) {
				list_del(l);
				atomic_dec(&ct->list_size);

				PTL_FASTLOCK_UNLOCK(&ct->lock);

				buf->init_state = STATE_INIT_CLEANUP;
				process_init(buf);

				PTL_FASTLOCK_LOCK(&ct->lock);
			} else if ((ct->info.event.success + ct->info.event.failure) >= buf->ct_threshold) {
				list_del(l);
				atomic_dec(&ct->list_size);

				PTL_FASTLOCK_UNLOCK(&ct->lock);

				process_init(buf);

				PTL_FASTLOCK_LOCK(&ct->lock);
			}
		} else if (buf->type == BUF_TRIGGERED_ME){
			if (ct->info.interrupt) {
                                list_del(l);
                                atomic_dec(&ct->list_size);

                                PTL_FASTLOCK_UNLOCK(&ct->lock);

                                ct_put(buf->ct);
                                buf_put(buf);

                                PTL_FASTLOCK_LOCK(&ct->lock);
                        } else if ((ct->info.event.success + ct->info.event.failure) >= buf->ct_threshold) {
                                ptl_info("ME operation triggered: %i \n",buf->op);
				list_del(l);
                                atomic_dec(&ct->list_size);

                                PTL_FASTLOCK_UNLOCK(&ct->lock);
				
                                do_trig_me_op(buf);

                                PTL_FASTLOCK_LOCK(&ct->lock);
                        }
			else{
				ptl_info("ME operation not triggered %i:%i threshold: %i\n",(int)ct->info.event.success,(int)ct->info.event.failure, (int)buf->threshold);
			}
		} else {
			assert(buf->type == BUF_TRIGGERED);
			if (ct->info.interrupt) {
				list_del(l);
				atomic_dec(&ct->list_size);

				PTL_FASTLOCK_UNLOCK(&ct->lock);

				ct_put(buf->ct);
				buf_put(buf);

				PTL_FASTLOCK_LOCK(&ct->lock);
			} else if ((ct->info.event.success + ct->info.event.failure) >= buf->threshold) {
				list_del(l);
				atomic_dec(&ct->list_size);

				PTL_FASTLOCK_UNLOCK(&ct->lock);

				do_trig_ct_op(buf);

				PTL_FASTLOCK_LOCK(&ct->lock);
			}
		}
	}

	PTL_FASTLOCK_UNLOCK(&ct->lock);
}

/**
 * @brief Set counting event to new value.
 *
 * @param[in] ct
 * @param[in] new_ct
 */
static void ct_set(ct_t *ct, ptl_ct_event_t new_ct)
{
	/* set new value */
	ct->info.event = new_ct;

	/* check to see if this triggers any further
	 * actions */
	if (atomic_read(&ct->list_size))
		ct_check(ct);
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
int _PtlCTSet(PPEGBL ptl_handle_ct_t ct_handle, ptl_ct_event_t new_ct)
{
	int err;
	ct_t *ct;

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(MYGBL_ ct_handle, &ct);
	if (err)
		goto err1;

	if (!ct) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ct = to_obj(MYGBL_ POOL_ANY, ct_handle);
#endif

	ct_set(ct, new_ct);

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
 * @param[in] ct The counting event to increment.
 * @param[in] increment The increment value.
 */
static void ct_inc(ct_t *ct, ptl_ct_event_t increment)
{
	/* increment ct by value */
	if (likely(increment.success))
		(void)__sync_add_and_fetch(&ct->info.event.success, increment.success);
	else
		(void)__sync_add_and_fetch(&ct->info.event.failure, increment.failure);

	/* check to see if this triggers any further
	 * actions */
	if (atomic_read(&ct->list_size))
		ct_check(ct);
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
int _PtlCTInc(PPEGBL ptl_handle_ct_t ct_handle, ptl_ct_event_t increment)
{
	int err;
	ct_t *ct;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ct(MYGBL_ ct_handle, &ct);
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
	ct = to_obj(MYGBL_ POOL_ANY, ct_handle);
#endif

	ct_inc(ct, increment);

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
int _PtlTriggeredCTInc(PPEGBL ptl_handle_ct_t ct_handle, ptl_ct_event_t increment,
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

	err = to_ct(MYGBL_ trig_ct_handle, &trig_ct);
	if (unlikely(err))
		goto err1;

	ni = obj_to_ni(trig_ct);

	err = to_ct(MYGBL_ ct_handle, &ct);
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
	ct = to_obj(MYGBL_ POOL_ANY, ct_handle);
	trig_ct = to_obj(MYGBL_ POOL_ANY, trig_ct_handle);
	ni = obj_to_ni(trig_ct);
#endif

	if ((trig_ct->info.event.failure + trig_ct->info.event.success) >= threshold) {
		/* Fast path. Condition is already met. */
		ct_inc(ct, increment);

		ct_put(ct);
	} else {
		err = buf_alloc(ni, &buf);
		if (err) {
			err = PTL_NO_SPACE;
			ct_put(ct);
			goto err2;
		}

		buf->type = BUF_TRIGGERED;
		buf->op = TRIG_CT_INC;
		buf->ct = ct;
		buf->ct_event = increment;
		buf->threshold = threshold;

		post_trig_ct(buf, trig_ct);

	}

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
int _PtlTriggeredCTSet(PPEGBL ptl_handle_ct_t ct_handle, ptl_ct_event_t new_ct,
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

	err = to_ct(MYGBL_ trig_ct_handle, &trig_ct);
	if (err)
		goto err1;

	ni = obj_to_ni(trig_ct);

	err = to_ct(MYGBL_ ct_handle, &ct);
	if (err)
		goto err2;

	if (ni != obj_to_ni(ct)) {
		err = PTL_ARG_INVALID;
		ct_put(ct);
		goto err2;
	}
#else
	trig_ct = to_obj(MYGBL_ POOL_ANY, trig_ct_handle);
	ct = to_obj(MYGBL_ POOL_ANY, ct_handle);
	ni = obj_to_ni(trig_ct);
#endif

	if ((trig_ct->info.event.failure + trig_ct->info.event.success) >= threshold) {
		/* Fast path. Condition already met. */
		ct_set(ct, new_ct);

	} else {
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
		buf->ct_event = new_ct;
		buf->threshold = threshold;

		post_trig_ct(buf, trig_ct);
	}

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
	if (ct->info.interrupt)
		goto done;

	switch(buf->op) {
	case TRIG_CT_SET:
		ct_set(ct, buf->ct_event);
		break;
	case TRIG_CT_INC:
		ct_inc(ct, buf->ct_event);
		break;
	default:
		assert(0);
	}

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
	assert(buf->type == BUF_INIT);

	/* Put the buffer on the wait list. */
	PTL_FASTLOCK_LOCK(&ct->lock);

	/* 1st check to see whether the condition is already met. */
	if ((ct->info.event.success + ct->info.event.failure) >= buf->ct_threshold) {
		PTL_FASTLOCK_UNLOCK(&ct->lock);

		process_init(buf);
	} else {
		atomic_inc(&ct->list_size);

		list_add(&buf->list, &ct->trig_list);

		/* We must check again to avoid a race with make_ct_event/ct_inc_ct_set. */
		if ((ct->info.event.success + ct->info.event.failure) >= buf->ct_threshold) {
			/* Something arrived while we were adding the buffer. */
			PTL_FASTLOCK_UNLOCK(&ct->lock);
			ct_check(ct);
		} else {
			PTL_FASTLOCK_UNLOCK(&ct->lock);
		}
	}
}

/**
 * @brief Add a triggered ct operation to a list or
 * do it if threshold has already been reached.
 *
 * @param[in] buf
 * @param[in] trig_ct
 */
static void post_trig_ct(buf_t *buf, ct_t *trig_ct)
{
	/* Put the buffer on the wait list. */
	PTL_FASTLOCK_LOCK(&trig_ct->lock);

	/* 1st check to see whether the condition is already met. */
	if ((trig_ct->info.event.failure + trig_ct->info.event.success) >= buf->threshold) {
		PTL_FASTLOCK_UNLOCK(&trig_ct->lock);

		do_trig_ct_op(buf);

	} else {
		atomic_inc(&trig_ct->list_size);

		list_add(&buf->list, &trig_ct->trig_list);

		/* We must check again to avoid a race with make_ct_event/ct_inc_ct_set. */
		if ((trig_ct->info.event.success + trig_ct->info.event.failure) >= buf->threshold) {
			/* Something arrived while we were adding the buffer. */
			PTL_FASTLOCK_UNLOCK(&trig_ct->lock);
			ct_check(trig_ct);
		} else {
			PTL_FASTLOCK_UNLOCK(&trig_ct->lock);
		}
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
	if (unlikely(buf->ni_fail))
		(void)__sync_add_and_fetch(&ct->info.event.failure, 1);
	else if (likely(bytes == CT_EVENTS))
		(void)__sync_add_and_fetch(&ct->info.event.success, 1);
	else if (likely(bytes == CT_MBYTES))
		(void)__sync_add_and_fetch(&ct->info.event.success, buf->mlength);
	else {
		assert(bytes == CT_RBYTES);
		(void)__sync_add_and_fetch(&ct->info.event.success, buf->rlength);
	}

	if (atomic_read(&ct->list_size))
		ct_check(ct);
}
