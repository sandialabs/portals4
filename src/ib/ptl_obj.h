/**
 * @file ptl_obj.h
 *
 * Common object type base class.
 *
 * This interface provides a base object class that
 * is used to hold all the a Portals objects.  Objects are managed
 * in pools which on demand create new objects in 'slabs'.
 *
 * The pool type is modeled loosely on the Linux kernel slab cache
 * data type which is widely used. It provides efficient allocation and
 * deallocation of fixed sized objects without fragmentation and allows
 * memory usage to grow as needed.
 *
 * Slabs are maintained in 'chunks' which are page sized arrays of
 * slab_info structs.
 *
 * And chunks are maintained in circular lists within pools. The
 * current design is very simple and only allows the object resources
 * in a pool to grow. Additional work would allow this design to
 * dynamically grow and shrink the amount of memory used in a given
 * pool.
 *
 * Pools are designed to allow an object to 'own' pools of other objects
 * in a heirarchy. All objects eventually belong to an NI which is the
 * root of Portals4 resources.
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
 * Pool types.
 * Used as part of the object handle.
 */
enum obj_type {
	POOL_NONE,
	POOL_NI,
	POOL_MR,
	POOL_LE,
	POOL_ME,
	POOL_MD,
	POOL_EQ,
	POOL_CT,
	POOL_XI,
	POOL_XT,
	POOL_BUF,
	POOL_SBUF,
	POOL_LAST,		/* keep me last */
};

/**
 * A slab_info struct holds information about a 'slab'.
 * If the pool is used to hold buffers the priv pointer
 * is used to hold information about the memory registration
 * of the slab so that it is not necessary to register each
 * buffer separately.
 */
struct slab_info {
	/** address of slab */
	void			*addr;

	/** slab private data */
	void			*priv;
};

typedef struct slab_info slab_info_t;

/**
 * A chunk is a page sized structure that holds an array
 * of slab_info structs.  Chunks are kept on a circular list
 * in a pool.
 */
struct chunk {
	/** list head to add to pool chunk list */
	struct list_head	list;

	/** number of entries in chunk->slab_list */
	unsigned int		max_slabs;

	/** next slab_list entry to allocate */
	unsigned int		num_slabs;

	/** space for slab_list with max_slabs entries */
	slab_info_t		slab_list[0];
};

typedef struct chunk chunk_t;

/**
 * A pool struct holds information about a type of object
 * that it manages.
 */
struct pool {
	/** object that owns pool (usually NI) */
	struct obj		*parent;

	/** pool name for debugging output */
	char			*name;

	/** if set, called once per object, when object is created */
	int			(*init)(void *arg, void *parm);

	/** if set, called once per object, when object is destroyed */
	void			(*fini)(void *arg);

	/** if set, called when object is allocated from the free list */
	int			(*setup)(void *arg);

	/** if set, called when object moved to the free list */
	void			(*cleanup)(void *arg);

	/** list of chunks each of which holds an array of slab descriptors */
	struct list_head	chunk_list;

	/** lock to protect pool */
	pthread_mutex_t		mutex;

	/** pointer to free list */
	struct obj		*free_list;

	/** pool type */
	enum obj_type		type;

	/** number of objects currently allocated */
	int			count;

	/** object size */
	int			size;

	/** object alignment */
	int			round_size;

	/** size of memory for new slab */
	int			slab_size;

	/** number of objects per slab */
	int			obj_per_slab;

	/** slab is in preallocated memory */
	int			use_pre_alloc_buffer;

	/** address of preallocated slab */
	void			*pre_alloc_buffer;
};

typedef struct pool pool_t;

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

	/** spinlock to protect object */
	pthread_spinlock_t	obj_lock;

	/** object reference count */
	ref_t			obj_ref;

	/** object is free if set */
	int			obj_free;
};

typedef struct obj obj_t;

void **index_map;

int index_init(void);

void index_fini(void);

int pool_init(pool_t *pool, char *name, int size,
	      enum obj_type type, obj_t *parent);

int pool_fini(pool_t *pool);

void obj_release(ref_t *ref);

int obj_alloc(pool_t *pool, obj_t **p_obj);

int to_obj(enum obj_type type, ptl_handle_any_t handle, obj_t **obj_p);

/**
 * Reset an object to all zeros.
 */
#define OBJ_NEW(obj) memset((char *)(obj) + sizeof(obj_t), 0,	\
			    sizeof(*(obj))-sizeof(obj_t))

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

/**
 * Faster version of to_obj without checking.
 *
 * @param handle the object handle
 *
 * @return the object
 */
static inline void *fast_to_obj(ptl_handle_any_t handle)
{
	obj_t *obj = (obj_t *)index_map[handle & HANDLE_INDEX_MASK];
	(void)__sync_fetch_and_add(&obj->obj_ref.ref_cnt, 1);
	return obj;
}

#endif /* PTL_OBJ_H */
