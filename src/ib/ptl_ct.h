/**
 * @file ptl_ct.h
 */

#ifndef PTL_CT_H
#define PTL_CT_H

#include "ptl_locks.h"
#include "ptl_ct_common.h"

struct buf;

/**
 * Counting event object.
 */
struct ct {
	obj_t			obj;		/**< object base class */
	struct list_head	trig_list;	/**< list head of pending
						     triggered operations */
	struct list_head        list;		/**< list member of allocated
						     counting events */
	atomic_t list_size;			/**< Number of elements in list */

	PTL_FASTLOCK_TYPE		lock;		/**< mutex for ct condition */

	struct ct_info info;
};

typedef struct ct ct_t;

enum trig_ct_op {
	TRIG_CT_SET = 1,
	TRIG_CT_INC,
};

enum ct_bytes {
	CT_EVENTS,	/**< count events */
	CT_RBYTES,	/**< count requested bytes */
	CT_MBYTES,	/**< count modified/actual bytes */
};

int ct_init(void *arg, void *unused);

void ct_fini(void *arg);

int ct_new(void *arg);

void ct_cleanup(void *arg);

void post_ct(struct buf *buf, ct_t *ct);

void post_ct_local(struct buf *buf, ct_t *ct);

void make_ct_event(ct_t *ct, struct buf *buf, enum ct_bytes bytes);

/**
 * Allocate a new ct object.
 *
 * @param[in] ni the ni for which to allocate the ct
 * @param[out] ct_p a pointer to the return value
 *
 * @return status
 */
static inline int ct_alloc(ni_t *ni, ct_t **ct_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->ct_pool, &obj);
	if (unlikely(err)) {
		*ct_p = NULL;
		return err;
	}

	*ct_p = container_of(obj, ct_t, obj);
	return PTL_OK;
}

/**
 * Convert a ct handle to a ct object.
 *
 * Takes a reference to the ct object.
 *
 * @param[in] ct_handle the ct handle to convert
 * @param[out] ct_p a pointer to the return value
 *
 * @return status
 */
static inline int to_ct(ptl_handle_ct_t ct_handle, ct_t **ct_p)
{
	obj_t *obj;

	if (ct_handle == PTL_CT_NONE) {
		*ct_p = NULL;
		return PTL_OK;
	}

	obj = to_obj(POOL_CT, (ptl_handle_any_t)ct_handle);
	if (unlikely(!obj)) {
		*ct_p = NULL;
		return PTL_ARG_INVALID;
	}

	*ct_p = container_of(obj, ct_t, obj);
	return PTL_OK;
}

/**
 * Take a reference to a ct object.
 *
 * @param[in] ct the ct object to take reference to
 */
static inline void ct_get(ct_t *ct)
{
	obj_get(&ct->obj);
}

/**
 * Drop a reference to a ct object
 *
 * If the last reference is dropped cleanup and free the object.
 *
 * @param[in] ct the ct to drop the reference to
 *
 * @return status
 */
static inline int ct_put(ct_t *ct)
{
	return obj_put(&ct->obj);
}

/**
 * Convert ct object to its handle.
 *
 * @param[in] ct the ct object from which to get handle
 *
 * @return the ct_handle
 */
static inline ptl_handle_ct_t ct_to_handle(ct_t *ct)
{
        return (ptl_handle_ct_t)ct->obj.obj_handle;
}

#endif /* PTL_CT_H */
