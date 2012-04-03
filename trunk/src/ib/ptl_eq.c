/**
 * @file ptl_eq.h
 *
 * @brief Event queue implementation.
 */

#include "ptl_loc.h"
#include "ptl_timer.h"

/**
 * @brief Initialize a eq object once when created.
 *
 * @param[in] arg opaque eq address
 * @param[in] unused unused
 */
int eq_init(void *arg, void *unused)
{
	eq_t *eq = arg;

	PTL_FASTLOCK_INIT(&eq->lock);

	return PTL_OK;
}

/**
 * @brief Cleanup a eq object once when destroyed.
 *
 * @param[in] arg opaque eq address
 */
void eq_fini(void *arg)
{
	PTL_FASTLOCK_DESTROY(&((eq_t*)arg)->lock);
}

/**
 * @brief Initialize eq each time when allocated from free list.
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

	return PTL_OK;
}

/**
 * @brief Cleanup eq each time when freed.
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
 * @brief Allocate an event queue object.
 *
 * @param[in] ni_handle The handle of the network interface.
 * @param[in] count The number of elements in the event queue.
 * @param[out] eq_handle_p The address of the returned event queue handle.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * @return PTL_NO_SPACE Indicates that there is insufficient memory to
 * allocate the event queue.
 */
int PtlEQAlloc(ptl_handle_ni_t ni_handle,
	       ptl_size_t count,
	       ptl_handle_eq_t *eq_handle_p)
{
	int err;
	ni_t *ni;
	eq_t *eq;

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
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Free an event queue object.
 *
 * @param[in] eq_handle The handle of the event queue to free.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that eq_handle is not a valid
 * event queue handle.
 */
int PtlEQFree(ptl_handle_eq_t eq_handle)
{
	int err;
	eq_t *eq;
	ni_t *ni;

	/* convert handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
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
#else
	eq = fast_to_obj(eq_handle);
	ni = obj_to_ni(eq);
#endif

	/* cleanup resources. */
	eq->interrupt = 1;
	__sync_synchronize();

	err = PTL_OK;
	eq_put(eq);					/* from to_eq() */
	eq_put(eq);					/* from PtlCTAlloc() */

	/* give back the limit resource */
	(void)__sync_sub_and_fetch(&ni->current.max_eqs, 1);

#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

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
static int inline is_queue_empty(eq_t *eq)
{
	return ((eq->producer == eq->consumer) &&
			(eq->prod_gen == eq->cons_gen));
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
static int get_event(eq_t * restrict eq, ptl_event_t * restrict event_p)
{
	int dropped = 0;

	PTL_FASTLOCK_LOCK(&eq->lock);

	/* check to see if the queue is empty */
	if (is_queue_empty(eq)) {
		PTL_FASTLOCK_UNLOCK(&eq->lock);
		return PTL_EQ_EMPTY;
	}

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

	PTL_FASTLOCK_UNLOCK(&eq->lock);

	return dropped ? PTL_EQ_DROPPED : PTL_OK;
}

/**
 * @brief Get the next event in an event queue.
 *
 * The PtlEQGet() function is a nonblocking function that can be used to get
 * the next event in an event queue. The event * is removed from the queue.
 *
 * @param[in] eq_handle The handle of the event queue from which to get an event.
 * @param[out] event_p The address of the returned event.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_EQ_DROPPED Indicates success (i.e., an event is returned) and
 * that at least one full event between this full event and the last full
 * event obtained—using PtlEQGet(), PtlEQWait(), or PtlEQPoll()—from this
 * event queue has been dropped due to limited space in the event queue.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_EQ_EMPTY Indicates that eq_handle is empty or another thread
 * is waiting in PtlEQWait().
 * @return PTL_ARG_INVALID Indicates that eq_handle is not a valid event
 * queue handle.
 */
int PtlEQGet(ptl_handle_eq_t eq_handle,
			 ptl_event_t *event_p)
{
	int err;
	eq_t *eq;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_eq(eq_handle, &eq);
	if (err)
		goto err1;

	if (!eq) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	eq = fast_to_obj(eq_handle);
#endif

	err = get_event(eq, event_p);

	eq_put(eq);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Wait for next event in event queue.
 *
 * @param[in] eq_handle The handle of the event queue on which to wait
 * for an event.
 * @param[out] event_p The address of the returned event.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_EQ_DROPPED Indicates success (i.e., an event is returned)
 * and that at least one full event between this full event and the last
 * full event obtained—using PtlEQGet(), PtlEQWait(), or PtlEQPoll()—from
 * this event queue has been dropped due to limited space in the event queue.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that eq_handle is not a valid event
 * queue handle.
 * @return PTL_INTERRUPTED Indicates that PtlEQFree() or PtlNIFini() was
 * called by another thread while this thread was waiting in PtlEQWait().
 */
int PtlEQWait(ptl_handle_eq_t eq_handle,
			  ptl_event_t *event_p)
{
	int err;
	eq_t *eq;
	ni_t *ni;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_eq(eq_handle, &eq);
	if (err)
		goto err1;

	if (!eq) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	eq = fast_to_obj(eq_handle);
#endif

	ni = obj_to_ni(eq);

	while(1) {
		if (!is_queue_empty(eq)) {
			err = get_event(eq, event_p);
			if (err != PTL_EQ_EMPTY) {
				break;
			}
		}

		if (eq->interrupt) {
			err = PTL_INTERRUPTED;
			break;
		}

		SPINLOCK_BODY();
	}

	eq_put(eq);
#ifndef NO_ARG_VALIDATION
 err1:
	gbl_put();
 err0:
#endif
	return err;
}

/**
 * @brief Poll for an event in an array of event queues.
 *
 * @param[in] eq_handles array of event queue handles
 * @param[in] size the size of the array
 * @param[in] timeout how long to poll in msec
 * @param[out] event_p address of returned event
 * @param[out] which_p address of returned array index
 *
 * @return PTL_OK Indicates success.
 * @return PTL_EQ_DROPPED Indicates success (i.e., an event is returned)
 * and that at least one full event between this full event and the
 * last full event obtained from the event queue indicated by which has
 * been dropped due to limited space in the event queue.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * The definition of which arguments are checked is implementation dependent.
 * @return PTL_EQ_EMPTY Indicates that the timeout has been reached and all
 * of the event queues are empty.
 * @return PTL_INTERRUPTED Indicates that PtlEQFree() or PtlNIFini() was
 * called by another thread while this thread was waiting in PtlEQPoll().
 */
int PtlEQPoll(const ptl_handle_eq_t *eq_handles, unsigned int size,
			  ptl_time_t timeout, ptl_event_t *event_p, unsigned int *which_p)
{
	int err;
	ni_t *ni = NULL;
	eq_t **eqs = NULL;
	size_t nstart;
	int i;
	int i2;
	const int forever = (timeout == PTL_TIME_FOREVER);

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;
#endif

	if (size == 0) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	eqs = malloc(size*sizeof(*eqs));
	if (!eqs) {
		err = PTL_NO_SPACE;
		goto err1;
	}

#ifndef NO_ARG_VALIDATION
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
#else
	for (i = 0; i < size; i++)
		eqs[i] = fast_to_obj(eq_handles[i]);
	i2 = size - 1;
	ni = obj_to_ni(eqs[0]);
#endif

	{
	    TIMER_TYPE start;
	    MARK_TIMER(start);
	    nstart = TIMER_INTS(start);
	}

	while (1) {
		for (i = 0; i < size; i++) {
			eq_t *eq = eqs[i];

			if (!is_queue_empty(eq)) {
				err = get_event(eq, event_p);

				if (err != PTL_EQ_EMPTY) {
					*which_p = i;
					goto out;
				}
			}

			if (eq->interrupt) {
				err = PTL_INTERRUPTED;
				goto out;
			}
		}

		if (!forever) {
			TIMER_TYPE tp;
			MARK_TIMER(tp);
			if ((TIMER_INTS(tp) - nstart) >= timeout) {
			    err = PTL_EQ_EMPTY;
			    goto out;
			}
		}

		SPINLOCK_BODY();
	}

 out:

#ifndef NO_ARG_VALIDATION
 err2:
#endif
	for (i = i2; i >= 0; i--)
		eq_put(eqs[i]);
	free(eqs);
 err1:
#ifndef NO_ARG_VALIDATION
	gbl_put();
 err0:
#endif
	return err;
}

/**
 * @brief Make and add a new event to the event queue from a buf.
 *
 * @param[in] buf The req buf for which to make the event.
 * @param[in] eq The event queue.
 * @param[in] type The event type.
 */
void make_init_event(buf_t * restrict buf, eq_t * restrict eq, ptl_event_kind_t type)
{
	ptl_event_t *ev;

	PTL_FASTLOCK_LOCK(&eq->lock);

	eq->eqe_list[eq->producer].generation = eq->prod_gen;
	ev = &eq->eqe_list[eq->producer++].event;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}

	ev->type		= type;
	ev->user_ptr		= buf->user_ptr;

	if (type == PTL_EVENT_SEND) {
		/* The buf->ni_fail field can be overriden by the target side
		 * before we reach this function. If the message has been
		 * delivered then sending it was OK. */
		if (buf->ni_fail == PTL_NI_UNDELIVERABLE)
			ev->ni_fail_type = PTL_NI_UNDELIVERABLE;
		else
			ev->ni_fail_type = PTL_NI_OK;
	} else {
		ev->mlength		= buf->mlength;
		ev->remote_offset	= buf->moffset;
		ev->ni_fail_type	= buf->ni_fail;
	}

	PTL_FASTLOCK_UNLOCK(&eq->lock);
}

/**
 * @brief Fill in event struct in memory.
 *
 * @param[in] buf The req buf from which to make the event.
 * @param[in] type The event type.
 * @param[in] usr_ptr The user pointer.
 * @param[in] start The start address.
 * @param[out] The address of the returned event.
 */
void fill_target_event(buf_t * restrict buf, ptl_event_kind_t type,
		       void *user_ptr, void *start, ptl_event_t * restrict ev)
{
	const req_hdr_t *hdr = (req_hdr_t *)buf->data;

	ev->type		= type;
	ev->start		= start;
	ev->user_ptr		= user_ptr;

	ev->ni_fail_type	= buf->ni_fail;
	ev->mlength		= buf->mlength;
	ev->match_bits		= le64_to_cpu(hdr->match_bits);
	ev->hdr_data		= le64_to_cpu(hdr->hdr_data);
	ev->pt_index		= le32_to_cpu(hdr->pt_index);
	ev->uid			= le32_to_cpu(hdr->uid);
	ev->rlength		= le64_to_cpu(hdr->length);
	ev->remote_offset	= le64_to_cpu(hdr->offset);
	ev->atomic_operation	= hdr->atom_op;
	ev->atomic_type		= hdr->atom_type;
	ev->initiator.phys.nid	= le32_to_cpu(hdr->src_nid);
	ev->initiator.phys.pid	= le32_to_cpu(hdr->src_pid);
}

/**
 * @brief generate an event from an event struct.
 *
 * @param[in] eq the event queue to post the event to.
 * @param[in] ev the event to post.
 */
void send_target_event(eq_t * restrict eq, ptl_event_t * restrict ev)
{
	PTL_FASTLOCK_LOCK(&eq->lock);

	eq->eqe_list[eq->producer].generation = eq->prod_gen;
	eq->eqe_list[eq->producer++].event = *ev;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}

	PTL_FASTLOCK_UNLOCK(&eq->lock);
}
		       
/**
 * @brief Make and add an event to the event queue from a buf.
 *
 * @param[in] buf The req buf from which to make the event.
 * @param[in] eq The event queue to add the event to.
 * @param[in] type The event type.
 * @param[in] usr_ptr The user pointer.
 * @param[in] start The start address.
 */
void make_target_event(buf_t * restrict buf, eq_t * restrict eq, ptl_event_kind_t type,
		       void *user_ptr, void *start)
{
	ptl_event_t *ev;

	PTL_FASTLOCK_LOCK(&eq->lock);

	eq->eqe_list[eq->producer].generation = eq->prod_gen;
	ev = &eq->eqe_list[eq->producer++].event;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}

	fill_target_event(buf, type, user_ptr, start, ev);

	PTL_FASTLOCK_UNLOCK(&eq->lock);
}

/**
 * @brief Make and event and add to event queue from an LE/ME.
 *
 * @param[in] le The list element to make the event from.
 * @param[in] eq The event queue to add the event to.
 * @param[in] type The event type.
 * @param[in] fail_type The NI fail type.
 */
void make_le_event(le_t * restrict le, eq_t * restrict eq, ptl_event_kind_t type,
		   ptl_ni_fail_t fail_type)
{
	ptl_event_t *ev;

	PTL_FASTLOCK_LOCK(&eq->lock);

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

	PTL_FASTLOCK_UNLOCK(&eq->lock);
}
