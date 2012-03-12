/**
 * @file ptl_le.c
 *
 * @brief Portals LE APIs.
 */

#include "ptl_loc.h"

/**
 * @brief Initialize a new LE once when created.
 *
 * @param[in] arg An opaque reference to LE.
 *
 * @return PTL_OK Always.
 */
int le_init(void *arg, void *unused)
{
	le_t *le = arg;

	le->type = TYPE_LE;

	return PTL_OK;
}

/**
 * @brief Cleanup an LE when the last reference is dropped.
 * @note This is common code for both LE's and ME's.
 *
 * @param[in] arg opaque reference to LE/ME.
 */
void le_cleanup(void *arg)
{
	le_t *le = arg;
	ni_t *ni = obj_to_ni(le);

	if (le->ct)
		ct_put(le->ct);

	if (le->pt) {
		WARN();
		le->pt = NULL;
	}

	if (le->do_auto_free)
		make_le_event(le, le->eq, PTL_EVENT_AUTO_FREE, PTL_NI_OK);

	(void)__sync_fetch_and_sub(&ni->current.max_entries, 1);
}

/**
 * @brief Unlink an entry from a PT list and remove
 * the reference held by the PT list.
 *
 * @param[in] le The LE object to unlink.
 * @param[in] auto_event A flag indicating if an auto unlink event
 * should be generated.
 */
void le_unlink(le_t *le, int auto_event)
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

			if (auto_event && le->eq) {
				make_le_event(le, le->eq,
					      PTL_EVENT_AUTO_UNLINK,
					      PTL_NI_OK);
				if (le->ptl_list == PTL_OVERFLOW_LIST)
					le->do_auto_free = 1;
			}

			le->pt = NULL;
		}

		pthread_spin_unlock(&pt->lock);

		if (le->type == TYPE_ME)
			me_put((me_t *)le);
		else
			le_put(le);
	}
}

/**
 * @brief Check call parameters for append or search API.
 * @note common code for LE and ME.
 */
int le_append_check(int type, ni_t *ni, ptl_pt_index_t pt_index,
		    const ptl_le_t *le_init, ptl_list_t ptl_list,
		    ptl_search_op_t search_op, ptl_handle_le_t *le_handle)
{
#ifndef NO_ARG_VALIDATION
	pt_t *pt = &ni->pt[pt_index];

	if (pt_index > ni->limits.max_pt_index)
		return PTL_ARG_INVALID;

	if (!pt->in_use)
		return PTL_ARG_INVALID;

	if (type == TYPE_ME) {
		if ((ni->options & PTL_NI_MATCHING) == 0)
			return PTL_ARG_INVALID;
	} else {
		if ((ni->options & PTL_NI_NO_MATCHING) == 0)
			return PTL_ARG_INVALID;
	}

	if (le_init->options & PTL_IOVEC) {
		if (le_init->length > ni->limits.max_iovecs)
			return PTL_ARG_INVALID;
	}

	if (type == TYPE_ME) {
		if (le_init->options & ~PTL_ME_APPEND_OPTIONS_MASK)
			return PTL_ARG_INVALID;
	} else {
		if (le_init->options & ~PTL_LE_APPEND_OPTIONS_MASK)
			return PTL_ARG_INVALID;
	}

	if (le_handle) {
		if (ptl_list < PTL_PRIORITY_LIST ||
		    ptl_list > PTL_OVERFLOW_LIST)
			return PTL_ARG_INVALID;
	} else {
		if (search_op < PTL_SEARCH_ONLY ||
		    search_op > PTL_SEARCH_DELETE)
			return PTL_ARG_INVALID;
	}

#endif /* NO_ARG_VALIDATION */
	return PTL_OK;
}

/**
 * @brief Computes the length of an LE/ME and register the memory it
 * refers to if desired.
 * @note Common between LE and ME.
 *
 * @param[in] ni
 * @param[in] le_init
 * @param[in] le
 *
 * @return status
 */
int le_get_mr(ni_t * restrict ni, const ptl_le_t *le_init, le_t *le)
{
	int i;
	ptl_iovec_t *iov;

	if (le_init->options & PTL_IOVEC) {
		le->num_iov = le_init->length;

		le->length = 0;
		iov = (ptl_iovec_t *)le_init->start;

		for (i = 0; i < le->num_iov; i++) {
			mr_t *mr;

			if (mr_lookup(ni, iov->iov_base, iov->iov_len, &mr) == PTL_OK)
				mr_put(mr);
			else
				return PTL_ARG_INVALID;

			le->length += iov->iov_len;
			iov++;
		}
	} else {
		/* If the memory is supposedly all accessible, register it. */
		if (!(ni->limits.features & PTL_TARGET_BIND_INACCESSIBLE) ||
			(le_init->options & PTL_LE_IS_ACCESSIBLE)) {
			mr_t *mr;

			if (mr_lookup(ni, le_init->start, le_init->length, &mr) == PTL_OK)
				mr_put(mr);
			else
				return PTL_ARG_INVALID;
		}

		le->length = le_init->length;
	}

	return PTL_OK;
}

/**
 * @brief Add LE/ME to pt entry.
 * @note Common between LE and ME
 * @todo Check limits on this the spec is confusing
 * @pre caller should hold the pt spinlock.
 *
 * @param[in] ni
 * @param[in] le
 *
 * @return status
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
			WARN();
			return PTL_NO_SPACE;
		}
		list_add(&le->list, &pt->priority_list);
	} else if (le->ptl_list == PTL_OVERFLOW_LIST) {
		pt->overflow_size++;
		if (unlikely(pt->overflow_size > ni->limits.max_list_size)) {
			pt->overflow_size--;
			pthread_spin_unlock(&pt->lock);
			WARN();
			return PTL_NO_SPACE;
		}
		list_add(&le->list, &pt->overflow_list);
	}

	if (le->eq && !(le->options & PTL_LE_EVENT_LINK_DISABLE))
		make_le_event(le, le->eq, PTL_EVENT_LINK, PTL_NI_OK);

	return PTL_OK;
}

enum {
	DONT_CHECK_PERM,
	CHECK_PERM,
};

/**
 * @brief Compares an ME/LE with the unexpected list
 * and returns a list of messages that match. 
 * @note Common code for LE/ME list elements.
 * @pre PT lock must be held by caller.
 *
 * @param[in] le The LE/ME match against the unexpected list.
 * @param[in] perm Flag to control permission checking.
 * @param[out] buf_list The returned message list.
 */
static void __match_le_unexpected(const le_t *le, int perm, 
				  struct list_head *buf_list)
{
	ni_t *ni = obj_to_ni(le);
	pt_t *pt = &ni->pt[le->pt_index];
	buf_t *buf;
	buf_t *n;

	assert(pthread_spin_trylock(&pt->lock) != 0);

	INIT_LIST_HEAD(buf_list);

	list_for_each_entry_safe(buf, n, &pt->unexpected_list,
				 unexpected_list) {

		if ((le->type == TYPE_LE || check_match(buf, (me_t *)le)) &&
		    (perm == DONT_CHECK_PERM || !check_perm(buf, le))) {
			list_del(&buf->unexpected_list);
			list_add_tail(&buf->unexpected_list, buf_list);

			if (le->options & PTL_LE_USE_ONCE)
				break;
		}
	}
}

static void flush_from_expected_list(le_t *le, const struct list_head *buf_list, int delete)
{
	buf_t *buf;
	buf_t *n;

	list_for_each_entry_safe(buf, n, buf_list,
							 unexpected_list) {
		int err;
		int state;

		pthread_mutex_lock(&buf->mutex);

		assert(buf->matching.le == NULL);
		buf->matching.le = le;
		le_get(le);

		if (delete && buf->le) {
			le_put(buf->le);
			buf->le = NULL;
		}

		list_del(&buf->unexpected_list);

		state = buf->tgt_state;

		pthread_mutex_unlock(&buf->mutex);

		if (state == STATE_TGT_WAIT_APPEND) {
			err = process_tgt(buf);
			if (err)
				WARN();
		}

		/* From get_match(). */
		buf_put(buf);
	}
}

/**
 * @brief Check whether the LE/ME matches one or more messages on the
 * unexpected list.
 *
 * Return true if at least one message was processed.
 *
 * @note Common code for LE/ME list elements.
 * @pre PT lock must be taken.
 *
 * @param[in] le the LE/ME object to check against the overflow list.
 *
 * @return status
 */
int __check_overflow(le_t *le, int delete)
{
	ni_t *ni = obj_to_ni(le);
	pt_t *pt = &ni->pt[le->pt_index];
	struct list_head buf_list;
	int ret;

	assert(pthread_spin_trylock(&pt->lock) != 0);

	__match_le_unexpected(le, DONT_CHECK_PERM, &buf_list);

	ret = !list_empty(&buf_list);
	if (ret) {
		/* Process the elements of the list. */
		pthread_spin_unlock(&pt->lock);

		flush_from_expected_list(le, &buf_list, 0);

		pthread_spin_lock(&pt->lock);
	}

	return ret;
}

/**
 * @brief Check whether the LE/ME matches one or more messages on
 * the unexpected list.
 * @note Common code for LE/ME objects.
 *
 * @param[in] le the list element to check.
 *
 * @return status
 */
int check_overflow_search_only(le_t *le)
{
	ni_t *ni = obj_to_ni(le);
	pt_t *pt = &ni->pt[le->pt_index];
	buf_t *buf;
	buf_t *n;
	int found = 0;
	ptl_event_t event;

	pthread_spin_lock(&pt->lock);

	list_for_each_entry_safe(buf, n, &pt->unexpected_list,
				 unexpected_list) {

		if ((le->type == TYPE_LE || check_match(buf, (me_t *)le))) {
			found = 1;

			if (le->eq && !(le->options &
			    PTL_LE_EVENT_COMM_DISABLE)) {
				fill_target_event(buf, PTL_EVENT_SEARCH,
						  le->user_ptr, NULL,
						  &event);
			}

			break;
		}
	}

	pthread_spin_unlock(&pt->lock);

	/* note there is a race where the buf can get removed before
	 * the event is delivered to the target so we save the contents
	 * of the event in a local struct inside the lock and cause
	 * the event to be delivered outside the lock */

	if (le->eq && !(le->options & PTL_LE_EVENT_COMM_DISABLE)) {
		if (found) {
			/* note search events always set ni ok */
			event.ni_fail_type = PTL_NI_OK;
			send_target_event(le->eq, &event);
		} else {
			make_le_event(le, le->eq, PTL_EVENT_SEARCH,
				      PTL_NI_NO_MATCH);
		}
	}

	return PTL_OK;
}

/**
 * @brief Search for matching entries in the unexpected and delete them.
 * @note Common code for LE/ME list elements.
 *
 * @param[in] le The list element to search the overflow list with.
 *
 * @return status
 */
int check_overflow_search_delete(le_t *le)
{
	ni_t *ni = obj_to_ni(le);
	pt_t *pt = &ni->pt[le->pt_index];
	struct list_head buf_list;

	/* scan the unexpected list removing each
	 * matching message and adding to the buf_list */
	pthread_spin_lock(&pt->lock);

	__match_le_unexpected(le, CHECK_PERM, &buf_list);

	pthread_spin_unlock(&pt->lock);

	if (list_empty(&buf_list)) {
		make_le_event(le, le->eq, PTL_EVENT_SEARCH, PTL_NI_NO_MATCH);
	} else {
		flush_from_expected_list(le, &buf_list, 1);
	}

	return PTL_OK;
}

/**
 * @brief Common code for implemtation of PtlLEAppend and PtlLESearch.
 *
 * Performs an append if le_handle_p != NULL, else a search.
 *
 * @param[in] ni_handle
 * @param[in] pt_index
 * @param[in] le_init
 * @param[in] ptl_list
 * @param[in] search_op
 * @param[in] user_ptr
 * @param[out] le_handle_p
 *
 * @return status
 */
static int le_append_or_search(ptl_handle_ni_t ni_handle,
			       ptl_pt_index_t pt_index,
			       const ptl_le_t *le_init, ptl_list_t ptl_list,
			       ptl_search_op_t search_op, void *user_ptr,
			       ptl_handle_le_t *le_handle_p)
{
	int err;
	ni_t *ni;
	le_t *le = le;
	pt_t *pt;

	/* sanity checks and convert ni handle to object */
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

	err = le_append_check(TYPE_LE, ni, pt_index, le_init,
			      ptl_list, search_op, le_handle_p);
	if (err)
		goto err2;
#else
	ni = fast_to_obj(ni_handle);
#endif

	// TODO convert these to atomic_inc/dec macros
	if (unlikely(__sync_add_and_fetch(&ni->current.max_entries, 1) >
				 ni->limits.max_entries)) {
		(void)__sync_fetch_and_sub(&ni->current.max_entries, 1);
		err = PTL_NO_SPACE;
		goto err2;
	}

	err = le_alloc(ni, &le);
	if (unlikely(err)) {
                (void)__sync_fetch_and_sub(&ni->current.max_entries, 1);
		goto err2;
	}

	err = le_get_mr(ni, le_init, le);
	if (unlikely(err))
		goto err3;

	pt = &ni->pt[pt_index];

	INIT_LIST_HEAD(&le->list);
	le->eq = pt->eq;
	le->pt_index = pt_index;
	le->uid = le_init->uid;
	le->user_ptr = user_ptr;
	le->start = le_init->start;
	le->options = le_init->options;
	le->do_auto_free = 0;
	le->ptl_list = ptl_list;

#ifndef NO_ARG_VALIDATION
	if (le_handle_p) {
		/* Only append can modify counters. */
		if (le_init->ct_handle != PTL_CT_NONE) {
			err = to_ct(le_init->ct_handle, &le->ct);
			if (err)
				goto err3;
		} else {
			le->ct = NULL;
		}
	} else {
		le->ct = NULL;
	}

	if (le->ct && (obj_to_ni(le->ct) != ni)) {
		err = PTL_ARG_INVALID;
		goto err3;
	}
#else
	le->ct = (le_handle_p && le_init->ct_handle != PTL_CT_NONE) ?
			fast_to_obj(le_init->ct_handle) : NULL;
#endif

	if (le_handle_p) {
		pthread_spin_lock(&pt->lock);

		if (ptl_list == PTL_PRIORITY_LIST) {
			/* To avoid races we must cycle through the list until
			 * nothing matches anymore. */
			while(__check_overflow(le, 0)) {
				/* Some XT were processed. */
				if (le->options & PTL_ME_USE_ONCE) {
					eq_t *eq = ni->pt[le->pt_index].eq;

					pthread_spin_unlock(&pt->lock);					

					if (eq && !(le->options &
					    PTL_ME_EVENT_UNLINK_DISABLE)) {
						make_le_event(le, eq,
							PTL_EVENT_AUTO_UNLINK,
							PTL_NI_OK);
						le->do_auto_free = 1;
					}

					*le_handle_p = le_to_handle(le);
					le_put(le);

					goto done;
				}
			}
		}

		err = le_append_pt(ni, le);

		pthread_spin_unlock(&pt->lock);

		if (unlikely(err))
			goto err3;

		*le_handle_p = le_to_handle(le);
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
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Append a list entry to a portals table list.
 *
 * The PtlLEAppend() function creates a single list entry and appends
 * this entry to the end of the list specified by * ptl_list associated
 * with the portal table entry specified by pt_index for the portal
 * table for ni_handle. If the list is currently uninitialized, the
 * PtlLEAppend() function creates the first entry in the list.
 *
 * @param[in] ni_handle The interface handle to use.
 * @param[in] pt_index The portal table index where the list entry
 * should be appended.
 * @param[in] le_init Provides initial values for the user-visible
 * parts of a list entry. Other than its use for initialization, there
 * is no linkage between this structure and the list entry maintained
 * by the API.
 * @param[in] ptl_list Determines whether the list entry is appended
 * to the priority list, appended to the overflow list, or simply
 * queries the overflow list.
 * @param[in] user_ptr A user-specified value that is associated with
 * each command that can generate an event. The value does not need
 * to be a pointer, but must fit in the space used by a pointer. This
 * value (along with other values) is recorded in full events
 * associated with operations on this list entry.
 * @param[out] le_handle_p On successful return, this location will
 * hold the newly created list entry handle.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * The definition of which arguments are checked is implementation dependent.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_NO_SPACE Indicates that there is insufficient memory to
 * allocate the match list entry.
 * @return PTL_LIST_TOO_LONG Indicates that the resulting list is too long.
 * The maximum length for a list is defined by the interface.
 */
int PtlLEAppend(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index,
		const ptl_le_t *le_init, ptl_list_t ptl_list, void *user_ptr,
		ptl_handle_le_t *le_handle_p)
{
	int err;

	err = le_append_or_search(ni_handle, pt_index, le_init,
				  ptl_list, 0, user_ptr, le_handle_p);
	return err;
}

/**
 * @brief Search for a message in an unexpected list.
 *
 * The PtlLESearch() function is used to search for a message in the
 * unexpected list associated with a specific portal table entry
 * specified by pt_index for the portal table for ni_handle. PtlLESearch()
 * uses the exact same search of the unexpected list as PtlLEAppend();
 * however, the list entry specified in the PtlLESearch() call is never
 * linked into a priority list.
 *
 * @param[in] ni_handle The interface handle to use.
 * @param[in] pt_index The portal table index that should be searched.
 * @param[in] le_init Provides values for the user-visible parts of a list
 * entry to use for searching.
 * @param[in] search_op Determines whether the function only
 * searches the list or searches the list and deletes the matching
 * entries from the list.
 * @param[in] user_ptr A user-specified value that is associated with
 * each command that can generate an event. The value does not need to
 * be a pointer, but must fit in the space used by a pointer. This
 * value (along with other values) is recorded in full events associated
 * with operations on this list entry.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 */
int PtlLESearch(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index,
		const ptl_le_t *le_init, ptl_search_op_t search_op,
		void *user_ptr)
{
	int err;

	err = le_append_or_search(ni_handle, pt_index, le_init,
				  0, search_op, user_ptr, NULL);
	return err;
}

/**
 * @brief Unlink a list element from a portals table list.
 *
 * The PtlLEUnlink() function can be used to unlink a list entry from a
 * list. This operation also releases any resources associated with
 * the list entry. It is an error to use the list entry handle after
 * calling PtlLEUnlink().
 *
 * @param[in] le_handle The list entry handle to be unlinked.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * @return PTL_IN_USE Indicates that the list entry has pending
 * operations and cannot be unlinked.
 */
int PtlLEUnlink(ptl_handle_le_t le_handle)
{
	int err;
	le_t *le;
	int ref_cnt;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_le(le_handle, &le);
	if (err)
		goto err1;
#else
	le = fast_to_obj(le_handle);
#endif

	ref_cnt = le_ref_cnt(le);

	/* There should only be 2 references on the object before we can
	 * release it. */
	if (ref_cnt > 2) {
		le_put(le);
		err = PTL_IN_USE;
		goto err1;
	} else if (ref_cnt < 2) {
		le_put(le);
		err = PTL_ARG_INVALID;
		goto err1;
	}

	le_unlink(le, 0);

	err = PTL_OK;

	le_put(le);
err1:
#ifndef NO_ARG_VALIDATION
	gbl_put();
err0:
#endif
	return err;
}
