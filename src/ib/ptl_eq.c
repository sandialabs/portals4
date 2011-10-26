/**
 * @file ptl_eq.h
 *
 * Event queue implementation.
 */

#include "ptl_loc.h"

/**
 * Initialize a eq object once when created.
 *
 * @param[in] arg opaque eq address
 * @param[in] unused unused
 */
int eq_init(void *arg, void *unused)
{
	eq_t *eq = arg;

        pthread_mutex_init(&eq->mutex, NULL);
        pthread_cond_init(&eq->cond, NULL);

	return PTL_OK;
}

/**
 * Cleanup a eq object once when destroyed.
 *
 * @param[in] arg opaque eq address
 */
void eq_fini(void *arg)
{
	eq_t *eq = arg;

	pthread_cond_destroy(&eq->cond);
	pthread_mutex_destroy(&eq->mutex);
}

/**
 * Initialize eq each time when allocated from free list.
 *
 * @param[in] arg opaque eq address
 *
 * @return status
 */
int eq_new(void *arg)
{
	eq_t *eq = arg;

	eq->producer = 0;
	eq->consumer = 0;
	eq->prod_gen = 0;
	eq->cons_gen = 0;
	eq->interrupt = 0;
	eq->overflow = 0;
	eq->waiters = 0;

	return PTL_OK;
}

/**
 * Cleanup eq each time when freed.
 *
 * Called when last reference to eq is dropped.
 *
 * @param[in] arg opaque eq address
 */
void eq_cleanup(void *arg)
{
	eq_t *eq = arg;

	if (eq->eqe_list)
		free(eq->eqe_list);
	eq->eqe_list = NULL;
}

/**
 * Allocate an event queue object.
 *
 * @param[in] ni_handle
 * @param[in] count
 * @param[out] eq_handle_p
 *
 * @return status
 */
int PtlEQAlloc(ptl_handle_ni_t ni_handle,
	       ptl_size_t count,
	       ptl_handle_eq_t *eq_handle_p)
{
	int err;
	ni_t *ni;
	eq_t *eq;

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

        /* check limit resources to see if we can allocate another eq */
        if (unlikely(__sync_add_and_fetch(&ni->current.max_eqs, 1) >
            ni->limits.max_eqs)) {
                (void)__sync_fetch_and_sub(&ni->current.max_eqs, 1);
                err = PTL_NO_SPACE;
                goto err2;
        }

	/* allocate event queue object */
	err = eq_alloc(ni, &eq);
	if (unlikely(err)) {
                (void)__sync_fetch_and_sub(&ni->current.max_eqs, 1);
		goto err2;
	}

	/* allocate memory for event circular buffer */
	eq->eqe_list = calloc(count, sizeof(*eq->eqe_list));
	if (!eq->eqe_list) {
		err = PTL_NO_SPACE;
                (void)__sync_fetch_and_sub(&ni->current.max_eqs, 1);
		eq_put(eq);
		goto err2;
	}

	eq->count = count;

	*eq_handle_p = eq_to_handle(eq);

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
 * check to see if event queue has waiters.
 *
 * @pre caller should hold eq->mutex
 *
 * @param eq the event queue to check
 */
static void __eq_check(eq_t *eq)
{
	ni_t *ni = obj_to_ni(eq);

	if (eq->waiters)
		pthread_cond_broadcast(&eq->cond);

	pthread_mutex_lock(&ni->eq_wait_mutex);
	if (ni->eq_waiters)
		pthread_cond_broadcast(&ni->eq_wait_cond);
	pthread_mutex_unlock(&ni->eq_wait_mutex);
}

/**
 * Free an event queue object.
 *
 * @param[in] eq_handle
 *
 * @return status
 */
int PtlEQFree(ptl_handle_eq_t eq_handle)
{
	int err;
	eq_t *eq;
	ni_t *ni;

	/* convert handle to object */
	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_eq(eq_handle, &eq);
		if (err)
			goto err1;

		if (!eq) {
			err = PTL_ARG_INVALID;
			goto err1;
		}

		ni = obj_to_ni(eq);
		if (!ni) {
			err = PTL_ARG_INVALID;
			eq_put(eq);
			goto err1;
		}
	} else {
		eq = fast_to_obj(eq_handle);
		ni = obj_to_ni(eq);
	}

	/* cleanup resources waiting for an event */
	pthread_mutex_lock(&eq->mutex);

	eq->interrupt = 1;
	__eq_check(eq);

	pthread_mutex_unlock(&eq->mutex);

	err = PTL_OK;
	eq_put(eq);
	eq_put(eq);

	/* give back the limit resource */
	(void)__sync_sub_and_fetch(&ni->current.max_eqs, 1);

err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * find next event in event queue.
 *
 * @pre caller should hold eq->mutex
 *
 * @param[in] eq the event queue
 * @param[out] event_p the address of the returned event
 *
 * @return PTL_EQ_EMPTY if there are no events in the queue
 * @return PTL_EQ_DROPPED if there was an event but there was a
 * gap since the last event returned
 * @return PTL_EQ_OK if there was an event and no gap
 */
static int __get_event(volatile eq_t *eq, ptl_event_t *event_p)
{
	int dropped = 0;

	/* check to see if the queue is empty */
	if ((eq->producer == eq->consumer) &&
	    (eq->prod_gen == eq->cons_gen))
		return PTL_EQ_EMPTY;

	/* if we have been lapped by the producer advance the
	 * consumer pointer until we catch up */
	while (eq->cons_gen < eq->eqe_list[eq->consumer].generation) {
		eq->consumer++;
		if (eq->consumer == eq->count) {
			eq->consumer = 0;
			eq->cons_gen++;
		}
		dropped = 1;
	}

	/* return the next valid event and update the consumer pointer */
	*event_p = eq->eqe_list[eq->consumer++].event;
	if (eq->consumer >= eq->count) {
		eq->consumer = 0;
		eq->cons_gen++;
	}

	return dropped ? PTL_EQ_DROPPED : PTL_OK;
}

/**
 * Get the next event in an event queue.
 *
 * The PtlEQGet() function is a nonblocking function that can be used to get
 * the next event in an event queue. The event * is removed from the queue.
 *
 * @param[in] eq_handle
 * @param[out] event_p
 *
 * @return status
 */
int PtlEQGet(ptl_handle_eq_t eq_handle,
	     ptl_event_t *event_p)
{
	int err;
	eq_t *eq;

	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_eq(eq_handle, &eq);
		if (err)
			goto err1;

		if (!eq) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	} else {
		eq = fast_to_obj(eq_handle);
	}

	pthread_mutex_lock(&eq->mutex);
	err = __get_event(eq, event_p);
	pthread_mutex_unlock(&eq->mutex);

	eq_put(eq);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Wait for next event in event queue.
 *
 * @param[in] eq_handle
 * @param[out] event_p
 *
 * @return status
 */
int PtlEQWait(ptl_handle_eq_t eq_handle,
	      ptl_event_t *event_p)
{
	int err;
	eq_t *eq;
	ni_t *ni;
	int nloops = get_param(PTL_EQ_WAIT_LOOP_COUNT);

	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;

		err = to_eq(eq_handle, &eq);
		if (err)
			goto err1;

		if (!eq) {
			err = PTL_ARG_INVALID;
			goto err1;
		}
	} else {
		eq = fast_to_obj(eq_handle);
	}

	ni = obj_to_ni(eq);

	while(1) {
		/* spin nloops times and then block */
                if (nloops) {
			pthread_mutex_lock(&eq->mutex);
			err = __get_event(eq, event_p);
			pthread_mutex_unlock(&eq->mutex);
			if (err != PTL_EQ_EMPTY) {
				break;
			}

			if (eq->interrupt) {
				err = PTL_INTERRUPTED;
				break;
			}

                        nloops--;
                        SPINLOCK_BODY();
                        continue;
                } else {
			pthread_mutex_lock(&eq->mutex);

			/* check to see if there is an event in the queue */
			err = __get_event(eq, event_p);
			if (err != PTL_EQ_EMPTY) {
				pthread_mutex_unlock(&eq->mutex);
				break;
			}

			/* check to see if we should interrupt wait */
			if (eq->interrupt) {
				pthread_mutex_unlock(&eq->mutex);
				err = PTL_INTERRUPTED;
				break;
			}

			/* block until something changes */
			eq->waiters++;
			pthread_cond_wait(&eq->cond, &eq->mutex);
			eq->waiters--;

			pthread_mutex_unlock(&eq->mutex);
		}
	}

	eq_put(eq);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Poll for an event in an array of event queues.
 *
 * @param[in] eq_handles array of event queue handles
 * @param[in] size the size of the array
 * @param[in] timeout how long to poll in msec
 * @param[out] event_p address of returned event
 * @param[out] which_p address of returned array index
 *
 * @return status
 */
int PtlEQPoll(const ptl_handle_eq_t *eq_handles, unsigned int size,
	      ptl_time_t timeout, ptl_event_t *event_p, unsigned int *which_p)
{
	int err;
	ni_t *ni = NULL;
	eq_t **eqs = NULL;
	struct timespec expire;
	struct timespec now;
	int forever = (timeout == PTL_TIME_FOREVER);
	int i;
	int i2;
	int found_one;
	int nloops = get_param(PTL_EQ_POLL_LOOP_COUNT);

	if (check_param) {
		err = get_gbl();
		if (err)
			goto err0;
	}

	if (size == 0) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	eqs = malloc(size*sizeof(*eqs));
	if (!eqs) {
		err = PTL_NO_SPACE;
		goto err1;
	}

	if (check_param) {
		i2 = -1;
		for (i = 0; i < size; i++) {
			err = to_eq(eq_handles[i], &eqs[i]);
			if (unlikely(err || !eqs[i])) {
				err = PTL_ARG_INVALID;
				goto err2;
			}

			i2 = i;

			if (i == 0)
				ni = obj_to_ni(eqs[0]);

			if (ni != obj_to_ni(eqs[i])) {
				err = PTL_ARG_INVALID;
				goto err2;
			}
		}
	} else {
		for (i = 0; i < size; i++)
			eqs[i] = fast_to_obj(eq_handles[i]);
		ni = obj_to_ni(eqs[0]);
		i2 = size;
	}

	clock_gettime(CLOCK_REALTIME, &expire);
	expire.tv_nsec += 1000000UL * timeout;
	expire.tv_sec += 9999999999UL * forever;

	while (expire.tv_nsec > 1000000000UL) {
		expire.tv_nsec -= 1000000000UL;
		expire.tv_sec++;
	}

	while (1) {
		/* spin nloops times and then block */
                if (nloops) {
			found_one = 0;
			for (i = 0; i < size; i++) {
				pthread_mutex_lock(&eqs[i]->mutex);
				err = __get_event(eqs[i], event_p);
				pthread_mutex_unlock(&eqs[i]->mutex);
				if (err != PTL_EQ_EMPTY) {
					*which_p = i;
					found_one++;
					break;
				}

				if (eqs[i]->interrupt) {
					err = PTL_INTERRUPTED;
					found_one++;
					break;
				}
			}

			if (found_one)
				break;

			if (!forever) {
				clock_gettime(CLOCK_REALTIME, &now);
				if ((now.tv_sec > expire.tv_sec) ||
				    ((now.tv_sec == expire.tv_sec) &&
				     (now.tv_nsec > expire.tv_nsec))) {
					err = PTL_EQ_EMPTY;
					break;
				}
			}

                        nloops--;
                        SPINLOCK_BODY();
                        continue;
                } else {
			pthread_mutex_lock(&ni->eq_wait_mutex);

			found_one = 0;
			for (i = 0; i < size; i++) {
				pthread_mutex_lock(&eqs[i]->mutex);
				err = __get_event(eqs[i], event_p);
				pthread_mutex_unlock(&eqs[i]->mutex);
				if (err != PTL_EQ_EMPTY) {
					*which_p = i;
					found_one++;
					break;
				}

				if (eqs[i]->interrupt) {
					err = PTL_INTERRUPTED;
					found_one++;
					break;
				}
			}

			if (found_one) {
				pthread_mutex_unlock(&ni->eq_wait_mutex);
				break;
			}

			ni->eq_waiters++;
			err = pthread_cond_timedwait(&ni->eq_wait_cond, &ni->eq_wait_mutex, &expire);
			ni->eq_waiters--;

			pthread_mutex_unlock(&ni->eq_wait_mutex);

                        /* check to see if we timed out */
                        if (err == ETIMEDOUT) {
                                err = PTL_EQ_EMPTY;
                                break;
                        }
		}
	}

err2:
	for (i = 0; i < i2; i++)
		eq_put(eqs[i]);
	free(eqs);
err1:
	if (check_param)
		gbl_put();
err0:
	return err;
}

/**
 * Make and add a new event to the event queue from an xi.
 *
 * @param[in] xi
 * @param[in] eq
 * @param[in] type
 * @param[in] start
 */
void make_init_event(buf_t *buf, eq_t *eq, ptl_event_kind_t type, void *start)
{
	ptl_event_t *ev;

	pthread_mutex_lock(&eq->mutex);

	eq->eqe_list[eq->producer].generation = eq->prod_gen;
	ev = &eq->eqe_list[eq->producer++].event;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}

	ev->type		= type;
	ev->initiator		= buf->xi.target;
	ev->pt_index		= buf->xi.pt_index;
	ev->uid			= buf->xi.uid;
	ev->match_bits		= buf->xi.match_bits;
	ev->rlength		= buf->xi.rlength;
	ev->mlength		= buf->xi.mlength;
	ev->remote_offset	= buf->xi.moffset;
	ev->start		= start;
	ev->user_ptr		= buf->xi.user_ptr;
	ev->hdr_data		= buf->xi.hdr_data;
	ev->ni_fail_type	= buf->xi.ni_fail;
	ev->atomic_operation	= buf->xi.atom_op;
	ev->atomic_type		= buf->xi.atom_type;

	__eq_check(eq);

	pthread_mutex_unlock(&eq->mutex);
}

/**
 * Make and add an event to the event queue from an xt.
 *
 * @param[in] xt
 * @param[in] eq
 * @param[in] type
 * @param[in] usr_ptr
 * @param[in] start
 */
void make_target_event(xt_t *xt, eq_t *eq, ptl_event_kind_t type,
		       void *user_ptr, void *start)
{
	ptl_event_t *ev;

	pthread_mutex_lock(&eq->mutex);

	eq->eqe_list[eq->producer].generation = eq->prod_gen;
	ev = &eq->eqe_list[eq->producer++].event;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}

	ev->type		= type;
	ev->initiator		= xt->initiator;
	ev->pt_index		= xt->pt_index;
	ev->uid			= xt->uid;
	ev->match_bits		= xt->match_bits;
	ev->rlength		= xt->rlength;
	ev->mlength		= xt->mlength;
	ev->remote_offset	= xt->roffset;
	ev->start		= start;
	ev->user_ptr		= user_ptr;
	ev->hdr_data		= xt->hdr_data;
	ev->ni_fail_type	= xt->ni_fail;
	ev->atomic_operation	= xt->atom_op;
	ev->atomic_type		= xt->atom_type;

	__eq_check(eq);

	pthread_mutex_unlock(&eq->mutex);
}

/**
 * Make and event and add to event queue from an LE/ME.
 *
 * @param[in] le
 * @param[in] eq
 * @param[in] type
 * @param[in] fail_type
 */
void make_le_event(le_t *le, eq_t *eq, ptl_event_kind_t type,
		   ptl_ni_fail_t fail_type)
{
	ptl_event_t *ev;

	pthread_mutex_lock(&eq->mutex);

	eq->eqe_list[eq->producer].generation = eq->prod_gen;
	ev = &eq->eqe_list[eq->producer++].event;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}

	ev->type = type;
	ev->pt_index = le->pt_index;
	ev->user_ptr = le->user_ptr;
	ev->ni_fail_type = fail_type;

	__eq_check(eq);

	pthread_mutex_unlock(&eq->mutex);
}
