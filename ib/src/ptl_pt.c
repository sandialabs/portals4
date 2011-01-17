/*
 * ptl_pt.c
 */

#include "ptl_loc.h"

/* caller must hold a lock which protects the list */
static int get_pt_index(ni_t *ni, ptl_pt_index_t req,
			ptl_pt_index_t *index_p)
{
	ptl_pt_index_t max = ni->limits.max_pt_index;
	ptl_pt_index_t index;

	if (req != PTL_PT_ANY) {
		if (req >= 0 && req < max) {
			if (!ni->pt[req].in_use) {
				index = req;
				goto done;
			} else {
				return PTL_PT_IN_USE;
			}
		} else {
			return PTL_ARG_INVALID;
		}
	}

	/* search for next free slot starting
	   from the last one allocated */
	for (index = ni->last_pt + 1; index < max; index++) {
		if (!ni->pt[index].in_use)
			goto done;
	}

	for (index = 0; index <= ni->last_pt; index++) {
		if (!ni->pt[index].in_use)
			goto done;
	}

	return PTL_PT_FULL;

done:
	ni->last_pt = index;
	*index_p = index;
	return PTL_OK;
}

int PtlPTAlloc(ptl_handle_ni_t ni_handle,
	       unsigned int options,
	       ptl_handle_eq_t eq_handle,
	       ptl_pt_index_t pt_index_req,
	       ptl_pt_index_t *pt_index)
{
	int err;
	pt_t *pt;
	ni_t *ni;
	ptl_pt_index_t index;
	gbl_t *gbl;
	eq_t *eq;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	if (unlikely(CHECK_POINTER(pt_index, ptl_pt_index_t))) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	if (unlikely(options & ~_PTL_PT_ALLOC_OPTIONS)) {
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

	err = eq_get(eq_handle, &eq);
	if (unlikely(err))
		goto err2;

	if (unlikely((options & PTL_PT_FLOWCTRL) && !eq)) {
		err = PTL_PT_EQ_NEEDED;
		goto err2;
	}

	pthread_mutex_lock(&ni->pt_mutex);
	err = get_pt_index(ni, pt_index_req, &index);
	if (unlikely(err)) {
		pthread_mutex_unlock(&ni->pt_mutex);
		goto err3;
	}
	pt = &ni->pt[index];
	pt->in_use = 1;
	pthread_mutex_unlock(&ni->pt_mutex);

	pt->enable = 1;
	pt->options = options;
	pt->eq = eq;

	pt->obj_type = &type_info[OBJ_TYPE_PT];
	pt->obj_parent = (obj_t *)ni;
	pt->obj_ni = ni;
	pt->obj_type = type_pt;

	pthread_spin_init(&pt->list_lock, PTHREAD_PROCESS_PRIVATE);
	INIT_LIST_HEAD(&pt->priority_list);
	INIT_LIST_HEAD(&pt->overflow_list);

	*pt_index = index;

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

int PtlPTFree(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index)
{
	int err;
	ni_t *ni;
	pt_t *pt;
	gbl_t *gbl;

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

	pthread_mutex_lock(&ni->pt_mutex);

	if (unlikely(pt_index >= ni->limits.max_pt_index ||
	    !ni->pt[pt_index].in_use)) {
		pthread_mutex_unlock(&ni->pt_mutex);
		err = PTL_ARG_INVALID;
		goto err2;
	}

	pt = &ni->pt[pt_index];

	if (!list_empty(&pt->priority_list) ||
	    !list_empty(&pt->overflow_list)) {
		pthread_mutex_unlock(&ni->pt_mutex);
		err = PTL_PT_IN_USE;
		goto err2;
	}

	pthread_spin_destroy(&pt->list_lock);
	pt->obj_parent = NULL;
	pt->obj_ni = NULL;

	pt->in_use = 0;
	pt->enable = 0;

	if (pt->eq)
		eq_put(pt->eq);

	pthread_mutex_unlock(&ni->pt_mutex);

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err2:
	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}

int PtlPTDisable(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index)
{
	int err;
	ni_t *ni;
	pt_t *pt;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ni_get(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (unlikely(pt_index >= ni->limits.max_pt_index ||
	    !ni->pt[pt_index].in_use)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	pt = &ni->pt[pt_index];

	if (!pt->in_use) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	pt->enable = 0;

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err2:
	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}

int PtlPTEnable(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index)
{
	int err;
	ni_t *ni;
	pt_t *pt;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ni_get(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (unlikely(pt_index >= ni->limits.max_pt_index ||
	    !ni->pt[pt_index].in_use)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	pt = &ni->pt[pt_index];

	if (!pt->in_use) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	pt->enable = 1;

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err2:
	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}
