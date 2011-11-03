/**
 * @file ptl_me.h 
 *
 * @brief Header for ptl_me.c.
 */
#ifndef PTL_ME_H
#define PTL_ME_H

/* forward declarations */
struct pt;
struct eq;
struct ct;
struct mr;

/**
 * @brief Portals matching list element.
 */
struct me {
	obj_t			obj;
	PTL_LE_OBJ

	ptl_size_t		offset;
	ptl_size_t		min_free;
	uint64_t		match_bits;
	uint64_t		ignore_bits;
	ptl_process_t		id;
};

typedef struct me me_t;

int me_init(void *arg, void *unused);

void me_cleanup(void *arg);

/**
 * @brief Allocate a new matching list element.
 *
 * @param[in] ni The network interface for which to allocate ME.
 * @param[out] me_p The address of the returned ME.
 *
 * @return status
 */
static inline int me_alloc(ni_t *ni, me_t **me_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->me_pool, &obj);
	if (err) {
		*me_p = NULL;
		return err;
	}

	*me_p = container_of(obj, me_t, obj);
	return PTL_OK;
}

/**
 * @brief Convert a matching list element handle to a
 * matching list element object.
 *
 * @param[in] handle The matching list element handle to convert.
 * @param[out] me_p The address of the returned ME object.
 *
 * @return status
 */
static inline int to_me(ptl_handle_me_t handle, me_t **me_p)
{
	obj_t *obj;

	obj = to_obj(POOL_ME, (ptl_handle_any_t)handle);
	if (!obj) {
		*me_p = NULL;
		return PTL_ARG_INVALID;
	}

	*me_p = container_of(obj, me_t, obj);
	return PTL_OK;
}

/**
 * @brief Take a reference to a matching list element.
 *
 * @param[in] le the ME object to reference.
 */
static inline void me_get(me_t *me)
{
	obj_get(&me->obj);
}

/**
 * @brief Drop a reference to a matching list element.
 *
 * If the last reference is dropped free the object.
 *
 * @param[in] le the ME object to dereference.
 *
 * @return status
 */
static inline int me_put(me_t *me)
{
	return obj_put(&me->obj);
}

/**
 * @brief convert an ME object to its handle.
 *
 * @param[in] me the ME object to convert.
 *
 * @return The ME handle.
 */
static inline ptl_handle_me_t me_to_handle(me_t *me)
{
	return (ptl_handle_me_t)me->obj.obj_handle;
}

#endif /* PTL_ME_H */
