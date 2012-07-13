/**
 * @file ptl_obj.h
 *
 * Object pools
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
 */

#ifndef PTL_POOL_H
#define PTL_POOL_H

struct ni;

/**
 * Pool types.
 * Used as part of the object handle.
 */
enum obj_type {
	POOL_ANY,
	POOL_NI,
	POOL_MR,
	POOL_LE,
	POOL_ME,
	POOL_MD,
	POOL_EQ,
	POOL_CT,
	POOL_BUF,
	POOL_SBUF,
	POOL_PPEBUF,
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
#if WITH_TRANSPORT_IB
	struct ibv_mr	*mr;
#endif
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

	struct gbl *gbl;

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
	union counted_ptr free_list;

	/** pool type */
	enum obj_type		type;

	/** number of objects currently allocated */
	atomic_t	count;

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

#endif
