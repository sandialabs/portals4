/**
 * @file ptl_md.h
 *
 * @brief Portals MD APIs
 */

#include "ptl_loc.h"

/**
 * @brief Cleanup an md when last reference is dropped.
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
 * @return PTL_OK Indicates success.
 * @return PTL_NO_SPACE Indicates that there is insufficient memory to
 * allocate the sge lists for the iovec.
 */
static int init_iovec(md_t *md, const ptl_iovec_t *iov_list, int num_iov)
{
	int err;
	ni_t *ni = obj_to_ni(md);
	const ptl_iovec_t *iov;
#if WITH_TRANSPORT_IB
	struct ibv_sge *sge;
#endif
#if WITH_TRANSPORT_SHMEM
	struct mem_iovec *mem_iovec;
#endif
	void *p;
	int i;

	md->num_iov = num_iov;

	md->internal_data = calloc(num_iov, sizeof(mr_t)
#if WITH_TRANSPORT_IB
							   + sizeof(struct ibv_sge)
#endif
#if WITH_TRANSPORT_SHMEM
							   + sizeof(struct mem_iovec)
#endif
							   );
	if (!md->internal_data) {
		err = PTL_NO_SPACE;
		goto err1;
	}

	p = md->internal_data;

	md->mr_list = p;
	p += num_iov*sizeof(mr_t);

#if WITH_TRANSPORT_IB
	sge = md->sge_list = p;
	p += num_iov*sizeof(struct ibv_sge);
#endif

#if WITH_TRANSPORT_SHMEM
	mem_iovec = md->mem_iovecs = p;
	p += num_iov*sizeof(struct mem_iovec);
#endif

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

	for (i = 0; i < num_iov; i++) {
		mr_t *mr;

		md->length += iov->iov_len;

		err = mr_lookup(ni, iov->iov_base,
				iov->iov_len, &md->mr_list[i]);
		if (err)
			goto err3;

		mr = md->mr_list[i];

#if WITH_TRANSPORT_IB
		sge->addr = cpu_to_le64((uintptr_t)iov->iov_base);
		sge->length = cpu_to_le32(iov->iov_len);
		sge->lkey = cpu_to_le32(mr->ibmr->rkey);
		sge++;
#endif

#if WITH_TRANSPORT_SHMEM
		mem_iovec->cookie = mr->knem_cookie;
		mem_iovec->offset = iov->iov_base - mr->addr;
		mem_iovec->length = iov->iov_len;
		mem_iovec++;
#endif

		iov++;
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
 * @brief Create a new MD and bind to NI.
 *
 * @param ni_handle
 * @param md_init
 * @param md_handle_p
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was
 * passed. Argument checking is implementation dependent, but this
 * may indicate that an invalid ni_handle was used, an invalid event
 * queue was associated with the md, or other contents in the md
 * were illegal.
 * @return PTL_NO_SPACE Indicates that there is insufficient memory
 * to allocate the memory descriptor.
 */
int PtlMDBind(ptl_handle_ni_t ni_handle, const ptl_md_t *md_init,
	      ptl_handle_md_t *md_handle_p)
{
	int err;
	ni_t *ni;
	md_t *md;

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

	if (md_init->options & ~PTL_MD_OPTIONS_MASK) {
		err = PTL_ARG_INVALID;
		goto err2;
	}

	if (md_init->options & PTL_IOVEC) {
		if (md_init->length > ni->limits.max_iovecs) {
			err = PTL_ARG_INVALID;
			goto err2;
		}
	}
#else
	ni = fast_to_obj(ni_handle);
#endif

	err = md_alloc(ni, &md);
	if (unlikely(err))
		goto err2;

	if (md_init->options & PTL_IOVEC) {
		err = init_iovec(md, (ptl_iovec_t *)md_init->start,
				 md_init->length);
		if (err)
			goto err3;
	} else {
		md->length = md_init->length;
		md->num_iov = 0;
	}

#ifndef NO_ARG_VALIDATION
	err = to_eq(md_init->eq_handle, &md->eq);
	if (err)
		goto err3;

	if (md->eq && (obj_to_ni(md->eq) != ni)) {
		err = PTL_ARG_INVALID;
		goto err3;
	}

	err = to_ct(md_init->ct_handle, &md->ct);
	if (err)
		goto err3;

	if (md->ct && (obj_to_ni(md->ct) != ni)) {
		err = PTL_ARG_INVALID;
		goto err3;
	}
#else
	md->eq = (md_init->eq_handle != PTL_EQ_NONE) ?
		fast_to_obj(md_init->eq_handle) : NULL;
	md->ct = (md_init->ct_handle != PTL_CT_NONE) ?
		fast_to_obj(md_init->ct_handle) : NULL;
#endif

	md->start = md_init->start;
	md->options = md_init->options;

	//	printf("FZ[%d] - md bind start=%p, len=%zd\n", getpid(), md->start, md->length);

	/* account for the number of MDs allocated */
	if (unlikely(__sync_add_and_fetch(&ni->current.max_mds, 1) >
	    ni->limits.max_mds)) {
		(void)__sync_sub_and_fetch(&ni->current.max_mds, 1);
		err = PTL_NO_SPACE;
		goto err3;
	}

	*md_handle_p = md_to_handle(md);

	ni_put(ni);
#ifndef NO_ARG_VALIDATION
	gbl_put();
#endif
	return PTL_OK;

err3:
	md_put(md);
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
 * @brief Release MD from NI.
 *
 * If this is the last reference to the MD destroy the object.
 *
 * @param md_handle the handle of the MD to release
 *
 * @return PTL_OK Indicates success.
 * @return PTL_NO_INIT Indicates that the portals API has not been
 * successfully initialized.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was
 * passed. The definition of which arguments are checked is
 * implementation dependent.
 * @return PTL_IN_USE Indicates that md_handle has pending operations
 * and cannot be released.
 */
int PtlMDRelease(ptl_handle_md_t md_handle)
{
	int err;
	md_t *md;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	md = to_md(md_handle);
	if (!md) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	md = fast_to_obj(md_handle);
#endif

	/* Ensure there is no in-flight transfer. */
	if (ref_cnt(&md->obj.obj_ref) > 2) {
		err = PTL_ARG_INVALID;
	} else {
		err = PTL_OK;
		md_put(md);
	}

	md_put(md);

#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif

	return err;
}
