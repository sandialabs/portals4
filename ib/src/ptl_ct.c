/*
 * ptl_ct.c - Portals API
 */

#include "ptl_loc.h"

void ct_release(void *arg)
{
	ct_t *ct = arg;
	ni_t *ni = to_ni(ct);

	pthread_spin_lock(&ni->ct_list_lock);
	list_del(&ct->list);
	pthread_spin_unlock(&ni->ct_list_lock);

	pthread_spin_lock(&ni->obj.obj_lock);
	ni->current.max_cts--;
	pthread_spin_unlock(&ni->obj.obj_lock);
}

void post_ct(xi_t *xi, ct_t *ct)
{
	pthread_spin_lock(&ct->obj.obj_lock);
	if ((ct->event.success + ct->event.failure) >= xi->threshold) {
		pthread_spin_unlock(&ct->obj.obj_lock);
		process_init(xi);
		return;
	}
	list_add(&xi->list, &ct->xi_list);
	pthread_spin_unlock(&ct->obj.obj_lock);
}

/* caller must hold the CT mutex handling Wait/Poll */
static void ct_check(ct_t *ct)
{
	struct list_head *l;
	xi_t *xi;
	ni_t *ni = to_ni(ct);

	if (ni->ct_waiting)
		pthread_cond_broadcast(&ni->ct_wait_cond);

	list_for_each_prev(l, &ct->xi_list) {
		xi = list_entry(l, xi_t, list);
		if ((ct->event.success + ct->event.failure) >= xi->threshold) {
			list_del(l);
			process_init(xi);
		}
	}
}

void make_ct_event(ct_t *ct, ptl_ni_fail_t ni_fail, ptl_size_t length,
		   int bytes)
{
	ni_t *ni = to_ni(ct);

	/* Must take mutex because of poll API */
	pthread_mutex_lock(&ni->ct_wait_mutex);
	pthread_spin_lock(&ct->obj.obj_lock);
	if (ni_fail)
		ct->event.failure++;
	else
		ct->event.success += bytes ? length : 1;
	ct_check(ct);
	pthread_spin_unlock(&ct->obj.obj_lock);
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

	err = ni_get(ni_handle, &ni);
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

	err = ct_get(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni = to_ni(ct);
	pthread_mutex_lock(&ni->ct_wait_mutex);
	if (ni->ct_waiting) {
		ct->interrupt = 1;
		pthread_cond_broadcast(&ni->ct_wait_cond);
	}
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

	err = ct_get(ct_handle, &ct);
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
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ct_get(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* Check first */
	pthread_spin_lock(&ct->obj.obj_lock);
	if ((ct->event.success + ct->event.failure) >= test) {
		*event = ct->event;
		pthread_spin_unlock(&ct->obj.obj_lock);
		goto done;
	}
	pthread_spin_unlock(&ct->obj.obj_lock);

	/* Conditionally block until interrupted or CT test succeeds */
	ni = to_ni(ct);
	pthread_mutex_lock(&ni->ct_wait_mutex);
	while (1) {
		pthread_spin_lock(&ct->obj.obj_lock);
		if ((ct->event.success + ct->event.failure) >= test) {
			*event = ct->event;
			pthread_spin_unlock(&ct->obj.obj_lock);
			pthread_mutex_unlock(&ni->ct_wait_mutex);
			goto done;
		}
		pthread_spin_unlock(&ct->obj.obj_lock);
		ni->ct_waiting++;
		pthread_cond_wait(&ni->ct_wait_cond, &ni->ct_wait_mutex);
		ni->ct_waiting--;
		if (ct->interrupt) {
			pthread_mutex_unlock(&ni->ct_wait_mutex);
			err = PTL_INTERRUPTED;
			goto err2;
		}
	}
	pthread_mutex_unlock(&ni->ct_wait_mutex);

done:
	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

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
	ct_t **ct = NULL;
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

	ct = malloc(size*sizeof(*ct));
	if (!ct) {
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
		err = ct_get(ct_handles[i], &ct[i]);
		if (unlikely(err || !ct[i])) {
			WARN();
			err = PTL_ARG_INVALID;
			goto done2;
		}

		if (i == 0)
			ni = to_ni(ct[0]);
		else
			if (to_ni(ct[i]) != ni) {
				WARN();
				ct_put(ct[i]);
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
	err = 0;
        while (!err) {
                for (j = 0; j < size; j++) {
                        pthread_spin_lock(&ct[j]->obj.obj_lock);
                        if (ct[j]->interrupt) {
                                err = PTL_INTERRUPTED;
                                pthread_spin_unlock(&ct[j]->obj.obj_lock);
                                pthread_mutex_unlock(&ni->ct_wait_mutex);
                                goto done2;
                        }

                        if ((ct[j]->event.success +
                                ct[j]->event.failure) >= tests[j]) {
                                *event = ct[j]->event;
                                *which = j;
                                pthread_spin_unlock(&ct[j]->obj.obj_lock);
                                pthread_mutex_unlock(&ni->ct_wait_mutex);
                                goto done2;
                        }
                        pthread_spin_unlock(&ct[j]->obj.obj_lock);
                }

                ni->ct_waiting++;
                if (timeout == PTL_TIME_FOREVER)
                        pthread_cond_wait(&ni->ct_wait_cond,
                                &ni->ct_wait_mutex);
                else
                        err = pthread_cond_timedwait(&ni->ct_wait_cond,
                                &ni->ct_wait_mutex, &expire);
                ni->ct_waiting--;
        }
	pthread_mutex_unlock(&ni->ct_wait_mutex);

	err = PTL_CT_NONE_REACHED;

done2:
	for (j = 0; j < i; j++)
		ct_put(ct[j]);

done:
	if (ct)
		free(ct);

	gbl_put(gbl);
	return err;
}
	      

int PtlCTSet(ptl_handle_ct_t ct_handle,
	     ptl_ct_event_t new_ct)
{
	int err;
	gbl_t *gbl;
	ct_t *ct;
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ct_get(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* Must take mutex because of poll API */
	ni = to_ni(ct);
	pthread_mutex_lock(&ni->ct_wait_mutex);
	pthread_spin_lock(&ct->obj.obj_lock);
	ct->event = new_ct;

	ct_check(ct);
	pthread_spin_unlock(&ct->obj.obj_lock);
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
	int err;
	gbl_t *gbl;
	ct_t *ct;
	ni_t *ni;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ct_get(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* Must take mutex because of poll API */
	ni = to_ni(ct);
	pthread_mutex_lock(&ni->ct_wait_mutex);
	pthread_spin_lock(&ct->obj.obj_lock);
	ct->event.success += increment.success;
	ct->event.failure += increment.failure;

	ct_check(ct);
	pthread_spin_unlock(&ct->obj.obj_lock);
	pthread_mutex_unlock(&ni->ct_wait_mutex);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}
