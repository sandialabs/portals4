/*
 * ptl_le.c
 */

#include "ptl_loc.h"

/*
 * le_init
 *	initialize new le
 */
int le_init(void *arg, void *unused)
{
	le_t *le = arg;

	le->type = TYPE_LE;
	return 0;
}

/*
 * le_release
 *	called from le_put when the last reference is dropped
 * note:
 *	common between LE and ME
 */
void le_cleanup(void *arg)
{
	le_t *le = arg;
	ni_t *ni = obj_to_ni(le);
	pt_t *pt = le->pt;

	if (le->ct)
		ct_put(le->ct);

	if (pt)
		WARN();

	if (le->sge_list) {
		free(le->sge_list);
		le->sge_list = NULL;
	}

	if (le->sge_list_mr) {
		mr_put(le->sge_list_mr);
		le->sge_list_mr = NULL;
	}

	pthread_spin_lock(&ni->obj.obj_lock);
	ni->current.max_entries--;
	pthread_spin_unlock(&ni->obj.obj_lock);
}

/*
 * le_unlink
 *	called to unlink the entry from the PT list and remove
 *	the reference held by the PT list.
 */
void le_unlink(le_t *le, int send_event)
{
	pt_t *pt = le->pt;

	if (pt) {
		pthread_spin_lock(&pt->lock);

		/* Avoid a race between PTLMeUnlink and autounlink. */ 
		if (le->pt) {
			if (le->ptl_list == PTL_PRIORITY_LIST)
				pt->priority_size--;
			else if (le->ptl_list == PTL_OVERFLOW_LIST)
				pt->overflow_size--;
			list_del_init(&le->list);

			if (send_event && le->eq)
				make_le_event(le, le->eq,
					      PTL_EVENT_AUTO_UNLINK,
					      PTL_NI_OK);

			le->pt = NULL;
		}

		pthread_spin_unlock(&pt->lock);

		if (le->type == TYPE_ME)
			me_put((me_t *)le);
		else
			le_put(le);
	}
}

/*
 * le_get_le
 *	allocate an le after checking to see if there
 *	is room in the limit
 */
static int le_get_le(ni_t *ni, le_t **le_p)
{
	int err;
	le_t *le;

	pthread_spin_lock(&ni->obj.obj_lock);
	if (unlikely(ni->current.max_entries >= ni->limits.max_entries)) {
		pthread_spin_unlock(&ni->obj.obj_lock);
		return PTL_NO_SPACE;
	}
	ni->current.max_entries++;
	pthread_spin_unlock(&ni->obj.obj_lock);

	err = le_alloc(ni, &le);
	if (unlikely(err)) {
		pthread_spin_lock(&ni->obj.obj_lock);
		ni->current.max_entries--;
		pthread_spin_unlock(&ni->obj.obj_lock);
		return err;
	}

	*le_p = le;
	return PTL_OK;
}

/*
 * le_append_check
 *	check call parameters for PtlLEAppend
 * note:
 *	common between LE and ME
 */
int le_append_check(int type, ni_t *ni, ptl_pt_index_t pt_index,
		    const ptl_le_t *le_init, ptl_list_t ptl_list,
		    ptl_search_op_t search_op, ptl_handle_le_t *le_handle)
{
	pt_t *pt;

	if (unlikely(!ni)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	pt = &ni->pt[pt_index];

	if (!pt->in_use) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (type == TYPE_ME) {
		if (unlikely((ni->options & PTL_NI_MATCHING) == 0)) {
			WARN();
			return PTL_ARG_INVALID;
		}
	} else {
		if (unlikely((ni->options & PTL_NI_NO_MATCHING) == 0)) {
			WARN();
			return PTL_ARG_INVALID;
		}
	}

	if (unlikely(pt_index >= ni->limits.max_pt_index)) {
		WARN();
		return PTL_ARG_INVALID;
	}

	if (le_init->options & PTL_IOVEC) {
		if (le_init->length > ni->limits.max_iovecs) {
			WARN();
			return PTL_ARG_INVALID;
		}
	}

	if (type == TYPE_ME) {
		if (unlikely(le_init->options & ~PTL_ME_APPEND_OPTIONS_MASK)) {
			WARN();
			return PTL_ARG_INVALID;
		}
	} else {
		if (unlikely(le_init->options & ~PTL_LE_APPEND_OPTIONS_MASK)) {
			WARN();
			return PTL_ARG_INVALID;
		}
	}

	if (le_handle) {
		if (unlikely(ptl_list < PTL_PRIORITY_LIST ||
			     ptl_list > PTL_OVERFLOW_LIST)) {
			WARN();
			return PTL_ARG_INVALID;
		}
	} else {
		if (unlikely(search_op < PTL_SEARCH_ONLY ||
			     search_op > PTL_SEARCH_DELETE)) {
			WARN();
			return PTL_ARG_INVALID;
		}
	}

	return PTL_OK;
}

/*
 * le_get_mr
 *	allocate InfiniBand MRs for le
 * note:
 *	common between LE and ME
 */
int le_get_mr(ni_t *ni, const ptl_le_t *le_init, le_t *le)
{
	int err;
	int i;
	ptl_iovec_t *iov;
	struct ibv_sge *sge;

	if (le_init->options & PTL_IOVEC) {
		le->num_iov = le_init->length;

		if (le->num_iov > get_param(PTL_MAX_INLINE_SGE)) {
			le->sge_list = calloc(le->num_iov, sizeof(*sge));
			if (!le->sge_list)
				return PTL_NO_SPACE;

			err = mr_lookup(ni, le->sge_list,
					le->num_iov * sizeof(*sge),
					&le->sge_list_mr);
			if (err)
				return err;
		}

		le->length = 0;
		iov = (ptl_iovec_t *)le_init->start;

		for (i = 0; i < le->num_iov; i++) {
			le->length += iov->iov_len;
			iov++;
		}
	} else {
		le->length = le_init->length;
	}

	return PTL_OK;
}

/*
 * le_append_pt
 *	add le to pt entry
 *	TODO check limits on this the spec is confusing
 * note:
 *	common between LE and ME
 */
int le_append_pt(ni_t *ni, le_t *le)
{
	pt_t *pt = &ni->pt[le->pt_index];

	assert(pthread_spin_trylock(&pt->lock) != 0);

	le->pt = pt;

	if (le->ptl_list == PTL_PRIORITY_LIST) {
		pt->priority_size++;
		if (unlikely(pt->priority_size > ni->limits.max_list_size)) {
			pt->priority_size--;
			pthread_spin_unlock(&pt->lock);
			return PTL_NO_SPACE;
		}
		list_add(&le->list, &pt->priority_list);
	} else if (le->ptl_list == PTL_OVERFLOW_LIST) {
		pt->overflow_size++;
		if (unlikely(pt->overflow_size > ni->limits.max_list_size)) {
			pt->overflow_size--;
			pthread_spin_unlock(&pt->lock);
			return PTL_NO_SPACE;
		}
		list_add(&le->list, &pt->overflow_list);
	}

#if 1
	if (le->eq && !(le->options & PTL_LE_EVENT_LINK_DISABLE)) {
		make_le_event(le, le->eq, PTL_EVENT_LINK, PTL_NI_OK);
	}
#endif

	return PTL_OK;
}

/* Do the LE append or LE search. It's an append if le_handle is not
 * NULL. */
static int le_append_or_search(ptl_handle_ni_t ni_handle,
			       ptl_pt_index_t pt_index,
			       const ptl_le_t *le_init, ptl_list_t ptl_list,
			       ptl_search_op_t search_op, void *user_ptr,
			       ptl_handle_le_t *le_handle)
{
	int err;
	ni_t *ni;
	le_t *le = le;
	pt_t *pt;

	err = get_gbl();
	if (unlikely(err))
		return err;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

#ifndef NO_ARG_VALIDATION
	err = le_append_check(TYPE_LE, ni, pt_index, le_init,
			      ptl_list, 0, le_handle);
	if (unlikely(err))
		goto err2;
#endif

	err = le_get_le(ni, &le);
	if (err)
		goto err2;

	err = le_get_mr(ni, le_init, le);
	if (unlikely(err))
		goto err3;

	pt = &ni->pt[pt_index];

	le->pt_index = pt_index;
	le->eq = pt->eq;
	le->uid = le_init->uid;
	le->user_ptr = user_ptr;
	le->start = le_init->start;
	le->options = le_init->options;
	le->ptl_list = ptl_list;
	INIT_LIST_HEAD(&le->list);

	if (le_handle) {
		/* Only append can modify counters. */
		err = to_ct(le_init->ct_handle, &le->ct);
		if (unlikely(err))
			goto err3;
	} else {
		le->ct = NULL;
	}

	if (unlikely(le->ct && (obj_to_ni(le->ct) != ni))) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	if (le_handle) {
		pthread_spin_lock(&pt->lock);

		if (ptl_list == PTL_PRIORITY_LIST) {
			/* To avoid races we must cycle through the list until
			 * nothing matches anymore. */
			while(check_overflow(le)) {
				/* Some XT were processed. */
				if (le->options & PTL_ME_USE_ONCE) {
					eq_t *eq = ni->pt[le->pt_index].eq;

					pthread_spin_unlock(&pt->lock);					

					if (eq && !(le->options &
					    PTL_ME_EVENT_UNLINK_DISABLE)) {
						make_le_event(le, eq,
							PTL_EVENT_AUTO_UNLINK,
							PTL_NI_OK);
					}
					*le_handle = le_to_handle(le);
					le_put(le);

					goto done;
				}
			}
		}

		err = le_append_pt(ni, le);

		pthread_spin_unlock(&pt->lock);

		if (unlikely(err))
			goto err3;

		*le_handle = le_to_handle(le);
	} else {
		if (search_op == PTL_SEARCH_ONLY)
			err = check_overflow_search_only(le);
		else
			err = check_overflow_search_delete(le);

		if (err)
			goto err3;

		le_put(le);
	}

 done:
	ni_put(ni);
	gbl_put();
	return PTL_OK;

 err3:
	le_put(le);
 err2:
	ni_put(ni);
 err1:
	gbl_put();
	return err;
}

int PtlLEAppend(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index,
		const ptl_le_t *le_init, ptl_list_t ptl_list, void *user_ptr,
		ptl_handle_le_t *le_handle)
{
	return le_append_or_search(ni_handle, pt_index,
				   le_init, ptl_list, 0, user_ptr,
				   le_handle);
}

int PtlLESearch(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index,
		const ptl_le_t *le_init, ptl_search_op_t search_op,
		void *user_ptr)
{
	return le_append_or_search(ni_handle, pt_index,
				   le_init, 0, search_op, user_ptr,
				   NULL);
}

int PtlLEUnlink(ptl_handle_le_t le_handle)
{
	int err;
	le_t *le;

	err = get_gbl();
	if (unlikely(err))
		return err;

	err = to_le(le_handle, &le);
	if (unlikely(err))
		goto err1;

	le_unlink(le, 0);

	le_put(le);
	gbl_put();
	return PTL_OK;

err1:
	gbl_put();
	return err;
}
