/**
 * @file ptl_ref.h
 *
 * Simple reference counting api.
 *
 * Provides thread safe reference counting.
 */

#ifndef PTL_REF_H
#define PTL_REF_H

/**
 * Reference count info.
 *
 * Include this into object that requires reference counting.
 */
typedef struct ref {
	/** The reference count. */
	atomic_t ref_cnt;
} ref_t;

/**
 * Return the reference count.
 *
 * @param ref the ref to set.
 *
 * @return the reference count.
 */
static inline int ref_cnt(struct ref *ref)
{
	return atomic_read(&ref->ref_cnt);
}

/**
 * Set the reference count.
 *
 * Used to initialize a new ref.
 *
 * @param ref the ref to set.
 * @param num the value to set it to.
 */
static inline void ref_set(struct ref *ref, int num)
{
	atomic_set(&ref->ref_cnt, num);
}

/**
 * Get or take a new reference.
 *
 * Uses gcc built in atomic operations.
 * When debugging causes an assert if the new reference count is less than one.
 *
 * @param ref the ref to get a reference to.
 */
static inline void ref_get(struct ref *ref)
{
	int ref_cnt = ref_cnt;

	ref_cnt = atomic_inc(&ref->ref_cnt);

	assert(ref_cnt >= 1);
}

/**
 * Put or drop a reference.
 *
 * Uses gcc built in atomic operations.
 * When debugging causes an assert if the new reference count is less than zero.
 *
 * @param ref the ref to get a reference to.
 * @param release a cleanup routine to call if the resulting reference count reaches zero.
 */
static inline int ref_put(struct ref *ref, void (*release)(ref_t *ref))
{
	int ref_cnt;

	ref_cnt = atomic_dec(&ref->ref_cnt);

	assert(ref_cnt >= 0);

	if (ref_cnt == 0) {
	        release(ref);
	        return 1;
	}

	return 0;
}

#endif /* PTL_REF_H */
