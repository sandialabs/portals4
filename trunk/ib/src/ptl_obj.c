/*
 * ptl_obj.c -- object management
 */

#include "ptl_loc.h"

#define HANDLE_SHIFT ((sizeof(ptl_handle_any_t)*8)-8)

static void *obj_get_zero_filled_segment(pool_t *pool)
{
	int err;
	void *p;

	err = posix_memalign(&p, pagesize, pool->segment_size);
	if (unlikely(err))
		return NULL;

	memset(p, 0, pool->segment_size);

	return p;
}

/*
 * pool_init
 *	caller is expected to set linesize to cache line size
 *	and pagesize to system page size
 */
int pool_init(pool_t *pool, char *name, int size,
		  int type, obj_t *parent)
{
	int err;

	pool->name = name;
	pool->size = size;
	pool->type = type;
	pool->parent = parent;

	pool->round_size = (pool->size + linesize - 1) & ~(linesize - 1);

	if (pool->segment_size < pagesize)
		pool->segment_size = pagesize;

	pool->segment_size = (pool->segment_size + pagesize - 1) &
			     ~(pagesize - 1);

	pool->obj_per_segment = pool->segment_size/pool->round_size;
	if (!pool->obj_per_segment) {
		ptl_error("Well that's embarassing but "
			  "can't fit any %s's in a segment\n",
			  pool->name);
		return PTL_FAIL;
	}

	pool->count = 0;

	INIT_LIST_HEAD(&pool->free_list);
	INIT_LIST_HEAD(&pool->chunk_list);

	/* would like to use spinlock but need a mutex for
	 * cond_wait and posix_memalign which can schedule */
	err = pthread_mutex_init(&pool->mutex, NULL);
	if (err) {
		WARN();
		return PTL_FAIL;
	}

	err = pthread_cond_init(&pool->cond, NULL);
	if (err) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

void pool_fini(pool_t *pool)
{
	int err;
	struct list_head *l, *t;
	obj_t *obj;
	segment_list_t *chunk;
	int i;

	/* avoid getting called from cleanup during PtlNIInit */
	if (!pool->name)
		return;

	if (pool->count) {
		/*
		 * we shouldn't get here since we
		 * are supposed to clean up all the
		 * objects during processing PtlFini
		 * so this would be a library bug
		 */
		ptl_warn("leaked %d %s objects\n", pool->count, pool->name);
		ptl_test_return = PTL_FAIL;
	}

	/*
	 * if pool has a cleanup routine call it
	 */
	if (pool->fini) {
		pthread_mutex_lock(&pool->mutex);
		list_for_each(l, &pool->free_list) {
			obj = list_entry(l, obj_t, obj_list);
			pool->fini(obj);
		}
		pthread_mutex_unlock(&pool->mutex);
	}

	err = pthread_cond_destroy(&pool->cond);
	if (err)
		WARN();

	err = pthread_mutex_destroy(&pool->mutex);
	if (err)
		WARN();

	/*
	 * free the segments
	 */
	list_for_each_safe(l, t, &pool->chunk_list) {
		list_del(l);
		chunk = list_entry(l, segment_list_t, chunk_list);

		for (i = 0; i < chunk->num_segments; i++) {
			/* see below TODO make more elegant */
			if (chunk->segment_list[i].priv) {
				struct ibv_mr *mr = chunk->segment_list[i].priv;

				ibv_dereg_mr(mr);
			}

			free(chunk->segment_list[i].addr);
		}

		free(chunk);
	}
}

/*
 * type_get_segment_list_pointer
 *	get pointer to segment_list with room to hold
 *	a new segment pointer
 *	caller should hold type->mutex
 *	note that we never free object memory so there are
 *	never any holes in a segment list
 */
static int type_get_segment_list_pointer(pool_t *pool, segment_list_t **pp)
{
	int err;
	struct list_head *l = pool->chunk_list.next;
	segment_list_t *p = list_entry(l, segment_list_t, chunk_list);
	void *d;

	if (likely(!list_empty(&pool->chunk_list)
	    && (p->num_segments < p->max_segments))) {
		*pp = p;
		return PTL_OK;
	}

	/* have to allocate a new segment list */
	err = posix_memalign(&d, pagesize, pagesize);
	if (unlikely(err))
		return PTL_NO_SPACE;

	p = d;

	memset(p, 0, pagesize);

	p->num_segments = 0;
	p->max_segments = (pool->segment_size - sizeof(*p))/sizeof(void *);
	list_add(&p->chunk_list, &pool->chunk_list);

	*pp = p;
	return PTL_OK;
}

/*
 * pool_alloc_segment
 *	allocate a new segment of objects for a given pool
 *	caller should hold pool->mutex
 */
static int pool_alloc_segment(pool_t *pool)
{
	int err;
	segment_list_t *pp = NULL;
	uint8_t *p;
	int i;
	obj_t *obj;
	struct ibv_mr *mr = NULL;
	ni_t *ni;

	err = type_get_segment_list_pointer(pool, &pp);
	if (unlikely(err))
		return err;

	p = obj_get_zero_filled_segment(pool);
	if (unlikely(!p))
		return PTL_NO_SPACE;

	/*
	 * want to abstract this a little but just hack it for now
	 */
	if (pool->type == POOL_BUF) {
		ni = (ni_t *)pool->parent;
		mr = ibv_reg_mr(ni->iface->pd, p, pool->segment_size,
				IBV_ACCESS_LOCAL_WRITE);
		if (!mr) {
			WARN();
			return PTL_FAIL;
		}
		pp->segment_list[pp->num_segments].priv = mr;
	}

	pp->segment_list[pp->num_segments++].addr = p;

	for (i = 0; i < pool->obj_per_segment; i++) {
		obj = (obj_t *)p;
		obj->obj_free = 1;
		obj->obj_pool = pool;
		ref_set(&obj->obj_ref, 0);
		obj->obj_parent = pool->parent;
		obj->obj_ni = (pool->parent) ? pool->parent->obj_ni
					     : (ni_t *)obj;
		if (pool->init) {
			err = pool->init(obj, mr);
			if (err) {
				WARN();
				return PTL_FAIL;
			}
		}
		list_add(&obj->obj_list, &pool->free_list);
		p += pool->round_size;
	}

	return PTL_OK;
}

/*
 * obj_release
 *	release an object back to the free list
 *	called by obj_put when last reference
 *	is dropped
 */
void obj_release(ref_t *ref)
{
	int err;
	obj_t *obj = container_of(ref, obj_t, obj_ref);
	struct list_head *l = &obj->obj_list;
	pool_t *pool = obj->obj_pool;
	unsigned int index = obj_handle_to_index(obj->obj_handle);
	int do_signal = 0;

	err = index_free(index);
	if (err)
		WARN();

	obj->obj_handle = 0;

	if (pool->free)
		pool->free(obj);

	if (obj->obj_parent)
		obj_put(obj->obj_parent);

	obj->obj_free = 1;

	pthread_mutex_lock(&pool->mutex);
	list_add_tail(l, &pool->free_list);
	pool->count--;
	if (pool->max_count && pool->count < pool->min_count && pool->waiters)
		do_signal = 1;
	pthread_mutex_unlock(&pool->mutex);

	if (do_signal)
		pthread_cond_signal(&pool->cond);
}

/*
 * obj_alloc
 *	allocate a new object of indicated type
 *	zero fill after the base type
 */
int obj_alloc(pool_t *pool, obj_t **p_obj)
{
	int err;
        obj_t *obj;
	struct list_head *l;
	unsigned int index = 0;
	struct timespec timeout;

	pthread_mutex_lock(&pool->mutex);

	/*
	 * if the pool has a size limit and we are over it
	 * wait until enough objects have been freed to get
	 * under the limit, give up after a few seconds
	 */
	if (pool->max_count && pool->count >= pool->max_count) {
		pool->waiters++;
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += get_param(PTL_OBJ_ALLOC_TIMEOUT);
		do {
			err = pthread_cond_timedwait(&pool->cond, &pool->mutex, &timeout);
			if (err) {
				pthread_mutex_unlock(&pool->mutex);
				return PTL_FAIL;
			}
		} while(pool->count >= pool->max_count);
		pool->waiters--;
	}

	/* reserve an object */
	pool->count++;

	/*
	 * if the pool free list is empty make up a new batch of objects
	 */
	if (list_empty(&pool->free_list)) {
		err = pool_alloc_segment(pool);
		if (unlikely(err)) {
			pool->count--;
			pthread_mutex_unlock(&pool->mutex);
			return err;
		}
	}

	l = pool->free_list.next;
	list_del(l);
	obj = list_entry(l, obj_t, obj_list);

	pthread_mutex_unlock(&pool->mutex);

	if (pool->parent)
		obj_ref(pool->parent);

	err = index_get(obj, &index);
	if (err)
		return err;
	obj->obj_handle	= ((uint64_t)(pool->type) << HANDLE_SHIFT) | index;

	ref_init(&obj->obj_ref);

        pthread_spin_init(&obj->obj_lock, PTHREAD_PROCESS_PRIVATE);

	/*
	 * if any type specific per allocation initialization do it
	 */
	if (pool->alloc) {
		err = pool->alloc(obj);
		if (err) {
			WARN();
			obj_release(&obj->obj_ref);
			return PTL_FAIL;
		}
	}

	obj->obj_free = 0;
	*p_obj = obj;

        return PTL_OK;
}

/*
 * obj_get
 *	return an object from handle and optionally type
 */
int obj_get(unsigned int type, ptl_handle_any_t handle, obj_t **obj_p)
{
	int err;
	obj_t *obj = NULL;
	unsigned int index;
#ifndef NO_ARG_VALIDATION
	unsigned int handle_type = (unsigned int)(handle >> HANDLE_SHIFT);
#endif

	if ((type == POOL_CT && handle == PTL_CT_NONE) ||
		(type == POOL_EQ && handle == PTL_EQ_NONE))
		goto done;

	if (handle == PTL_INVALID_HANDLE) {
		WARN();
		goto err1;
	}

#ifndef NO_ARG_VALIDATION
	if (type && handle_type && type != handle_type) {
		WARN();
		goto err1;
	}
#endif

	index = obj_handle_to_index(handle);

	err = index_lookup(index, &obj);
	if (err) {
		WARN();
		goto err1;
	}

#ifndef NO_ARG_VALIDATION
	if (obj->obj_free) {
		WARN();
		goto err1;
	}

	if (obj_handle_to_index(obj->obj_handle) != index) {
		WARN();
		goto err1;
	}

	if (type && (type != obj->obj_pool->type)) {
		WARN();
		goto err1;
	}
#endif

	obj_ref(obj);

done:
	*obj_p = obj;
	return PTL_OK;

err1:
	return PTL_ARG_INVALID;
}
