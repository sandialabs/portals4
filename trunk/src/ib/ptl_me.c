/**
 * @file ptl_me.c
 *
 * @brief Portals matching list element API's.
 */

#include "ptl_loc.h"
#include "ptl_ct.h"

#ifdef WITH_TRIG_ME_OPS
static void post_trig_me(buf_t *buf, ct_t *me_ct);
void do_trig_me_op(buf_t *buf);
#endif

/**
 * @brief Initialize a new ME once when created.
 *
 * @param[in] arg An opaque reference to ME.
 *
 * @return PTL_OK Always.
 */
int me_init(void *arg, void *unused)
{
	me_t *me = arg;

	me->type = TYPE_ME;
	return 0;
}

/**
 * @brief Cleanup an ME when the last reference is dropped.
 *
 * @param[in] arg opaque reference to ME.
 */
void me_cleanup(void *arg)
{
	me_t *me = arg;

	le_cleanup((le_t *)me);
}

/**
 * @brief Common code for implementation of PtlMEAppend and PtlMESearch
 *
 * @param[in] ni_handle
 * @param[in] pt_index
 * @param[in] me_init
 * @param[in] ptl_list
 * @param[in] search_op
 * @param[in] user_ptr
 * @param[out] me_handle_p
 *
 * @return status
 */
static int me_append_or_search(PPEGBL ptl_handle_ni_t ni_handle,
			       ptl_pt_index_t pt_index,
			       const ptl_me_t *me_init, ptl_list_t ptl_list,
			       ptl_search_op_t search_op, void *user_ptr,
			       ptl_handle_me_t *me_handle_p)
{
	int err;
	ni_t *ni;
	me_t *me = me;
	pt_t *pt;

	/* sanity checks and convert ni handle to object */
#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ni(MYGBL_ ni_handle, &ni);
	if (err)
		goto err1;

	err = le_append_check(TYPE_ME, ni, pt_index, (ptl_le_t *)me_init,
			       ptl_list, search_op,
			       (ptl_handle_le_t *)me_handle_p);
	if (err)
		goto err2;
#else
	ni = to_obj(MYGBL_ POOL_ANY, ni_handle);
#endif

	// TODO convert these to atomic_inc/dec macros
	if (unlikely(__sync_add_and_fetch(&ni->current.max_entries, 1) >
				 ni->limits.max_entries)) {
		(void)__sync_fetch_and_sub(&ni->current.max_entries, 1);
		err = PTL_NO_SPACE;
		goto err2;
	}

	err = me_alloc(ni, &me);
	if (unlikely(err)) {
                (void)__sync_fetch_and_sub(&ni->current.max_entries, 1);
		goto err2;
	}

	//Only get the mr if we need it
	if (me_init->start != NULL && me_init->length > 0){
		err = le_get_mr(ni, (ptl_le_t *)me_init, (le_t *)me);
		if (unlikely(err))
			goto err3;
	}
	else{
		me->length = me_init->length;
		me->num_iov = 0;
	}

	pt = &ni->pt[pt_index];

	INIT_LIST_HEAD(&me->list);
	me->pt_index = pt_index;
	me->eq = pt->eq;
	me->uid = me_init->uid;
	me->user_ptr = user_ptr;
	me->start = me_init->start;
	me->options = me_init->options;
	me->do_auto_free = 0;
	me->ptl_list = ptl_list;
	me->offset = 0;
	me->min_free = me_init->min_free;
	me->id = me_init->match_id;
	me->match_bits = me_init->match_bits;
	me->ignore_bits = me_init->ignore_bits;
	
	atomic_set(&me->busy, 0);

#ifndef NO_ARG_VALIDATION
	if (me_handle_p) {
		/* Only append can modify counters. */
		err = to_ct(MYGBL_ me_init->ct_handle, &me->ct);
		if (err)
			goto err3;
	} else {
		me->ct = NULL;
	}

	if (me->ct && (obj_to_ni(me->ct) != ni)) {
		err = PTL_ARG_INVALID;
		goto err3;
	}
#else
	me->ct = (me_handle_p && me_init->ct_handle != PTL_CT_NONE) ?
			to_obj(MYGBL_ POOL_ANY, me_init->ct_handle) : NULL;
#endif

	if (me_handle_p) {
		PTL_FASTLOCK_LOCK(&pt->lock);

		if (ptl_list == PTL_PRIORITY_LIST) {

			/* To avoid races we must cycle through the list until
			 * nothing matches anymore. */
			while(__check_overflow((le_t *)me, 0)) {
				/* Some XT were processed. */
				if (me->options & PTL_ME_USE_ONCE) {
					eq_t *eq = ni->pt[me->pt_index].eq;

					PTL_FASTLOCK_UNLOCK(&pt->lock);
					if (eq && !(me->options &
					    PTL_ME_EVENT_UNLINK_DISABLE)) {
						make_le_event((le_t *)me, eq,
							PTL_EVENT_AUTO_UNLINK,
							PTL_NI_OK);

						if (me->ptl_list ==
						    PTL_OVERFLOW_LIST)
							me->do_auto_free = 1;
					}

					*me_handle_p = me_to_handle(me);
					me_put(me);

					goto done;
				}
			}
		}

		err = le_append_pt(ni, (le_t *)me);

		PTL_FASTLOCK_UNLOCK(&pt->lock);

		if (unlikely(err)) {
			WARN();
			goto err3;
		}

		*me_handle_p = me_to_handle(me);

	} else {
		if (search_op == PTL_SEARCH_ONLY)
			err = check_overflow_search_only((le_t *)me);
		else
			err = check_overflow_search_delete((le_t *)me);

		if (err)
			goto err3;

		me_put(me);
	}

done:
	ni_put(ni);
	gbl_put();
	return PTL_OK;

err3:
	me_put(me);
err2:
	ni_put(ni);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	WARN();
	return err;
}

/**
 * @brief  Append a matching list element to a portals table list.
 *
 * The PtlMEAppend() function creates a single match list entry. If
 * PTL_PRIORITY_LIST or PTL_OVERFLOW_LIST is specified by ptl_list,
 * this entry is appended to the end of the appropriate list specified
 * by ptl_list associated with the portal table entry specified by
 * pt_index for the portal table for ni_handle. If the list is
 * currently uninitialized, the PtlMEAppend() function creates the
 * first entry in the list.
 *
 * @param[in] ni_handle The interface handle to use.
 * @param[in] pt_index The portal table index where the match
 * list entry should be appended.
 * @param[in] me Provides initial values for the user-visible
 * parts of a match list entry. Other than its use for initialization,
 * there is no linkage between this structure and the match list entry
 * maintained by the API.
 * @param[in] ptl_list Determines whether the match list entry is
 * appended to the priority list, appended to the overflow list, or
 * simply queries the overflow list.
 * @param[in] user_ptr A user-specified value that is associated
 * with each command that can generate an event. The value does not need
 * to be a pointer, but must fit in the space used by a pointer. This
 * value (along with other values) is recorded in full events associated
 * with operations on this match list entry.
 * @param[out] me_handle On successful return, this location will
 * hold the newly created match list entry handle.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * The definition of which arguments are checked is implementation dependent.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_NO_SPACE Indicates that there is insufficient memory to
 * allocate the match list entry.
 * @return PTL_LIST_TOO_LONG Indicates that the resulting list is too
 * long. The maximum length for a list is defined by the interface.
 */
int _PtlMEAppend(PPEGBL ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index,
                const ptl_me_t *me_init, ptl_list_t ptl_list, void *user_ptr,
                ptl_handle_me_t *me_handle_p)
{
	int err;

	err = me_append_or_search(MYGBL_ ni_handle, pt_index,
				   me_init, ptl_list, 0, user_ptr,
				   me_handle_p);
	return err;
}

#ifdef WITH_TRIG_ME_OPS
/**
 * @brief Append a matching list element to a portals table list using 
 * Triggered behaviour.
 *
 *
 *
 */
int _PtlTriggeredMEAppend(PPEGBL ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index,
                ptl_me_t *me_init, ptl_list_t ptl_list, void *user_ptr,
                ptl_handle_me_t *me_handle_p, ptl_handle_ct_t trig_ct_handle, 
		ptl_size_t threshold)
{
	int err;
	
	buf_t *buf;
	ct_t *ct = NULL;
	ni_t *ni;

	err = gbl_get();
	if (unlikely(err))
                goto err0;

	err = to_ni(MYGBL_ ni_handle, &ni);
	if (unlikely(err))
		goto err0;	

	err = to_ct(MYGBL_ trig_ct_handle, &ct);
        if (unlikely(err))
               goto err0;

#ifndef NO_ARG_VALIDATION
        if (!ct) {
                err = PTL_ARG_INVALID;
                goto err1;
        }
#endif

	buf = malloc(sizeof(buf_t));

	buf->user_ptr = user_ptr;
	buf->ct_threshold = threshold;
	buf->threshold = threshold;
	buf->ct = ct;
	buf->type = BUF_TRIGGERED_ME;
	buf->me_op = TRIG_ME_APPEND;	
	buf->pt_index = pt_index;
	buf->me_init = me_init;
	buf->ptl_list = (ptl_list_t)ptl_list;
	buf->me_handle_p = me_handle_p;
	buf->ni_handle = ni_handle;	

	post_trig_me(buf, ct);	

	ct_put(ct);
	gbl_put();
	return PTL_OK;

	err1:
		gbl_put();
	err0:
		return err;
	
}
#endif

/**
 * @brief Search portals table overflow list for messages that match
 * a matching list entry.
 *
 * The PtlMESearch() function is used to search for a message in the
 * unexpected list associated with a specific portal table entry specified
 * by pt_index for the portal table for ni_handle. PtlMESearch() uses
 * the exact same search of the * unexpected list as PtlMEAppend();
 * however, the match list entry specified in the PtlMESearch() call is
 * never linked into a priority list.
 *
 * @param[in] ni_handle The interface handle to use.
 * @param[in] pt_index The portal table index that should be searched.
 * @param[in] me_init Provides values for the user-visible parts of a match
 * list entry to use for searching.
 * @param[in] search_op input Determines whether the function only searches
 * the list or searches the list and deletes the matching entries from
 * the list.
 * @param[in] user_ptr A user-specified value that is associated with
 * each command that can generate an event. The value does not need to be
 * a pointer, but must fit in the space used by a pointer. This * value
 * (along with other values) is recorded in full events associated with
 * operations on this match list entry4.
 *
 * @output PTL_OK Indicates success.
 * @output PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * The definition of which arguments are checked is implementation dependent.
 * @output PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 */
int _PtlMESearch(PPEGBL ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index,
		const ptl_me_t *me_init, ptl_search_op_t search_op,
		void *user_ptr)
{
	int err;

	err =  me_append_or_search(MYGBL_ ni_handle, pt_index, me_init, 0,
				   search_op, user_ptr, NULL);
	return err;
}

/**
 * @brief Unlink a matching list entry from a portals table list.
 *
 * The PtlMEUnlink() function can be used to unlink a match list entry
 * from a list. This operation also releases any resources associated
 * with the match list entry. It is an error to use the match list
 * entry handle after calling PtlMEUnlink().
 *
 * @param[in] me_handle The match list entry handle to be unlinked.
 *
 * @output PTL_OK Indicates success.
 * @output PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @output PTL_ARG_INVALID Indicates that an invalid argument was
 * passed. The definition of which arguments are checked is
 * implementation dependent.
 * @output PTL_IN_USE Indicates that the match list entry has pending
 * operations and cannot be unlinked.
 */
int _PtlMEUnlink(PPEGBL ptl_handle_me_t me_handle)
{
	int err;
	me_t *me;
	int ref_cnt;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_me(MYGBL_ me_handle, &me);
	if (err)
		goto err1;
#else
	me = to_obj(MYGBL_ POOL_ANY, me_handle);
#endif
	//If this was an overflow, it should just complete now
	//there's no other busy work being done
	if (me->ptl_list == PTL_OVERFLOW_LIST){
		atomic_set(&me->busy, 0);
	}

	/* make sure the me isn't still involved in any final
 	 * cleanup before we unlink it */		
	while (atomic_read(&me->busy) == 1){
		SPINLOCK_BODY();
	}

	ref_cnt = me_ref_cnt(me);

	/* There should only be 2 references on the object before we can
	 * release it. */
	if (ref_cnt > 2) {
		me_put(me);
		err = PTL_IN_USE;
		goto err1;
	} else if (ref_cnt < 2) {
		me_put(me);
		err = PTL_ARG_INVALID;
		goto err1;
	}

	le_unlink((le_t *)me, 0);

	err = PTL_OK;

	me_put(me);
err1:
#ifndef NO_ARG_VALIDATION
	gbl_put();
err0:
#endif
	return err;
}

/* TRIGGERED ME Code Begins */
#ifdef WITH_TRIG_ME_OPS

int _PtlTriggeredMEUnlink(PPEGBL ptl_handle_me_t me_handle,
		ptl_handle_ct_t trig_ct_handle, ptl_size_t threshold)
{

	int err;

        buf_t *buf;
        ct_t *ct = NULL;
        ni_t *ni;

        err = gbl_get();
        if (unlikely(err))
                goto err0;

        err = to_ct(MYGBL_ trig_ct_handle, &ct);
        if (unlikely(err))
               goto err1;

#ifndef NO_ARG_VALIDATION
        if (!ct) {
                err = PTL_ARG_INVALID;
                goto err1;
        }
#endif

        buf = malloc(sizeof(buf_t));

        buf->me_handle = me_handle;
	buf->ct = ct;
	buf->ct_threshold = threshold;
	buf->me_op = TRIG_ME_UNLINK;
	buf->type = BUF_TRIGGERED_ME;

	post_trig_me(buf, ct);


        ct_put(ct);
        gbl_put();
        return PTL_OK;

        err1:
                gbl_put();
        err0:
                return err;


}

/*@brief Perform triggered ME list operation.
 *
 * @param[in] buf
 */
void do_trig_me_op(buf_t *buf)
{
        ct_t *ct = buf->ct;

        /* we're a zombie */
        if (ct->info.interrupt)
                goto done;


	ptl_info("type of triggered me op is: %i \n",buf->me_op);
        switch(buf->me_op) {
        case TRIG_ME_APPEND:
                _PtlMEAppend(MYGBL_ buf->ni_handle, buf->pt_index,
                                   buf->me_init, buf->ptl_list, buf->user_ptr,
                                   buf->me_handle_p);
		break;
        case TRIG_ME_UNLINK:
                _PtlMEUnlink(buf->me_handle);
                break;
        default:
                assert(0);
        }

done:
        free(buf);
}

/**
 * @brief Add a triggered ME operation to a list or
 * do it if threshold has already been reached.
 *
 * @param[in] buf
 * @param[in] trig_ct
 */
static void post_trig_me(buf_t *buf, ct_t *me_ct)
{
        /* Put the buffer on the wait list. */
        PTL_FASTLOCK_LOCK(&me_ct->lock);

        /* 1st check to see whether the condition is already met. */
        if ((me_ct->info.event.failure + me_ct->info.event.success) >= buf->threshold) {
                PTL_FASTLOCK_UNLOCK(&me_ct->lock);

                do_trig_me_op(buf);

        } else {
                atomic_inc(&me_ct->list_size);

                list_add(&buf->list, &me_ct->trig_list);

                /* We must check again to avoid a race with make_ct_event/ct_inc_ct_set. */
                if ((me_ct->info.event.success + me_ct->info.event.failure) >= buf->threshold) {
                        /* Something arrived while we were adding the buffer. */
                        PTL_FASTLOCK_UNLOCK(&me_ct->lock);
                        ct_check(me_ct);
                } else {
                        PTL_FASTLOCK_UNLOCK(&me_ct->lock);
                }
        }
}

#endif
