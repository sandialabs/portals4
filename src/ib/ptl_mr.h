/**
 * @file ptl_mr.h
 *
 * This file contains the interface for the
 * mr (memory region) class which contains
 * information about OFA verbs memory regions.
 */
#ifndef PTL_MR_H
#define PTL_MR_H

/**
 * mr class info.
 */
typedef struct mr {
	/** base class */
	obj_t			obj;

	struct list_head list;		/* internal management */

	/* Boundaries of the region. */
	void           *addr;
	size_t          length;

#if WITH_TRANSPORT_IB
	/** OFA verbs mr for memory region */
	struct ibv_mr		*ibmr;
#endif

#if WITH_TRANSPORT_SHMEM
	/** knem cookie for memory region */
	uint64_t		knem_cookie;
#endif

#if IS_PPE
	/* To share with PPE. */
	xpmem_segid_t segid;
#endif

	/* ummunotify kernel memory notification cookie */
	uint64_t umn_cookie;

	/** entry in mr cache */
	RB_ENTRY(mr)		entry;
} mr_t;

void mr_cleanup(void *arg);

int mr_lookup(ni_t *ni, void *start, ptl_size_t length, mr_t **mr);

void cleanup_mr_tree(ni_t *ni);
void mr_init(ni_t *ni);

/**
 * Allocate an mr object.
 *
 * @param[in] ni from which to allocate mr
 * @param[out] mr_p address of return value
 *
 * @return status
 */
static inline int mr_alloc(ni_t *ni, mr_t **mr_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->mr_pool, &obj);
	if (err) {
		*mr_p = NULL;
		return err;
	}

	*mr_p = container_of(obj, mr_t, obj);
	return PTL_OK;
}

/**
 * Take a reference to an mr object
 *
 * @param[in] mr to take reference to
 */
static inline void mr_get(mr_t *mr)
{
	obj_get(&mr->obj);
}

/**
 * Drop a reference to mr object
 *
 * @param[in] mr to drop reference to
 *
 * @return status
 */
static inline int mr_put(mr_t *mr)
{
	return obj_put(&mr->obj);
}

#endif /* PTL_MR_H */
