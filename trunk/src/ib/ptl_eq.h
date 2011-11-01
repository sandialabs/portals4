/**
 * @file ptl_eq.h
 *
 * Event queue interface declarations.
 * @see ptl_eq.c
 */

#ifndef PTL_EQ_H
#define PTL_EQ_H

/**
 * Event queue entry.
 */
struct eqe {
	unsigned int		generation;	/**< increments each time
						     the producer pointer
						     wraps */
	ptl_event_t		event;		/**< portals event */
};

typedef struct eqe eqe_t;

/**
 * Event queue info.
 */
struct eq {
	obj_t			obj;		/**< object base class */
	eqe_t			*eqe_list;	/**< circular buffer for
						     holding events */
	unsigned int		count;		/**< size of event queue */
	unsigned int		producer;	/**< producer index */
	unsigned int		consumer;	/**< consumer index */
	unsigned int		prod_gen;	/**< producer generation */
	unsigned int		cons_gen;	/**< consumer generation */
	int			interrupt;	/**< if set eq is being
						     freed or destroyed */
	int			overflow;	/**< true if producer has
						     wrapped the consumer */
	pthread_mutex_t		mutex;		/**< mutex for eq condition */
	pthread_cond_t		cond;		/**< condition to break out
						     of eq wait calls */
	unsigned int		waiters;	/**< number of waiters for
						     eq condition */

};

typedef struct eq eq_t;

int eq_init(void *arg, void *unused);

void eq_fini(void *arg);

int eq_new(void *arg);

void eq_cleanup(void *arg);

void make_init_event(buf_t *buf, eq_t *eq, ptl_event_kind_t type, void *start);

void fill_target_event(buf_t *buf, eq_t *eq, ptl_event_kind_t type,
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
static inline int to_eq(ptl_handle_eq_t eq_handle, eq_t **eq_p)
{
	obj_t *obj;

	if (eq_handle == PTL_EQ_NONE) {
		*eq_p = NULL;
		return PTL_OK;
	}

	obj = to_obj(POOL_EQ, (ptl_handle_any_t)eq_handle);
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
