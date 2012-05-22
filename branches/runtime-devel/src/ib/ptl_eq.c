/**
 * @file ptl_eq.h
 *
 * @brief Event queue implementation.
 */

#include "ptl_loc.h"
#include "ptl_timer.h"

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

	INIT_LIST_HEAD(&eq->flowctrl_list);

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

	if (eq->eqe_list) {
		PTL_FASTLOCK_DESTROY(&eq->eqe_list->lock);
		free(eq->eqe_list);
	}
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
	struct eqe_list *eqe_list;

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

	/* Allocate memory for event circular buffer. Add some extra room
	 * to account for PTs with flow control. See implementation note
	 * 19 for PtlEQAlloc(). */
	eq->count_simple = count;
	count += ni->limits.max_pt_index + 1;

	eq->eqe_list_size = sizeof(struct eqe_list) + count * sizeof(eqe_t);
	eq->eqe_list = calloc(1, eq->eqe_list_size);
	if (!eq->eqe_list) {
		err = PTL_NO_SPACE;
		(void)__sync_fetch_and_sub(&ni->current.max_eqs, 1);
		eq_put(eq);
		goto err2;
	}

	eqe_list = eq->eqe_list;

	eqe_list->producer = 0;
	eqe_list->consumer = 0;
	eqe_list->used = 0;
	eqe_list->prod_gen = 0;
	eqe_list->cons_gen = 0;
	eqe_list->interrupt = 0;
	eqe_list->count = count;

	PTL_FASTLOCK_INIT_SHARED(&eqe_list->lock);

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
	eq->eqe_list->interrupt = 1;
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

	err = PtlEQGet_work(eq->eqe_list, event_p);

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

	err = PtlEQWait_work(eq->eqe_list, event_p);

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
	eq_t *eqs[size];
	struct eqe_list *eqes_list[size];
	int i;
	int i2;

#ifndef NO_ARG_VALIDATION
	ni_t *ni = NULL;

	err = gbl_get();
	if (err)
		goto err0;
#endif

	if (size == 0) {
		err = PTL_ARG_INVALID;
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
		eqes_list[i] = eqs[i]->eqe_list;

		i2 = i;

		if (i == 0)
			ni = obj_to_ni(eqs[0]);

		if (ni != obj_to_ni(eqs[i])) {
			err = PTL_ARG_INVALID;
			goto err2;
		}
	}
#else
	for (i = 0; i < size; i++) {
		eqs[i] = fast_to_obj(eq_handles[i]);
		eqes_list[i] = eqs[i]->eqe_list;
	}
	i2 = size - 1;
#endif

	err = PtlEQPoll_work(eqes_list, size,
						 timeout, event_p, which_p);

#ifndef NO_ARG_VALIDATION
 err2:
#endif
	for (i = i2; i >= 0; i--)
		eq_put(eqs[i]);
 err1:
#ifndef NO_ARG_VALIDATION
	gbl_put();
 err0:
#endif
	return err;
}

static inline ptl_event_t *reserve_ev(eq_t * restrict eq)
{
	ptl_event_t *ev;
	eqe_t *eqe;
	struct eqe_list *eqe_list;

	eqe_list = eq->eqe_list;
	eqe = &eqe_list->eqe[eqe_list->producer];

	eqe->generation = eqe_list->prod_gen;
	ev = &eqe->event;

	eqe_list->producer ++;
	if (eqe_list->producer >= eqe_list->count) {
		eqe_list->producer = 0;
		eqe_list->prod_gen++;
	}

	eqe_list->used ++;

	/* If all unreserved entries are used, then the queue is
	 * overflowing. It matters only if an attached PT wants flow
	 * control. TODO: we should not be counting already inserted
	 * reserved entries. */
	if (eqe_list->used == eq->count_simple &&
		!list_empty(&eq->flowctrl_list)) {
		eq->overflowing = 1;
	}

	return ev;
}

/* Overflow situation. The EQ lock must be taken. */
static void process_overflowing(eq_t * restrict eq)
{
	/* If the EQ is overflowing, warn every PT not already stopped by
	 * using one of the reserved EQ entries. */
	struct list_head *l;
	pt_t *pt;
	ptl_event_t *ev;

	assert(eq->overflowing);

	list_for_each(l, &eq->flowctrl_list) {
		pt = list_entry(l, pt_t, flowctrl_list);

		if (pt->state == PT_ENABLED) {
			pt->state = PT_AUTO_DISABLED;

			/* Note/TODO: this will take a reserved entry but it
			 * will be counted as a regular entry later. */
			ev = reserve_ev(eq);

			ev->type = PTL_EVENT_PT_DISABLED;
			ev->pt_index = pt->index;
			ev->ni_fail_type = PTL_NI_PT_DISABLED;
		}
	}
	
	eq->overflowing = 0;
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

	PTL_FASTLOCK_LOCK(&eq->eqe_list->lock);

	ev = reserve_ev(eq);
	ev->type		= type;
	ev->user_ptr		= buf->user_ptr;

	if (type == PTL_EVENT_SEND) {
		/* The buf->ni_fail field can be overriden by the target side
		 * before we reach this function. If the message has been
		 * delivered then sending it was OK. */
		if (buf->ni_fail == PTL_NI_UNDELIVERABLE ||
			buf->ni_fail == PTL_NI_PT_DISABLED)
			ev->ni_fail_type = buf->ni_fail;
		else
			ev->ni_fail_type = PTL_NI_OK;
	} else {
		ev->mlength		= buf->mlength;
		ev->remote_offset	= buf->moffset;
		ev->ni_fail_type	= buf->ni_fail;
		ev->ptl_list		= buf->matching_list;
	}

	/* If the EQ is overflowing, warn every PT not already stopped by
	 * using one of the reserved EQ entries. */
	if (eq->overflowing)
		process_overflowing(eq);

	PTL_FASTLOCK_UNLOCK(&eq->eqe_list->lock);
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
	ev->ptl_list		= buf->matching_list;
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
	PTL_FASTLOCK_LOCK(&eq->eqe_list->lock);

	*(reserve_ev(eq)) = *ev;

	if (eq->overflowing)
		process_overflowing(eq);

	PTL_FASTLOCK_UNLOCK(&eq->eqe_list->lock);
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

	PTL_FASTLOCK_LOCK(&eq->eqe_list->lock);

	ev = reserve_ev(eq);

	fill_target_event(buf, type, user_ptr, start, ev);

	if (eq->overflowing)
		process_overflowing(eq);

	PTL_FASTLOCK_UNLOCK(&eq->eqe_list->lock);
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

	PTL_FASTLOCK_LOCK(&eq->eqe_list->lock);

	ev = reserve_ev(eq);
	ev->type = type;
	ev->pt_index = le->pt_index;
	ev->user_ptr = le->user_ptr;
	ev->ni_fail_type = fail_type;

	if (eq->overflowing)
		process_overflowing(eq);

	PTL_FASTLOCK_UNLOCK(&eq->eqe_list->lock);
}
