/**
 * @file ptl_md.h
 */

#include "ptl_loc.h"

/**
 * cleanup an md when last reference is dropped.
 *
 * @param[in] arg opaque address of md
 */
void md_cleanup(void *arg)
{
	md_t *md = arg;
	ni_t *ni = obj_to_ni(md);
	int i;

	if (md->eq) {
		eq_put(md->eq);
		md->eq = NULL;
	}

	if (md->ct) {
		ct_put(md->ct);
		md->ct = NULL;
	}

	for (i = 0; i < md->num_iov; i++) {
		if (md->mr_list[i]) {
			mr_put(md->mr_list[i]);
			md->mr_list[i] = NULL;
		}
	}

	if (md->sge_list_mr) {
		mr_put(md->sge_list_mr);
		md->sge_list_mr = NULL;
	}

	if (md->internal_data) {
		free(md->internal_data);
		md->internal_data = NULL;
	}

	(void)__sync_sub_and_fetch(&ni->current.max_mds, 1);
}

/**
 * Initialize iovec arrays for md.
 *
 * @param[in] ni the ni that md belongs to
 * @param[in] md the md to initialize
 * @param[in] iov_list the iovec array
 * @param[in] num_iov the size of the iovec array
 *
 * @return status
 */
static int init_iovec(md_t *md, ptl_iovec_t *iov_list, int num_iov)
{
	int err;
	ni_t *ni = obj_to_ni(md);
	ptl_iovec_t *iov;
	struct ibv_sge *sge;
	struct shmem_iovec *knem_iovec;
	void *p;
	int i;

	md->num_iov = num_iov;

	md->internal_data = calloc(num_iov, sizeof(struct ibv_sge) +
				   sizeof(mr_t) + sizeof(struct shmem_iovec));
	if (!md->internal_data) {
		err = PTL_NO_SPACE;
		goto err1;
	}

	p = md->internal_data;

	md->sge_list = p;
	p += num_iov*sizeof(struct ibv_sge);

	md->mr_list = p;
	p += num_iov*sizeof(mr_t);

	md->knem_iovecs = p;
	p += num_iov*sizeof(struct shmem_iovec);

	if (num_iov > get_param(PTL_MAX_INLINE_SGE)) {
		/* Pin the whole thing. It's not big enough to make a
		 * difference. */
		err = mr_lookup(ni, md->internal_data,
				p - md->internal_data, &md->sge_list_mr);
		if (err)
			goto err2;
	} else {
		md->sge_list_mr = NULL;
	}

	md->length = 0;

	iov = iov_list;
	sge = md->sge_list;
	knem_iovec = md->knem_iovecs;

	for (i = 0; i < num_iov; i++) {
		mr_t *mr;

		md->length += iov->iov_len;

		err = mr_lookup(ni, iov->iov_base,
				iov->iov_len, &md->mr_list[i]);
		if (err)
			goto err3;

		mr = md->mr_list[i];

		sge->addr = cpu_to_be64((uintptr_t)iov->iov_base);
		sge->length = cpu_to_be32(iov->iov_len);
		sge->lkey = cpu_to_be32(mr->ibmr->rkey);

		knem_iovec->cookie = mr->knem_cookie;
		knem_iovec->offset = iov->iov_base - mr->ibmr->addr;
		knem_iovec->length = iov->iov_len;

		iov++;
		sge++;
		knem_iovec++;
	}

	return PTL_OK;

err3:
	for (i--; i >= 0; i--)
		mr_put(md->mr_list[i]);

	if (md->sge_list_mr)
		mr_put(md->sge_list_mr);
err2:
	free(md->internal_data);
	md->internal_data = NULL;
err1:
	return err;
}

/**
 * Create a new MD and bind to NI.
 *
 * @param ni_handle
 * @param md_init
 * @param md_handle_p
 *
 * @return status
 */
int PtlMDBind(ptl_handle_ni_t ni_handle, const ptl_md_t *md_init,
              ptl_handle_md_t *md_handle_p)
{
	int err;
	ni_t *ni;
	md_t *md;

	err = get_gbl();
	if (unlikely(err))
		goto err0;

	if (unlikely(md_init->options & ~PTL_MD_OPTIONS_MASK)) {
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

	err = md_alloc(ni, &md);
	if (unlikely(err))
		goto err2;

	if (md_init->options & PTL_IOVEC) {
		if (md_init->length > ni->limits.max_iovecs) {
			err = PTL_ARG_INVALID;
			goto err3;
		}

		err = init_iovec(md, (ptl_iovec_t *)md_init->start,
				 md_init->length);
		if (err)
			goto err3;
	} else {
		md->length = md_init->length;
		md->num_iov = 0;
	}

	err = to_eq(md_init->eq_handle, &md->eq);
	if (unlikely(err))
		goto err3;

	if (unlikely(md->eq && (obj_to_ni(md->eq) != ni))) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = to_ct(md_init->ct_handle, &md->ct);
	if (unlikely(err))
		goto err3;

	if (unlikely(md->ct && (obj_to_ni(md->ct) != ni))) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	md->start = md_init->start;
	md->options = md_init->options;

	/* account for the number of MDs allocated */
	if (unlikely(__sync_add_and_fetch(&ni->current.max_mds, 1) >
	    ni->limits.max_mds)) {
		(void)__sync_sub_and_fetch(&ni->current.max_mds, 1);
		err = PTL_NO_SPACE;
		goto err3;
	}

	*md_handle_p = md_to_handle(md);

	ni_put(ni);
	gbl_put();
	return PTL_OK;

err3:
	md_put(md);
err2:
	ni_put(ni);
err1:
	gbl_put();
err0:
	return err;
}

/**
 * Release MD from NI.
 *
 * If this is the last reference to the MD destroy the object.
 *
 * @param md_handle the handle of the MD to release
 *
 * @return status
 */
int PtlMDRelease(ptl_handle_md_t md_handle)
{
	int err;
	md_t *md;

	err = get_gbl();
	if (unlikely(err))
		goto err0;

	err = to_md(md_handle, &md);
	if (unlikely(err))
		goto err1;

	if (unlikely(!md)) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	/* simultaneous calls to PtlMDRelease, PtlGet/Put/etc, and/or
	 * the completion of move operations can lead to races
	 * it is the responsibility of the caller to make sure this
	 * doesn't happen here. The dangerous case is when the caller
	 * makes a call to a move operation and release at the same
	 * time and the release wins and deletes the MD.  A thread safe
	 * version would disable the MD under a lock. */
	if (md->obj.obj_ref.ref_cnt > 2) {
		err = PTL_IN_USE;
		goto err2;
	}

	md_put(md);	/* from to_md above */
	md_put(md);	/* from alloc_md */
	gbl_put();
	return PTL_OK;

err2:
	md_put(md);	/* from to_md */
err1:
	gbl_put();
err0:
	return err;
}
