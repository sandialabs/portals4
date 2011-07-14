/*
 * ptl_md.c
 */

#include "ptl_loc.h"

void md_release(void *arg)
{
	int i;
        md_t *md = arg;
	ni_t *ni = to_ni(md);

	if (md->eq) {
		eq_put(md->eq);
		md->eq = NULL;
	}

	if (md->ct) {
		ct_put(md->ct);
		md->ct = NULL;
	}

	if (md->sge_list_mr) {
		mr_put(md->sge_list_mr);
		md->sge_list_mr = NULL;
	}

	if (md->sge_list) {
		free(md->sge_list);
		md->sge_list = NULL;
	}

	pthread_spin_lock(&ni->obj.obj_lock);
	ni->current.max_mds--;
	pthread_spin_unlock(&ni->obj.obj_lock);
}

static int init_iovec(ni_t *ni, md_t *md, ptl_iovec_t *iov_list, int num_iov)
{
	int err;
	int i;
	ptl_iovec_t *iov;
	struct ibv_sge *sge;

	md->num_iov = num_iov;

	if (num_iov > get_param(PTL_MAX_INLINE_SGE)) {
		md->sge_list = calloc(num_iov, sizeof(struct ibv_sge));
		if (!md->sge_list) {
			WARN();
			return PTL_NO_SPACE;
		}

		err = mr_lookup(ni, md->sge_list,
				num_iov * sizeof(*sge),
				&md->sge_list_mr);
		if (err) {
			WARN();
			return err;
		}
	}

	md->length = 0;

	iov = iov_list;
	sge = md->sge_list;

	for (i = 0; i < num_iov; i++) {
		md->length += iov->iov_len;

		if (md->sge_list) {
			sge->addr = cpu_to_be64((uintptr_t)iov->iov_base);
			sge->length = cpu_to_be32(iov->iov_len);
		}

		iov++;
		sge++;
	}

	return PTL_OK;
}

int PtlMDBind(ptl_handle_ni_t ni_handle, ptl_md_t *md_init,
              ptl_handle_md_t *md_handle)
{
	int err;
	ni_t *ni;
	md_t *md;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (err) {
		WARN();
		return err;
	}

	if (unlikely(md_init->options & ~PTL_MD_OPTIONS_MASK)) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = ni_get(ni_handle, &ni);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	if (!ni) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	err = md_alloc(ni, &md);
	if (unlikely(err)) {
		WARN();
		goto err2;
	}

	if (md_init->options & PTL_IOVEC) {
		if (md_init->length > ni->limits.max_iovecs) {
			WARN();
			err = PTL_ARG_INVALID;
			goto err3;
		}

		err = init_iovec(ni, md, (ptl_iovec_t *)md_init->start,
				 md_init->length);

		if (err) {
			WARN();
			goto err3;
		}
	} else {
		md->length = md_init->length;
	}

	err = eq_get(md_init->eq_handle, &md->eq);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	if (unlikely(md->eq && (to_ni(md->eq) != ni))) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = ct_get(md_init->ct_handle, &md->ct);
	if (unlikely(err)) {
		WARN();
		goto err3;
	}

	if (unlikely(md->ct && (to_ni(md->ct) != ni))) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err3;
	}

	md->start = md_init->start;
	md->options = md_init->options;

	pthread_spin_lock(&ni->obj.obj_lock);
	ni->current.max_mds++;
	if (unlikely(ni->current.max_mds > ni->limits.max_mds)) {
		pthread_spin_unlock(&ni->obj.obj_lock);
		WARN();
		err = PTL_NO_SPACE;
		goto err3;
	}
	pthread_spin_unlock(&ni->obj.obj_lock);

	*md_handle = md_to_handle(md);

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err3:
	md_put(md);
err2:
	ni_put(ni);
err1:
	gbl_put(gbl);
	return err;
}

int PtlMDRelease(ptl_handle_md_t md_handle)
{
	int err;
	md_t *md;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (unlikely(err)) {
		WARN();
		return err;
	}

	err = md_get(md_handle, &md);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	if (!md) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* There should only be 2 references on the object before we can
	 * release it. */
	if (md->obj.obj_ref.ref_cnt > 2) {
		md_put(md);	/* from md_get */
		WARN();
		err = PTL_IN_USE;
		goto err1;
	}

	md_put(md);	/* from md_get */
	md_put(md);	/* from alloc_md */
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}
