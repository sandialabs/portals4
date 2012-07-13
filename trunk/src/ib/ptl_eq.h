/**
 * @file ptl_eq.h
 *
 * Event queue interface declarations.
 * @see ptl_eq.c
 */

#ifndef PTL_EQ_H
#define PTL_EQ_H

#include "ptl_locks.h"
#include "ptl_eq_common.h"

/**
 * Event queue info.
 */
struct eq {
	obj_t			obj;		/**< object base class */
	struct eqe_list		*eqe_list;	/**< circular buffer for
									   holding events */
	int eqe_list_size;
	unsigned int		count_simple;		/**< size of event queue minus reserved entries */

	/** to attach the PTs supporting flow control. **/
	struct list_head	flowctrl_list;
	int overflowing;			/* the queue is overflowing */

#if IS_PPE
	/* PPE transport specific */
	struct {
		struct xpmem_map eqe_list; /* for the eqe_list */
	} ppe;
#endif
};

typedef struct eq eq_t;

int eq_new(void *arg);

void eq_cleanup(void *arg);

void make_init_event(buf_t *buf, eq_t *eq, ptl_event_kind_t type);

void fill_target_event(buf_t *buf, ptl_event_kind_t type,
		       void *user_ptr, void *start, ptl_event_t *ev);

void send_target_event(eq_t *eq, ptl_event_t *ev);

void make_target_event(buf_t *buf, eq_t *eq, ptl_event_kind_t type,
		       void *user_ptr, void *start);

void make_le_event(le_t *le, eq_t *eq, ptl_event_kind_t type,
		   ptl_ni_fail_t fail_type);

/**
 * Allocate a new eq object.
 *
 * Takes a reference on the eq object.
 *
 * @param[in] ni the ni for which to allocate the eq
 * @param eq_p[out] address of the returned eq
 *
 * @return status
 */
static inline int eq_alloc(ni_t *ni, eq_t **eq_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->eq_pool, &obj);
	if (err) {
		*eq_p = NULL;
		return err;
	}

	*eq_p = container_of(obj, eq_t, obj);
	return PTL_OK;
}

/**
 * Convert from an eq handle to the eq object.
 *
 * Takes a reference to the object.
 *
 * @param[in] eq_handle the handle of the eq object
 * @param eq_p[out] address of the returned eq
 *
 * @return status
 */
static inline int to_eq(PPEGBL ptl_handle_eq_t eq_handle, eq_t **eq_p)
{
	obj_t *obj;

	if (eq_handle == PTL_EQ_NONE) {
		*eq_p = NULL;
		return PTL_OK;
	}

	obj = to_obj(MYGBL_ POOL_EQ, (ptl_handle_any_t)eq_handle);
	if (!obj) {
		*eq_p = NULL;
		return PTL_ARG_INVALID;
	}

	*eq_p = container_of(obj, eq_t, obj);
	return PTL_OK;
}

/**
 * Take a reference on an eq object.
 *
 * @param[in] eq the eq object
 */
static inline void eq_get(eq_t *eq)
{
	obj_get(&eq->obj);
}

/**
 * Drop a reference to an eq object.
 *
 * When the last reference is dropped the eq is freed.
 *
 * @param[in] eq the eq object
 *
 * @return status
 */
static inline int eq_put(eq_t *eq)
{
	return obj_put(&eq->obj);
}

/**
 * Convert eq object to its handle.
 *
 * @param[in] eq the eq object
 *
 * @return eq handle
 */
static inline ptl_handle_eq_t eq_to_handle(eq_t *eq)
{
	return (ptl_handle_eq_t)eq->obj.obj_handle;
}

#endif /* PTL_EQ_H */
