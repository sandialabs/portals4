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

	pthread_spin_lock(&ni->obj_lock);
	ni->current.max_cts--;
	pthread_spin_unlock(&ni->obj_lock);

	pthread_mutex_destroy(&ct->mutex);
	pthread_cond_destroy(&ct->cond);
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

/* caller must hold a lock */
static void ct_check(ct_t *ct)
{
	struct list_head *l;
	xi_t *xi;

	if (ct->waiting)
		pthread_cond_broadcast(&ct->cond);

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
	pthread_mutex_lock(&ct->mutex);
	if (ni_fail)
		ct->event.failure++;
	else
		ct->event.success += bytes ? length : 1;

	ct_check(ct);
	pthread_mutex_unlock(&ct->mutex);
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

	if (unlikely(CHECK_POINTER(ct_handle, ptl_handle_ct_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

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
	pthread_mutex_init(&ct->mutex, NULL);
	pthread_cond_init(&ct->cond, NULL);

	pthread_spin_lock(&ni->ct_list_lock);
	list_add(&ct->list, &ni->ct_list);
	pthread_spin_unlock(&ni->ct_list_lock);

	pthread_spin_lock(&ni->obj_lock);
	ni->current.max_cts++;
	if (unlikely(ni->current.max_cts > ni->limits.max_cts)) {
		pthread_spin_unlock(&ni->obj_lock);
		err = PTL_NO_SPACE;
		goto err3;
	}
	pthread_spin_unlock(&ni->obj_lock);

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

	if (ct->waiting) {
		ct->interrupt = 1;
		pthread_mutex_lock(&ct->mutex);
		pthread_cond_broadcast(&ct->cond);
		pthread_mutex_unlock(&ct->mutex);
	}

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

	if (unlikely(CHECK_POINTER(event, ptl_ct_event_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

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

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(CHECK_POINTER(event, ptl_ct_event_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = ct_get(ct_handle, &ct);
	if (unlikely(err))
		goto err1;

	if (unlikely(!ct)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	pthread_mutex_lock(&ct->mutex);
	while ((ct->event.success + ct->event.failure) < test) {
		ct->waiting++;
		pthread_cond_wait(&ct->cond, &ct->mutex);
		ct->waiting--;
		if (ct->interrupt) {
			pthread_mutex_unlock(&ct->mutex);
			err = PTL_INTERRUPTED;
			goto err2;
		}
	}
	pthread_mutex_unlock(&ct->mutex);

	*event = ct->event;

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err2:
	ct_put(ct);
err1:
	gbl_put(gbl);
	return err;
}

int PtlCTSet(ptl_handle_ct_t ct_handle,
	     ptl_ct_event_t new_ct)
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

	pthread_mutex_lock(&ct->mutex);
	ct->event = new_ct;

	ct_check(ct);
	pthread_mutex_unlock(&ct->mutex);

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

	pthread_mutex_lock(&ct->mutex);
	ct->event.success += increment.success;
	ct->event.failure += increment.failure;

	ct_check(ct);
	pthread_mutex_unlock(&ct->mutex);

	ct_put(ct);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}
