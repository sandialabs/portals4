/*
 * ptl.c - Portals API
 */

#include "ptl_loc.h"

void eq_release(void *arg)
{
	eq_t *eq = arg;
	ni_t *ni = to_ni(eq);

	pthread_spin_lock(&ni->obj.obj_lock);
	ni->current.max_eqs--;
	pthread_spin_unlock(&ni->obj.obj_lock);

	if (eq->eqe_list)
		free(eq->eqe_list);
	eq->eqe_list = NULL;
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
	eq->overflow = 0;
	eq->interrupt = 0;

	pthread_spin_lock(&ni->obj.obj_lock);
	ni->current.max_eqs++;
	/* eq_release will decrement count and free eqe_list */
	if (unlikely(ni->current.max_eqs > ni->limits.max_eqs)) {
		pthread_spin_unlock(&ni->obj.obj_lock);
		err = PTL_NO_SPACE;
		goto err3;
	}
	pthread_spin_unlock(&ni->obj.obj_lock);

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
	ni_t *ni;

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

	ni = to_ni(eq);
	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (ni->eq_waiting) {
		eq->interrupt = 1;
		pthread_mutex_lock(&ni->eq_wait_mutex);
		pthread_cond_broadcast(&ni->eq_wait_cond);
		pthread_mutex_unlock(&ni->eq_wait_mutex);
	}

	eq_put(eq);
	eq_put(eq);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

int get_event(eq_t *eq, ptl_event_t *event)
{
	int ret;

	pthread_spin_lock(&eq->obj.obj_lock);
	if ((eq->producer == eq->consumer) &&
	    (eq->prod_gen == eq->cons_gen)) {
		pthread_spin_unlock(&eq->obj.obj_lock);
		return PTL_EQ_EMPTY;
	}

	if (eq->overflow) {
		ret = PTL_EQ_DROPPED;
		eq->overflow = 0;
	} else
		ret = PTL_OK;

	*event = eq->eqe_list[eq->consumer++].event;
	if (eq->consumer >= eq->count) {
		eq->consumer = 0;
		eq->cons_gen++;
	}
	pthread_spin_unlock(&eq->obj.obj_lock);
	return ret;
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

	err = eq_get(eq_handle, &eq);
	if (unlikely(err))
		goto err1;

	if (!eq) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = get_event(eq, event);
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
	int ret;
	gbl_t *gbl;
	eq_t *eq;
	ni_t *ni;

	ret = get_gbl(&gbl);
	if (unlikely(ret))
		return ret;

	ret = eq_get(eq_handle, &eq);
	if (unlikely(ret))
		goto err1;

	if (!eq) {
		ret = PTL_ARG_INVALID;
		goto err1;
	}

	ni = to_ni(eq);
	if (!ni) {
		ret = PTL_ARG_INVALID;
		goto done;
	}

	/* First try with just spining */
	ret = get_event(eq, event);
	if (ret != PTL_EQ_EMPTY)
		goto done;

	/* Serialize for blocking on empty */
	pthread_mutex_lock(&ni->eq_wait_mutex);
	while((ret = get_event(eq, event)) == PTL_EQ_EMPTY) {
		ni->eq_waiting++;
		pthread_cond_wait(&ni->eq_wait_cond, &ni->eq_wait_mutex);
		ni->eq_waiting--;
		if (eq->interrupt) {
			pthread_mutex_unlock(&ni->eq_wait_mutex);
			ret = PTL_INTERRUPTED;
			goto done;
		}
	}
	pthread_mutex_unlock(&ni->eq_wait_mutex);

done:
	eq_put(eq);
	gbl_put(gbl);
	return ret;

err1:
	gbl_put(gbl);
	return ret;
}

int PtlEQPoll(ptl_handle_eq_t *eq_handles,
	      int size,
	      ptl_time_t timeout,
	      ptl_event_t *event,
	      int *which)
{
	int err;
	gbl_t *gbl;
	ni_t *ni;
	struct timeval time;
	struct timespec expire;
	eq_t *eq[size];
	int i = 0;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	/* First try with just spinning */
	for (i = 0; i < size; i++) {
		err = eq_get(eq_handles[i], &eq[i]);
		if (unlikely(err) || !eq[i]) {
			int j;

			for (j=0; j < i; j++)
				eq_put(eq[j]);
			err = PTL_ARG_INVALID;
			goto done;
		}

		err = get_event(eq[i], event);
		if (err == PTL_OK) {
			int j;

			*which = i;
			for (j=0; j <= i; j++)
				eq_put(eq[j]);
			goto done;
		}
	}

	ni = to_ni(eq[0]);

	if (timeout != PTL_TIME_FOREVER) {
		long usec;

		gettimeofday(&time, NULL);
		usec = time.tv_usec + (timeout % 1000) * 1000;
		expire.tv_sec = time.tv_sec + usec/1000000;
		expire.tv_nsec = (usec % 1000000) * 1000;
	}

	/* Serialize for blocking, note all EQ are from same NI */
	pthread_mutex_lock(&ni->eq_wait_mutex);
	err = 0;
	while (!err) {
		for (i = 0; i < size; i++) {
			if (eq[i]->interrupt) {
				pthread_mutex_unlock(&ni->eq_wait_mutex);
				err = PTL_INTERRUPTED;
				goto done2;
			}
			err = get_event(eq[i], event);
			if (err != PTL_EQ_EMPTY) {
				pthread_mutex_unlock(&ni->eq_wait_mutex);
				goto done2;
			}
		}

		ni->eq_waiting++;
		if (timeout == PTL_TIME_FOREVER)
			pthread_cond_wait(&ni->eq_wait_cond,
				&ni->eq_wait_mutex);
		else
			err = pthread_cond_timedwait(&ni->eq_wait_cond,
				&ni->eq_wait_mutex, &expire);
		ni->eq_waiting--;
	}
	pthread_mutex_unlock(&ni->eq_wait_mutex);
	err = PTL_EQ_EMPTY;

done2:
	for (i = 0; i < size; i++)
		eq_put(eq[i]);

done:
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
	ni_t *ni;

	pthread_spin_lock(&eq->obj.obj_lock);
	if ((eq->prod_gen != eq->cons_gen) && (eq->producer >= eq->consumer))
		eq->overflow = 1;

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
	pthread_spin_unlock(&eq->obj.obj_lock);

	/* Handle case where waiters have blocked */
	ni = to_ni(eq);
	pthread_mutex_lock(&ni->eq_wait_mutex);
	if (ni->eq_waiting)
		pthread_cond_broadcast(&ni->eq_wait_cond);
	pthread_mutex_unlock(&ni->eq_wait_mutex);

	if (debug) event_dump(ev);

	return PTL_OK;
}

int make_target_event(xt_t *xt, eq_t *eq, ptl_event_kind_t type, void *start)
{
	ptl_event_t *ev;
	ni_t *ni;

	pthread_spin_lock(&eq->obj.obj_lock);
	if ((eq->prod_gen != eq->cons_gen) && (eq->producer >= eq->consumer))
		eq->overflow = 1;

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
	ev->user_ptr		=  xt->le ? xt->le->user_ptr : NULL;
	ev->hdr_data		= xt->hdr_data;
	ev->ni_fail_type	= xt->ni_fail;
	ev->atomic_operation	= xt->atom_op;
	ev->atomic_type		= xt->atom_type;

	eq->producer++;
	if (eq->producer >= eq->count) {
		eq->producer = 0;
		eq->prod_gen++;
	}
	pthread_spin_unlock(&eq->obj.obj_lock);

	/* Handle case where waiters have blocked */
	ni = to_ni(eq);
	pthread_mutex_lock(&ni->eq_wait_mutex);
	if (ni->eq_waiting)
		pthread_cond_broadcast(&ni->eq_wait_cond);
	pthread_mutex_unlock(&ni->eq_wait_mutex);

	if (debug) event_dump(ev);

	return PTL_OK;
}
