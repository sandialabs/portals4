/*
 * ptl_le.c
 */

#include "ptl_loc.h"

/*
 * le_init
 *	initialize new le
 */
int le_init(void *arg)
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
void le_release(void *arg)
{
	int i;
        le_t *le = arg;
	ni_t *ni = to_ni(le);
	pt_t *pt = le->pt;

	if (le->ct)
		ct_put(le->ct);

	if (pt)
		WARN();

	if (le->sge_list) {
		free(le->sge_list);
		le->sge_list = NULL;
	}

	if (le->mr_list) {
		for (i = 0; i < le->num_iov; i++) {
			if (le->mr_list[i])
				mr_put(le->mr_list[i]);
		}
		free(le->mr_list);
		le->mr_list = NULL;
	}

	if (le->mr) {
		mr_put(le->mr);
		le->mr = NULL;
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
void le_unlink(le_t *le)
{
	pt_t *pt = le->pt;

	if (pt) {
		pthread_spin_lock(&pt->list_lock);
		if (le->ptl_list == PTL_PRIORITY_LIST)
			pt->priority_size--;
		else if (le->ptl_list == PTL_OVERFLOW)
			pt->overflow_size--;
		list_del_init(&le->list);
		pthread_spin_unlock(&pt->list_lock);
		le->pt = NULL;
		le_put(le);
	} else
		WARN();
}

/*
 * le_get_le
 *	allocate an le after checking to see if there
 *	is room in the limit
 */
int le_get_le(ni_t *ni, le_t **le_p)
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
		    ptl_le_t *le_init, ptl_list_t ptl_list,
		    ptl_handle_le_t *le_handle)
{
	if (unlikely(!ni))
		return PTL_ARG_INVALID;

	if (type == TYPE_ME) {
		if (unlikely((ni->options & PTL_NI_MATCHING) == 0))
			return PTL_ARG_INVALID;
	} else {
		if (unlikely((ni->options & PTL_NI_NO_MATCHING) == 0))
			return PTL_ARG_INVALID;
	}

	if (unlikely(pt_index >= ni->limits.max_pt_index))
		return PTL_ARG_INVALID;

	if (le_init->options & PTL_IOVEC) {
		if (le_init->length > ni->limits.max_iovecs)
			return PTL_ARG_INVALID;
	}

	if (type == TYPE_ME) {
		if (unlikely(le_init->options & ~_PTL_ME_APPEND_OPTIONS))
			return PTL_ARG_INVALID;
	} else {
		if (unlikely(le_init->options & ~_PTL_LE_APPEND_OPTIONS))
			return PTL_ARG_INVALID;
	}

	if (unlikely(ptl_list < PTL_PRIORITY_LIST || ptl_list > PTL_PROBE_ONLY))
		return PTL_ARG_INVALID;

	return PTL_OK;
}

/*
 * le_get_mr
 *	allocate InfiniBand MRs for le
 * note:
 *	common between LE and ME
 */
int le_get_mr(ni_t *ni, ptl_le_t *le_init, le_t *le)
{
	int err;
	int i;
	mr_t *mr;
	ptl_iovec_t *iov;
	struct ibv_sge *sge;

	if (le_init->options & PTL_IOVEC) {
		le->num_iov = le_init->length;
		le->mr_list = calloc(le->num_iov, sizeof(mr_t *));
		if (!le->mr_list)
			return PTL_NO_SPACE;

		if (le->num_iov > MAX_INLINE_SGE) {
			le->sge_list = calloc(le->num_iov, sizeof(*sge));
			if (!le->sge_list)
				return PTL_NO_SPACE;

			err = mr_lookup(ni, le->sge_list,
					le->num_iov * sizeof(*sge), &le->mr);
			if (err)
				return err;
		}

		le->length = 0;
		iov = (ptl_iovec_t *)le_init->start;
		sge = le->sge_list;

		for (i = 0; i < le->num_iov; i++) {
			err = mr_lookup(ni, iov->iov_base, iov->iov_len, &mr);
			if (err)
				return err;

			if (le->sge_list) {
				sge->addr =
					cpu_to_be64((uintptr_t)iov->iov_base);
				sge->length = cpu_to_be32(iov->iov_len);
				sge->lkey = cpu_to_be32(mr->ibmr->rkey);
			}

			le->length += iov->iov_len;
			le->mr_list[i] = mr;
			iov++;
			sge++;
		}
	} else {
		le->length = le_init->length;
		err = mr_lookup(ni, le_init->start, le_init->length, &le->mr);
		if (err)
			return err;
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

	if (!pt->in_use)
		return PTL_ARG_INVALID;

	pthread_spin_lock(&pt->list_lock);

	if (le->ptl_list == PTL_PRIORITY_LIST) {
		pt->priority_size++;
		if (unlikely(pt->priority_size > ni->limits.max_list_size)) {
			pt->priority_size--;
			pthread_spin_unlock(&pt->list_lock);
			return PTL_NO_SPACE;
		}
		list_add(&le->list, &pt->priority_list);
	} else if (le->ptl_list == PTL_OVERFLOW) {
		pt->overflow_size++;
		if (unlikely(pt->overflow_size > ni->limits.max_list_size)) {
			pt->overflow_size--;
			pthread_spin_unlock(&pt->list_lock);
			return PTL_NO_SPACE;
		}
		list_add(&le->list, &pt->overflow_list);
	}

	le->pt = pt;
	pthread_spin_unlock(&pt->list_lock);

	return PTL_OK;
}

int PtlLEAppend(ptl_handle_ni_t ni_handle, ptl_pt_index_t pt_index,
                ptl_le_t *le_init, ptl_list_t ptl_list, void *user_ptr,
                ptl_handle_le_t *le_handle)
{
	int err;
	gbl_t *gbl;
	ni_t *ni;
	le_t *le;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = ni_get(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	err = le_append_check(TYPE_LE, ni, pt_index, le_init,
			      ptl_list, le_handle);
	if (unlikely(err))
		goto err2;

	err = le_get_le(ni, &le);
	if (err)
		goto err2;

	err = le_get_mr(ni, le_init, le);
	if (unlikely(err))
		goto err3;

	le->pt_index = pt_index;
	le->uid = le_init->ac_id.uid;
	le->user_ptr = user_ptr;
	le->start = le_init->start;
	le->options = le_init->options;
	le->ptl_list = ptl_list;
	INIT_LIST_HEAD(&le->list);

	err = ct_get(le_init->ct_handle, &le->ct);
	if (unlikely(err))
		goto err3;

	if (unlikely(le->ct && (to_ni(le->ct) != ni))) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = le_append_pt(ni, le);
	if (unlikely(err))
		goto err3;

	*le_handle = le_to_handle(le);

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err3:
	le_put(le);
err2:
	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}

int PtlLEUnlink(ptl_handle_le_t le_handle)
{
	int err;
	le_t *le;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (unlikely(err))
		return err;

	err = le_get(le_handle, &le);
	if (unlikely(err))
		goto err1;

	le_unlink(le);

	le_put(le);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}
