/**
 * @file ptl_obj.c
 */

#include "ptl_loc.h"

/* Maximum number of stored objects ever. */
#define MAX_INDEX	(256*1024)

static atomic_t next_index;
void **index_map;

/**
 * initialize indexing service
 *
 * @return status
 */
int index_init(void)
{
	index_map = calloc(MAX_INDEX, sizeof(void *));
	if (!index_map)
		return PTL_NO_SPACE;

	atomic_set(&next_index, 0);

	return PTL_OK;
}

/**
 * cleanup indexing service
 */
void index_fini(void)
{
	free(index_map);
}

/**
 * Get index for object and save address.
 *
 * @param obj
 * @param index_p
 *
 * @output status
 */
static inline int index_get(obj_t *obj, unsigned int *index_p)
{
	unsigned int index;

	index = atomic_inc(&next_index);

	if (index >= MAX_INDEX)
		return PTL_FAIL;

	index_map[index] = obj;

	*index_p = index;

	return PTL_OK;
}

/**
 * Convert index to object.
 *
 * @param obj_p address of return value
 *
 * @return status
 */
static inline int index_lookup(unsigned int index, obj_t **obj_p)
{
	if (index >= MAX_INDEX) {
		WARN();
		return PTL_FAIL;
	}

	if (index_map[index]) {
		*obj_p = index_map[index];
		return PTL_OK;
	} else {
		return PTL_FAIL;
	}
}

#define HANDLE_SHIFT ((sizeof(ptl_handle_any_t)*8)-8)

/**
 * Return a new zero filled slab.
 *
 * The slab will be used by caller to hold a new
 * batch of objects. Normal behavior is to allocate
 * page aligned memory. In the special case that
 * we are creating objects in shared memory the pool
 * has a pre allocated chunk of shared memory that is
 * used instead.
 *
 * @param pool the pool for which slab is created.
 *
 * @return address of slab or null if unable to allocate memory
 */
static void *pool_get_slab(pool_t *pool)
{
	int err;
	void *slab;

	if (pool->use_pre_alloc_buffer) {
		slab = pool->pre_alloc_buffer;
		pool->pre_alloc_buffer = NULL;
	} else {
		err = posix_memalign(&slab, pagesize, pool->slab_size);
		if (unlikely(err))
			slab = NULL;
	}

	if (slab)
		memset(slab, 0, pool->slab_size);

	return slab;
}

/**
 * get chunk hold new slab.
 * note that we currently never free objects so there are
 * never any holes in chunk->slab_list
 *
 * @pre caller should hold pool->mutex
 *
 * @param pool the pool for which to get chunk
 * @param chunk_p address of return value
 *
 * @return status
 */
static int pool_get_chunk(pool_t *pool, chunk_t **chunk_p)
{
	int err;
	chunk_t *chunk;

	/* see if there is a chunk with room at head of list */
	if (likely(!list_empty(&pool->chunk_list))) {
		chunk = list_first_entry(&pool->chunk_list, chunk_t, list);
		if (chunk->num_slabs < chunk->max_slabs) {
			*chunk_p = chunk;
			return PTL_OK;
		}
	}

	/* have to allocate a new chunk */
	err = posix_memalign((void *)&chunk, pagesize, pagesize);
	if (unlikely(err))
		return PTL_NO_SPACE;

	memset(chunk, 0, pagesize);

	chunk->max_slabs = (pagesize - sizeof(*chunk))/
				sizeof(chunk->slab_list[0]);
	chunk->num_slabs = 0;

	list_add(&chunk->list, &pool->chunk_list);

	*chunk_p = chunk;
	return PTL_OK;
}

/**
 * Return an object from pool freelist.
 *
 * This algorithm is thanks to Kyle and does not
 * require a holding a lock.
 *
 * @param pool the pool
 *
 * @return the object
 */
static inline obj_t *dequeue_free_obj(pool_t *pool)
{
	obj_t *oldv, *newv, *retv;

	retv = pool->free_list;

	do {
		oldv = retv;
		if (retv != NULL) {
			newv = retv->next;
		} else {
			newv = NULL;
		}
		retv = __sync_val_compare_and_swap(&pool->free_list,
						   oldv, newv);
	} while (retv != oldv);

	return retv;
}

/**
 * Add an object to pool freelist.
 *
 * This algorithm is thanks to Kyle and does not
 * require a holding a lock.
 *
 * @param pool the pool
 * @param obj the object
 */
static inline void enqueue_free_obj(pool_t *pool, obj_t *obj)
{
	obj_t *oldv, *newv, *tmpv;

	tmpv = pool->free_list;

	do {
		oldv = obj->next = tmpv;
		newv = obj;
		tmpv = __sync_val_compare_and_swap(&pool->free_list,
						   oldv, newv);
	} while (tmpv != oldv);
}

/**
 * Allocate a new slab of objects for a given pool
 *
 * @param pool
 *
 * @return status
 */
static int pool_alloc_slab(pool_t *pool)
{
	int err;
	chunk_t *chunk = NULL;
	uint8_t *p;
	int i;
	obj_t *obj;
	struct ibv_mr *mr = NULL;
	struct list_head temp_list;

	err = pool_get_chunk(pool, &chunk);
	if (unlikely(err))
		return err;

	p = pool_get_slab(pool);
	if (unlikely(!p))
		return PTL_NO_SPACE;

#if WITH_TRANSPORT_IB
	/*
	 * want to abstract this a little but just hack it for now
	 */
	if (pool->type == POOL_BUF) {
		const ni_t *ni = (ni_t *)pool->parent;
		mr = ibv_reg_mr(ni->iface->pd, p, pool->slab_size,
				IBV_ACCESS_LOCAL_WRITE);
		if (!mr) {
			WARN();
			free(p);
			return PTL_FAIL;
		}
		chunk->slab_list[chunk->num_slabs].priv = mr;
	}
#endif

	chunk->slab_list[chunk->num_slabs++].addr = p;
	INIT_LIST_HEAD(&temp_list);

	for (i = 0; i < pool->obj_per_slab; i++) {
		unsigned int index;

		obj = (obj_t *)p;
		obj->obj_free = 1;
		obj->obj_pool = pool;
		ref_set(&obj->obj_ref, 0);
		obj->obj_parent = pool->parent;
		obj->obj_ni = (pool->parent) ? pool->parent->obj_ni
					     : (ni_t *)obj;

		err = index_get(obj, &index);
		if (err) {
			WARN();
			return err;
		}
		obj->obj_handle	= ((uint64_t)(pool->type) << HANDLE_SHIFT) |
					index;

		if (pool->init) {
			err = pool->init(obj, mr);
			if (err) {
				WARN();
				return PTL_FAIL;
			}
		}
		enqueue_free_obj(pool, obj);
		p += pool->round_size;
	}

	return PTL_OK;
}

/**
 * Cleanup an object pool.
 *
 * @param pool the pool to cleanup
 *
 * @return status
 */
int pool_fini(pool_t *pool)
{
	struct list_head *l, *t;
	obj_t *obj;
	chunk_t *chunk;
	int i;
	int err = PTL_OK;

	/* avoid getting called from cleanup during PtlNIInit */
	if (!pool->name)
		return err;

	/*
	 * if pool has a fini routine call it on
	 * each free object
	 */
	if (pool->fini) {
		while(pool->free_list) {
			obj = pool->free_list;
			pool->free_list = obj->next;
			pool->fini(obj);
		}
	}

	if (atomic_read(&pool->count)) {
		/* There's still an object allocated. Do not free the pool
		 * else we open the library to memory scribble bugs. */
		ptl_warn("leaked %d %s objects\n", atomic_read(&pool->count), pool->name);
		return PTL_FAIL;
	}

	pthread_mutex_destroy(&pool->mutex);

	/*
	 * free slabs and chunks
	 */
	list_for_each_safe(l, t, &pool->chunk_list) {
		list_del(l);
		chunk = list_entry(l, chunk_t, list);

		for (i = 0; i < chunk->num_slabs; i++) {
			/* see below TODO make more elegant */
			if (chunk->slab_list[i].priv) {
				struct ibv_mr *mr = chunk->slab_list[i].priv;

				ibv_dereg_mr(mr);
			}

			if (!pool->use_pre_alloc_buffer)
				free(chunk->slab_list[i].addr);
		}

		free(chunk);
	}

	return err;
}

/**
 * Initialize a new object pool.
 *
 * @pre caller is expected to either set linesize to cache line size
 * and pagesize to system page size or initialize pool->round_size
 * and pool->slab_size to override default values.
 *
 * @param pool the pool to initialize
 * @param name pool name for debugging
 * @param size sizeof objects provided by pool
 * @param type type of objects in pool
 * @param parent object that owns pool
 *
 * @return status
 */
int pool_init(pool_t *pool, char *name, int size,
		  enum obj_type type, obj_t *parent)
{
	pool->name = name;
	pool->size = size;
	pool->type = type;
	pool->parent = parent;

	if (pool->round_size == 0)
		pool->round_size = (pool->size + linesize - 1)
					& ~(linesize - 1);

	if (pool->slab_size == 0)
		pool->slab_size = pagesize;

	pool->obj_per_slab = pool->slab_size/pool->round_size;

	if (!pool->obj_per_slab) {
		ptl_error("Well that's embarassing but "
			  "can't fit any %s's in a slab\n",
			  pool->name);
		return PTL_FAIL;
	}

	atomic_set(&pool->count, 0);
	pool->free_list = NULL;
	INIT_LIST_HEAD(&pool->chunk_list);
	pthread_mutex_init(&pool->mutex, NULL);

	return PTL_OK;
}

/**
 * Release an object back to the free list.
 *
 * Called by obj_put when last reference to an object is dropped.
 *
 * @param ref pointer to obj->obj_ref
 */
void obj_release(ref_t *ref)
{
	obj_t *obj = container_of(ref, obj_t, obj_ref);
	pool_t *pool = obj->obj_pool;

	if (pool->cleanup)
		pool->cleanup(obj);

	if (obj->obj_parent)
		obj_put(obj->obj_parent);

	assert(obj->obj_free == 0);
	obj->obj_free = 1;

	__sync_synchronize();

	enqueue_free_obj(pool, obj);
	atomic_dec(&pool->count);
}

/**
 * Allocate a new object.
 *
 * If the free list is empty allocate a new
 * slab of objects first.
 *
 * @param pool pool to get object from
 * @param obj_p pointer to returned object
 *
 * @return status
 */
int obj_alloc(pool_t *pool, obj_t **obj_p)
{
	int err;
	obj_t *obj;

	/* reserve an object */
	atomic_inc(&pool->count);

	while ((obj = dequeue_free_obj(pool)) == NULL) {

		pthread_mutex_lock(&pool->mutex);
		err = pool_alloc_slab(pool);
		pthread_mutex_unlock(&pool->mutex);

		if (unlikely(err)) {
			if (pool->type == POOL_SBUF) {
				/* Wait for some buffers to be released. */
				SPINLOCK_BODY();
			}
			else {
				atomic_dec(&pool->count);
				WARN();
				return err;
			}
		}
	}

	assert(obj->obj_free == 1);

	ref_set(&obj->obj_ref, 1);

	if (pool->parent)
		obj_get(pool->parent);

	/*
	 * if any type specific per allocation initialization do it
	 */
	if (pool->setup) {
		err = pool->setup(obj);
		if (err) {
			WARN();
			obj_release(&obj->obj_ref);
			return PTL_FAIL;
		}
	}

	obj->obj_free = 0;
	*obj_p = obj;

	return PTL_OK;
}

#ifndef NO_ARG_VALIDATION
/**
 * Return an object from handle and type.
 *
 * This version is only used when arg validation is turned on
 *
 * @param[in] type optional pool type
 * @param[in] handle object handle
 *
 * @return the object
 */
void *to_obj(enum obj_type type, ptl_handle_any_t handle)
{
	int err;
	obj_t *obj = NULL;
	unsigned int index;
	enum obj_type handle_type = (unsigned int)(handle >> HANDLE_SHIFT);

	if (handle == PTL_INVALID_HANDLE) {
		WARN();
		goto err1;
	}

	if (type!=POOL_ANY && handle_type && type != handle_type) {
		WARN();
		goto err1;
	}

	index = obj_handle_to_index(handle);

	err = index_lookup(index, &obj);
	if (err) {
		goto err1;
	}

	if (obj->obj_free) {
		WARN();
		goto err1;
	}

	if (obj_handle_to_index(obj->obj_handle) != index) {
		WARN();
		goto err1;
	}

	if ((type != POOL_ANY) && (type != obj->obj_pool->type)) {
		WARN();
		goto err1;
	}

	obj_get(obj);
	return obj;

err1:
	return NULL;
}
#endif
