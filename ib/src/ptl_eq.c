/*
 * ptl.c - Portals API
 */

#include "ptl_loc.h"

void eq_release(void *arg)
{
        eq_t *eq = arg;
	ni_t *ni = to_ni(eq);

	pthread_spin_lock(&ni->obj_lock);
	ni->current.max_eqs--;
	pthread_spin_unlock(&ni->obj_lock);

	if (eq->eqe_list)
		free(eq->eqe_list);
	eq->eqe_list = NULL;

	pthread_mutex_destroy(&eq->mutex);
	pthread_cond_destroy(&eq->cond);
}

/* can return
PTL_OK
PTL_NO_INIT
PTL_ARG_INVALID
PTL_NO_SPACE
PTL_ARG_INVALID
*/
int PtlEQAlloc(ptl_handle_ni_t ni_handle,
	       ptl_size_t count,
	       ptl_handle_eq_t *eq_handle)
{
	int err;
	gbl_t *gbl;
	ni_t *ni;
	eq_t *eq;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(CHECK_POINTER(eq_handle, ptl_handle_eq_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = ni_get(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = eq_alloc(ni, &eq);
	if (unlikely(err))
		goto err2;

	eq->eqe_list = calloc(count, sizeof(*eq->eqe_list));
	if (!eq->eqe_list) {
		err = PTL_NO_SPACE;
		goto err3;
	}

	eq->count = count;
	eq->producer = 0;
	eq->consumer = 0;
	eq->prod_gen = 0;
	eq->cons_gen = 0;
	pthread_mutex_init(&eq->mutex, NULL);
	pthread_cond_init(&eq->cond, NULL);

	pthread_spin_lock(&ni->obj_lock);
	ni->current.max_eqs++;
	/* eq_release will decrement count and free eqe_list */
	if (unlikely(ni->current.max_eqs > ni->limits.max_eqs)) {
		pthread_spin_unlock(&ni->obj_lock);
		err = PTL_NO_SPACE;
		goto err3;
	}
	pthread_spin_unlock(&ni->obj_lock);

	// TODO add to a list of eqs somewhere

	*eq_handle = eq_to_handle(eq);

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err3:
	eq_put(eq);
err2:
	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}

/* can return
PTL_OK
PTL_NO_INIT
PTL_ARG_INVALID
*/
int PtlEQFree(ptl_handle_eq_t eq_handle)
{
	int err;
	gbl_t *gbl;
	eq_t *eq;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = eq_get(eq_handle, &eq);
	if (unlikely(err))
		goto err1;

	if (!eq) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (eq->waiting) {
		eq->interrupt = 1;
		pthread_mutex_lock(&eq->mutex);
		pthread_cond_broadcast(&eq->cond);
		pthread_mutex_unlock(&eq->mutex);
	}

	eq_put(eq);
	eq_put(eq);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

/* Must hold eq->mutex outside of this call */
int get_event(eq_t *eq, ptl_event_t *event)
{
	if ((eq->producer == eq->consumer) &&
	    (eq->prod_gen == eq->cons_gen)) {
		return PTL_EQ_EMPTY;
	}

	//TODO detect dropped events
	*event = eq->eqe_list[eq->consumer++].event;
	if (eq->consumer >= eq->count) {
		eq->consumer = 0;
		eq->cons_gen++;
	}
	return PTL_OK;
}

int PtlEQGet(ptl_handle_eq_t eq_handle,
	     ptl_event_t *event)
{
	int err;
	gbl_t *gbl;
	eq_t *eq;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(CHECK_POINTER(event, ptl_event_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = eq_get(eq_handle, &eq);
	if (unlikely(err))
		goto err1;

	if (!eq) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	pthread_mutex_lock(&eq->mutex);
	err = get_event(eq, event);
	pthread_mutex_unlock(&eq->mutex);

	eq_put(eq);
	gbl_put(gbl);
	return err;

err1:
	gbl_put(gbl);
	return err;
}

int PtlEQWait(ptl_handle_eq_t eq_handle,
	      ptl_event_t *event)
{
	int err;
	gbl_t *gbl;
	eq_t *eq;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(CHECK_POINTER(event, ptl_event_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = eq_get(eq_handle, &eq);
	if (unlikely(err))
		goto err1;

	if (!eq) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	pthread_mutex_lock(&eq->mutex);
	while((err = get_event(eq, event)) == PTL_EQ_EMPTY) {
		eq->waiting++;
		pthread_cond_wait(&eq->cond, &eq->mutex);
		eq->waiting--;
		if (eq->interrupt) {
			pthread_mutex_unlock(&eq->mutex);
			err = PTL_INTERRUPTED;
			goto err2;
		}
	}
	pthread_mutex_unlock(&eq->mutex);

err2:
	eq_put(eq);
	gbl_put(gbl);
	return err;

err1:
	gbl_put(gbl);
	return err;
}

int PtlEQPoll(ptl_handle_eq_t *eq_handles,
	      int size,
	      ptl_time_t timeout,
	      ptl_event_t *event,
	      int *which)
{
	int err;
	gbl_t *gbl;
	eq_t *eq;
	int i;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(CHECK_POINTER(event, ptl_event_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(CHECK_POINTER(which, int))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	// TODO try harder

	for (i = 0; i < size; i++) {
		err = eq_get(eq_handles[i], &eq);
		if (unlikely(err))
			goto err1;

		if (!eq) {
			err = PTL_ARG_INVALID;
			goto err1;
		}

		pthread_mutex_lock(&eq->mutex);
		if (get_event(eq, event) == PTL_OK) {
			pthread_mutex_unlock(&eq->mutex);
			*which = i;
			eq_put(eq);
			goto done;
		}
		pthread_mutex_unlock(&eq->mutex);

		eq_put(eq);
	}

	err = PTL_EQ_EMPTY;
	goto err1;

done:
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

void event_dump(ptl_event_t *ev)
{
	printf("ev:		%p\n", ev);
	printf("ev->type:	%d\n", ev->type);
	printf("\n");
}

int make_init_event(xi_t *xi, eq_t *eq, ptl_event_kind_t type, void *start)
{
	ptl_event_t *ev;

	/* TODO check to see if there is room in the eq */

	pthread_mutex_lock(&eq->mutex);
	eq->eqe_list[eq->producer].generation = eq->prod_gen;
	ev = &eq->eqe_list[eq->producer].event;

	ev->type		= type;
	ev->initiator		= xi->target;
	ev->pt_index		= xi->pt_index;
	ev->uid			= xi->uid;
	ev->jid			= xi->jid;
	ev->match_bits		= xi->match_bits;
	ev->rlength		= xi->rlength;
	ev->mlength		= xi->mlength;
	ev->remote_offset	= xi->moffset;
	ev->start		= start;
	ev->user_ptr		= xi->user_ptr;
	ev->hdr_data		= xi->hdr_data;
	ev->ni_fail_type	= xi->ni_fail;
	ev->atomic_operation	= xi->atom_op;
	ev->atomic_type		= xi->atom_type;

	eq->producer++;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}
	if (eq->waiting)
		pthread_cond_broadcast(&eq->cond);

	pthread_mutex_unlock(&eq->mutex);

	if (debug) event_dump(ev);

	return PTL_OK;
}

int make_target_event(xt_t *xt, eq_t *eq, ptl_event_kind_t type, void *start)
{
	ptl_event_t *ev;

	/* TODO check to see if there is room in the eq */

	pthread_mutex_lock(&eq->mutex);
	eq->eqe_list[eq->producer].generation = eq->prod_gen;
	ev = &eq->eqe_list[eq->producer].event;

	ev->type		= type;
	ev->initiator		= xt->initiator;
	ev->pt_index		= xt->pt_index;
	ev->uid			= xt->uid;
	ev->jid			= xt->jid;
	ev->match_bits		= xt->match_bits;
	ev->rlength		= xt->rlength;
	ev->mlength		= xt->mlength;
	ev->remote_offset	= xt->roffset;
	ev->start		= start;
	ev->user_ptr		= xt->le->user_ptr;
	ev->hdr_data		= xt->hdr_data;
	ev->ni_fail_type	= xt->ni_fail;
	ev->atomic_operation	= xt->atom_op;
	ev->atomic_type		= xt->atom_type;

	eq->producer++;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}
	if (eq->waiting)
		pthread_cond_broadcast(&eq->cond);

	pthread_mutex_unlock(&eq->mutex);

	if (debug) event_dump(ev);

	return PTL_OK;
}
