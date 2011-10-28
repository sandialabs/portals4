/**
 * @file ptl_md.h
 */

#ifndef PTL_MD_H
#define PTL_MD_H

/* forward declarations */
struct eq;
struct ct;

/** md object info */
struct md {
	/** object base class */
	obj_t			obj;

	/** start of buff or iovec array from md_init */
	void			*start;

	/** number of entries in iovec arrays */
	unsigned int		num_iov;

	/** size of md data from md_init length or sum of iovecs */
	ptl_size_t		length;

	/** md options */
	unsigned int		options;

	/** optional event queue or NULL */
	struct eq		*eq;

	/** optional counting event or NULL */
	struct ct		*ct;

	/** allocated memory used to hold iovec arrays
	 * or NULL if num_iov is zero */
	void			*internal_data;

	/** scatter gather list passed to the target that
	 * contains a list of addresses, lengths and rkeys
	 * in network byte order for use by messages sent
	 * over the OFA verbs API */
	struct ibv_sge		*sge_list;

	/** mrs to register memory regions for verbs API
	 * can hold one mr per iovec contained in internal_data  */
	mr_t			**mr_list;

	/** list of knem info for each iovec for use in long
	 * messages send through shared memory */
	struct shmem_iovec	*knem_iovecs;

	/** mr to register long array of iovecs passed to target
	 * when num_iov exceeds the amount that will fit in a
	 * short message */
	mr_t			*sge_list_mr;
};

typedef struct md md_t;

void md_cleanup(void *arg);

/**
 * Allocate a new md object.
 *
 * @param ni the ni for which to allocate an md
 * @param md_p the location in which to return the md
 *
 * @return status
 */
static inline int md_alloc(ni_t *ni, md_t **md_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->md_pool, &obj);
	if (err) {
		*md_p = NULL;
		return err;
	}

	*md_p = container_of(obj, md_t, obj);
	return PTL_OK;
}

/**
 * Convert an md handle to an md object.
 *
 * @param handle the md handle
 * @param md_p the location in which to return the md
 *
 * @return status
 */
static inline int to_md(ptl_handle_md_t handle, md_t **md_p)
{
	obj_t *obj;

	obj = to_obj(POOL_MD, (ptl_handle_any_t)handle);
	if (!obj) {
		*md_p = NULL;
		return PTL_ARG_INVALID;
	}

	*md_p = container_of(obj, md_t, obj);
	return PTL_OK;
}

/**
 * Take a reference on an md.
 *
 * @param md the md on which to take a reference
 */
static inline void md_get(md_t *md)
{
	obj_get(&md->obj);
}

/**
 * Drop a reference to an md.
 *
 * @param md the md from which to drop the deference
 *
 * @return status
 */
static inline int md_put(md_t *md)
{
	return obj_put(&md->obj);
}

/**
 * Get md handle from md object.
 *
 * @param md the md from which to get handle
 *
 * @return the handle of the md
 */
static inline ptl_handle_md_t md_to_handle(md_t *md)
{
        return (ptl_handle_md_t)md->obj.obj_handle;
}

#endif /* PTL_MD_H */
