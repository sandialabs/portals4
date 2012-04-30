/**
 * @file ptl_pt.c
 *
 * This file contains the implementation of
 * pt (portals table) class methods.
 */

#include "ptl_loc.h"

/**
 * Get pt index.
 *
 * @pre caller should hold ni->pt_mutex
 *
 * @param[in] ni for which to get index
 * @param[in] req requested index or PTL_PT_ANY
 * @param[out] index_p address of returned value
 *
 * @return PTL_OK		on success
 * @return PTL_PT_IN_USE	if req is already in use
 * @return PTL_PT_FULL		if no index available
 * @return PTL_ARG_INVALID	if req is out of range
 */
static int get_pt_index(ni_t *ni, ptl_pt_index_t req,
			ptl_pt_index_t *index_p)
{
	const ptl_pt_index_t max = ni->limits.max_pt_index;
	ptl_pt_index_t index;

	if (req != PTL_PT_ANY) {
		if (req <= max) {
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
	for (index = ni->last_pt + 1; index <= max; index++) {
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

/**
 * Allocate pt entry.
 *
 * @param[in] ni_handle of ni for which to allocate entry
 * @param[in] options for entry
 * @param[in] eq_handle of eq to associate with entry or PTL_EQ_NONE
 * @paran[in] pt_index_req requested pt index for entry or PTL_PT_ANY
 * @param[out] pt_index address of return value
 *
 * @return PTL_OK		on success
 * @return PTL_NO_INIT		if PtlInit has not been called
 * @return PTL_PT_IN_USE	if req is already in use
 * @return PTL_PT_FULL		if no index available
 * @return PTL_PT_EQ_NEEDED	if pt is flow controlled and no eq is provided
 * @return PTL_ARG_INVALID	if req is out of range, a handle is invalid,
 * 				or options is not supported
 */
int PtlPTAlloc(ptl_handle_ni_t ni_handle,
	       unsigned int options,
	       ptl_handle_eq_t eq_handle,
	       ptl_pt_index_t pt_index_req,
	       ptl_pt_index_t *pt_index)
{
	int err;
	pt_t *pt;
	ni_t *ni;
	ptl_pt_index_t index = 0;
	eq_t *eq;

	err = gbl_get();
	if (unlikely(err))
		return err;

	if (unlikely(options & ~PTL_PT_ALLOC_OPTIONS_MASK)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = to_eq(eq_handle, &eq);
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

	pt->index = index;
	pt->state = PT_ENABLED;
	pt->num_tgt_active = 0;
	pt->options = options;
	pt->eq = eq;

	PTL_FASTLOCK_INIT(&pt->lock);
	INIT_LIST_HEAD(&pt->priority_list);
	INIT_LIST_HEAD(&pt->overflow_list);
	INIT_LIST_HEAD(&pt->unexpected_list);
	INIT_LIST_HEAD(&pt->flowctrl_list);

	if (options & PTL_PT_FLOWCTRL) {
		PTL_FASTLOCK_LOCK(&eq->lock);
		list_add_tail(&pt->flowctrl_list, &eq->flowctrl_list);
		PTL_FASTLOCK_UNLOCK(&eq->lock);
	}

	*pt_index = index;

	ni_put(ni);
	gbl_put();
	return PTL_OK;

err3:
	if (eq)
		eq_put(eq);
err2:
	ni_put(ni);
err1:
	gbl_put();
	return err;
}

/**
 * Free pt entry.
 *
 * @pre pt entry must have been allocated by a call to PtlPTAlloc
 *
 * @param ni_handle of ni for which to free entry
 * @param pt_entry of entry to free
 *
 * @return PTL_OK		on success
 * @return PTL_NO_INIT		if PtlInit has not been called
 * @return PTL_PT_IN_USE	if pt entry is busy, i.e. it has
 * 				a list or matching element on one
 * 				of its lists
 * @return PTL_ARG_INVALID	if req is out of range or ni handle is invalid
 */
int PtlPTFree(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index)
{
	int err;
	ni_t *ni;
	pt_t *pt;

	err = gbl_get();
	if (unlikely(err))
		return err;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	pthread_mutex_lock(&ni->pt_mutex);

	if (unlikely(pt_index > ni->limits.max_pt_index ||
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

	PTL_FASTLOCK_DESTROY(&pt->lock);

	pt->in_use = 0;
	pt->state = PT_DISABLED;

	if (pt->eq)
		eq_put(pt->eq);

	pthread_mutex_unlock(&ni->pt_mutex);

	ni_put(ni);
	gbl_put();
	return PTL_OK;

err2:
	ni_put(ni);
err1:
	gbl_put();
	return err;
}

/**
 * Disable pt entry.
 *
 * @pre pt entry must have been allocated by a call to PtlPTAlloc
 *
 * @param ni_handle of ni for which to disable entry
 * @param pt_index of entry to disable
 *
 * @return PTL_OK		on success
 * @return PTL_NO_INIT		if PtlInit has not been called
 * @return PTL_ARG_INVALID	if req is out of range, ni handle is invalid,
 * 				or pt entry is not allocated
 */
int PtlPTDisable(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index)
{
	int err;
	ni_t *ni;
	pt_t *pt;

	err = gbl_get();
	if (unlikely(err))
		return err;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (unlikely(pt_index > ni->limits.max_pt_index ||
	    !ni->pt[pt_index].in_use)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	pt = &ni->pt[pt_index];

	if (!pt->in_use) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	/* Serialize with progress to let active target processing complete */
	PTL_FASTLOCK_LOCK(&pt->lock);
	pt->state = PT_DISABLED;
	while(pt->num_tgt_active) {
		PTL_FASTLOCK_UNLOCK(&pt->lock);
		sched_yield();
		PTL_FASTLOCK_LOCK(&pt->lock);
	}
	PTL_FASTLOCK_UNLOCK(&pt->lock);

	ni_put(ni);
	gbl_put();
	return PTL_OK;

err2:
	ni_put(ni);
err1:
	gbl_put();
	return err;
}

/**
 * Enable pt entry.
 *
 * @pre pt entry must have been allocated by a call to PtlPTAlloc
 *
 * @param ni_handle of ni for which to enable entry
 * @param pt_index of entry to enable
 *
 * @return PTL_OK		on success
 * @return PTL_NO_INIT		if PtlInit has not been called
 * @return PTL_ARG_INVALID	if req is out of range, ni handle is invalid,
 * 				or pt entry is not allocated
 */
int PtlPTEnable(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index)
{
	int err;
	ni_t *ni;
	pt_t *pt;

	err = gbl_get();
	if (unlikely(err))
		return err;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (unlikely(pt_index > ni->limits.max_pt_index ||
	    !ni->pt[pt_index].in_use)) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	pt = &ni->pt[pt_index];

	if (!pt->in_use) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	/* Serialize with disable operations */
	PTL_FASTLOCK_LOCK(&pt->lock);
	if (pt->state != PT_ENABLED) {
		PTL_FASTLOCK_UNLOCK(&pt->lock);
		sched_yield();
		PTL_FASTLOCK_LOCK(&pt->lock);
	}
	pt->state = PT_ENABLED;
	PTL_FASTLOCK_UNLOCK(&pt->lock);

	ni_put(ni);
	gbl_put();
	return PTL_OK;

err2:
	ni_put(ni);
err1:
	gbl_put();
	return err;
}
