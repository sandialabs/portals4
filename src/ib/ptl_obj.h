/** 
 * @file ptl_obj.h
 *
 * Common object type base class.
 *
 * This interface provides a base object class that
 * is used to hold all the a Portals objects.  Objects are managed
 * in pools which on demand create new objects in 'slabs'.
 *
 * Each base object has a handle assigned which can be used to lookup
 * the object. The handle includes an object type and an index into
 * an array of object pointers.
 *
 * Each base object also has a reference count that is used to handle
 * object cleanup. When an object is allocated its ref count is set to 1.
 * Other objects or code that depends on being able to reference the
 * object can take additional references to the object and free them
 * when done. When the final reference is dropped the object is cleaned
 * up and put back on the pool freelist.
 */

#ifndef PTL_OBJ_H
#define PTL_OBJ_H

struct ni;

/**
 * An obj struct is the base type for all Portals objects.
 * Various object types (e.g. NI, MD, LE, etc.) should include
 * a struct obj as its first member.
 */
struct obj {
	/** freelist chain pointer */
	struct obj		*next;

	/** backpointer to pool that owns object */
	pool_t			*obj_pool;

	/** backpointer to parent that owns pool */
	struct obj		*obj_parent;

	/** backpointer to NI that owns parent */
	struct ni		*obj_ni;

	/** object handle */
	ptl_handle_any_t	obj_handle;

	/** object reference count */
	ref_t			obj_ref;

	/** object is free if set */
	int			obj_free;
};

typedef struct obj obj_t;

struct gbl;
int index_init(struct gbl *gbl);

void index_fini(struct gbl *gbl);

int pool_init(struct gbl *gbl, pool_t *pool, char *name, int size,
	      enum obj_type type, obj_t *parent);

int pool_fini(pool_t *pool);

void obj_release(ref_t *ref);

int obj_alloc(pool_t *pool, obj_t **p_obj);

/**
 * Reset an object to all zeros.
 */
#define OBJ_NEW(obj) memset((char *)(obj) + sizeof(obj_t), 0,	\
			    sizeof(*(obj))-sizeof(obj_t))

/**
 * Return the ref count of an object
 *
 * @param obj the object
 */
static inline int obj_ref_cnt(obj_t *obj)
{
	return ref_cnt(&obj->obj_ref);
}

/**
 * Take a reference to an object.
 *
 * @param obj the object
 */
static inline void obj_get(obj_t *obj)
{
	ref_get(&obj->obj_ref);
}

/**
 * Drop a reference to an object.
 *
 * If the last reference has been dropped call obj_release to
 * free object resources.
 *
 * @param obj the object
 *
 * @return status
 */
static inline int obj_put(obj_t *obj)
{
	return ref_put(&obj->obj_ref, obj_release);
}

#define HANDLE_INDEX_MASK	(0x00ffffff)

/**
 * Convert a handle to an object index.
 *
 * @param handle the handle
 *
 * @return the object
 */
static inline unsigned int obj_handle_to_index(ptl_handle_any_t handle)
{
	return handle & HANDLE_INDEX_MASK;
}

#ifdef NO_ARG_VALIDATION
/**
 * Faster version of to_obj without checking.
 *
 * Handle must not be PTL_XX_NONE
 * type is unused
 *
 * @param handle the object handle
 *
 * @return the object
 */
static inline void *to_obj(PPEGBL enum obj_type type, ptl_handle_any_t handle)
{
	obj_t *obj = (obj_t *)MYGBL->index_map[handle & HANDLE_INDEX_MASK];
	atomic_inc(&obj->obj_ref.ref_cnt);
	return obj;
}
#else
void *to_obj(PPEGBL enum obj_type type, ptl_handle_any_t handle);
#endif

#endif /* PTL_OBJ_H */
