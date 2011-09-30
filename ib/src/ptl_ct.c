/*
 * ptl_ct.c - Portals API
 */

#include "ptl_loc.h"

/* Triggered Set and Inc need these special versions because the caller
 * already owns the ni ct mutex. */
static int PtlCTInc_lock(ptl_handle_ct_t ct_handle,
						 ptl_ct_event_t increment,
						 int do_lock);

static int PtlCTSet_lock(ptl_handle_ct_t ct_handle,
						 ptl_ct_event_t new_ct,
						 int do_lock);

void ct_cleanup(void *arg)
{
	ct_t *ct = arg;
	ni_t *ni = obj_to_ni(ct);

	pthread_spin_lock(&ni->ct_list_lock);
	list_del(&ct->list);
	pthread_spin_unlock(&ni->ct_list_lock);

	pthread_spin_lock(&ni->obj.obj_lock);
	ni->current.max_cts--;
	pthread_spin_unlock(&ni->obj.obj_lock);
}

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

static void process_triggered_local(xl_t *xl)
{
	switch(xl->op) {
	case TRIGGERED_CTSET:
		PtlCTSet_lock(xl->ct_handle, xl->value, 0);
		break;
	case TRIGGERED_CTINC:
		PtlCTInc_lock(xl->ct_handle, xl->value, 0);
		break;
	}

	free(xl);
}

void post_ct_local(xl_t *xl, ct_t *ct)
{
	/* Must take mutex because of poll API */
	pthread_mutex_lock(&ct->mutex);
	if ((ct->event.success + ct->event.failure) >= xl->threshold) {
		pthread_mutex_unlock(&ct->mutex);
		process_triggered_local(xl);
		return;
	}
	list_add(&xl->list, &ct->xl_list);
	pthread_mutex_unlock(&ct->mutex);
}

/* caller must hold the CT mutex handling Wait/Poll */
static void ct_check(ct_t *ct)
{
	struct list_head *l;
	struct list_head *t;
	ni_t *ni = obj_to_ni(ct);

	pthread_cond_broadcast(&ct->cond);
	pthread_cond_broadcast(&ni->ct_wait_cond);

	list_for_each_prev_safe(l, t, &ct->xi_list) {
		xi_t *xi = list_entry(l, xi_t, list);
		if ((ct->event.success + ct->event.failure) >= xi->threshold) {
			list_del(l);
			process_init(xi);
		}
	}

	list_for_each_prev_safe(l, t, &ct->xl_list) {
		xl_t *xl = list_entry(l, xl_t, list);
		if ((ct->event.success + ct->event.failure) >= xl->threshold) {
			list_del(l);
			process_triggered_local(xl);
		}
	}
}

void make_ct_event(ct_t *ct, ptl_ni_fail_t ni_fail, ptl_size_t length,
		   int bytes)
{
	ni_t *ni = obj_to_ni(ct);

	/* Must take mutex because of poll API */
	pthread_mutex_lock(&ni->ct_wait_mutex);
	pthread_mutex_lock(&ct->mutex);
	if (ni_fail)
		ct->event.failure++;
	else
		ct->event.success += bytes ? length : 1;
	ct_check(ct);
	pthread_mutex_unlock(&ct->mutex);
	pthread_mutex_unlock(&ni->ct_wait_mutex);
}

int PtlCTAlloc(ptl_handle_ni_t ni_handle,
	       ptl_handle_ct_t *ct_handle)
{
	int err;
	gbl_t *gbl;
	ni_t *ni;
	ct_t *ct;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ni)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = ct_alloc(ni, &ct);
	if (unlikely(err))
		goto err2;

	INIT_LIST_HEAD(&ct->xi_list);
 	INIT_LIST_HEAD(&ct->xl_list);
	pthread_cond_init(&ct->cond, NULL);
	pthread_mutex_init(&ct->mutex, NULL);

	pthread_spin_lock(&ni->ct_list_lock);
	list_add(&ct->list, &ni->ct_list);
	pthread_spin_unlock(&ni->ct_list_lock);

	pthread_spin_lock(&ni->obj.obj_lock);
	ni->current.max_cts++;
	if (unlikely(ni->current.max_cts > ni->limits.max_cts)) {
		pthread_spin_unlock(&ni->obj.obj_lock);
		err = PTL_NO_SPACE;
		goto err3;
	}
	pthread_spin_unlock(&ni->obj.obj_lock);

	*ct_handle = ct_to_handle(ct);

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err3:
	ct_put(ct);
err2:
	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}

int PtlCTFree(ptl_handle_ct_t ct_handle)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = to_ct(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = obj_to_ni(ct);

	ct->interrupt = 1;
	pthread_cond_broadcast(&ct->cond);

	pthread_mutex_lock(&ni->ct_wait_mutex);
	pthread_cond_broadcast(&ni->ct_wait_cond);
	pthread_mutex_unlock(&ni->ct_wait_mutex);

	ct_put(ct);
	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

int PtlCTGet(ptl_handle_ct_t ct_handle,
	     ptl_ct_event_t *event)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = to_ct(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	*event = ct->event;

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

int PtlCTWait(ptl_handle_ct_t ct_handle,
	      uint64_t test,
	      ptl_ct_event_t *event)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = to_ct(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	err = PTL_OK;

	/* Conditionally block until interrupted or CT test succeeds */
	pthread_mutex_lock(&ct->mutex);

	while (!ct->event.failure) {
		if ((ct->event.success) >= test) {
			if (event)
				*event = ct->event;
			break;
		}

		pthread_cond_wait(&ct->cond, &ct->mutex);

		if (ct->interrupt) {
			err = PTL_INTERRUPTED;
			break;
		}
	}
	pthread_mutex_unlock(&ct->mutex);

 err2:
	ct_put(ct);
 err1:
	gbl_put(gbl);
	return err;
}

int PtlCTPoll(ptl_handle_ct_t *ct_handles,
			  ptl_size_t *tests,
			  unsigned int size,
			  ptl_time_t timeout,
			  ptl_ct_event_t *event,
			  unsigned int *which)
{
	int err;
	gbl_t *gbl;
	ni_t *ni = NULL;
	ct_t **cts = NULL;
	struct timespec expire;
	int i;
	int j;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (size == 0) {
		WARN();
		err = PTL_ARG_INVALID;
		goto done;
	}

	cts = malloc(size*sizeof(*cts));
	if (!cts) {
		WARN();
		err = PTL_NO_SPACE;
		goto done;
	}

	/*
	 * convert handles to pointers
	 * check that all handles are OK and that
	 * they all belong to the same NI
	 */
	for (i = 0; i < size; i++) {
		err = to_ct(ct_handles[i], &cts[i]);
		if (unlikely(err || !cts[i])) {
			WARN();
			err = PTL_ARG_INVALID;
			goto done2;
		}

		if (i == 0)
			ni = obj_to_ni(cts[0]);
		else
			if (obj_to_ni(cts[i]) != ni) {
				WARN();
				ct_put(cts[i]);
				err = PTL_ARG_INVALID;
				goto done2;
			}
	}

	if (timeout != PTL_TIME_FOREVER) {
		clock_gettime(CLOCK_REALTIME, &expire);
		expire.tv_nsec += 1000000UL * timeout;
		expire.tv_sec += expire.tv_nsec/1000000000UL;
		expire.tv_nsec = expire.tv_nsec % 1000000000UL;
	}

	pthread_mutex_lock(&ni->ct_wait_mutex);

	while (1) {
		for (j = 0; j < size; j++) {
			ct_t *ct = cts[j];

			pthread_mutex_lock(&ct->mutex);
			if (ct->interrupt) {
				err = PTL_INTERRUPTED;
				pthread_mutex_unlock(&ct->mutex);
				goto done3;
			}

			if (ct->event.failure) {
				err = PTL_OK;
				pthread_mutex_unlock(&ct->mutex);
				goto done3;
			}

			if (ct->event.success >= tests[j]) {
				err = PTL_OK;
				*event = ct->event;
				*which = j;
				pthread_mutex_unlock(&ct->mutex);
				goto done3;
			}
			pthread_mutex_unlock(&ct->mutex);
		}

		if (timeout == PTL_TIME_FOREVER)
			pthread_cond_wait(&ni->ct_wait_cond,
							  &ni->ct_wait_mutex);
		else {
			err = pthread_cond_timedwait(&ni->ct_wait_cond,
										 &ni->ct_wait_mutex, &expire);
			if (err == ETIMEDOUT) {
				err = PTL_CT_NONE_REACHED;
				break;
			}
		}
	}

 done3:
	pthread_mutex_unlock(&ni->ct_wait_mutex);

 done2:
	for (j = 0; j < i; j++)
		ct_put(cts[j]);

 done:
	if (cts)
		free(cts);

	gbl_put(gbl);

	return err;
}

static int PtlCTSet_lock(ptl_handle_ct_t ct_handle,
				  ptl_ct_event_t new_ct,
				  int do_lock)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = to_ct(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* Must take mutex because of poll API */
	ni = obj_to_ni(ct);
	if (do_lock)
		pthread_mutex_lock(&ni->ct_wait_mutex);
	pthread_mutex_lock(&ct->mutex);
	ct->event = new_ct;
	ct_check(ct);
	pthread_mutex_unlock(&ct->mutex);
	if (do_lock)
		pthread_mutex_unlock(&ni->ct_wait_mutex);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

int PtlCTSet(ptl_handle_ct_t ct_handle,
			 ptl_ct_event_t new_ct)
{
	return PtlCTSet_lock(ct_handle, new_ct, 1);
}

static int PtlCTInc_lock(ptl_handle_ct_t ct_handle,
						 ptl_ct_event_t increment,
						 int do_lock)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;
	ni_t *ni;

#ifndef NO_ARG_VALIDATION
	if (increment.success != 0 && increment.failure != 0)
		return PTL_ARG_INVALID;
#endif

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = to_ct(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* Must take mutex because of poll API */
	ni = obj_to_ni(ct);
	if (do_lock)
		pthread_mutex_lock(&ni->ct_wait_mutex);
	pthread_mutex_lock(&ct->mutex);
	ct->event.success += increment.success;
	ct->event.failure += increment.failure;
	ct_check(ct);
	pthread_mutex_unlock(&ct->mutex);
	if (do_lock)
		pthread_mutex_unlock(&ni->ct_wait_mutex);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

int PtlCTInc(ptl_handle_ct_t ct_handle,
			 ptl_ct_event_t increment)
{
	return PtlCTInc_lock(ct_handle, increment, 1);
}

int PtlCTCancelTriggered(ptl_handle_ct_t ct_handle)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;
	ni_t *ni;
	struct list_head *l;
	struct list_head *t;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = to_ct(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = obj_to_ni(ct);

	pthread_mutex_lock(&ct->mutex);

	list_for_each_prev_safe(l, t, &ct->xi_list) {
		xi_t *xi = list_entry(l, xi_t, list);
		list_del(l);

		if (xi->get_md) {
			md_put(xi->get_md);
			xi->get_md = NULL;
		}

		if (xi->put_md) {
			md_put(xi->put_md);
			xi->put_md = NULL;
		}

		xi_put(xi);
	}

	list_for_each_prev_safe(l, t, &ct->xl_list) {
		xl_t *xl = list_entry(l, xl_t, list);
		list_del(l);
		free(xl);
	}

	pthread_mutex_unlock(&ct->mutex);

	err = PTL_OK;

	ct_put(ct);
	
 err1:
 	gbl_put(gbl);
 	return err;
}
