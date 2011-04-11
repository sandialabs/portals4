/*
 * ptl_obj.c -- object management
 */

#include "ptl_loc.h"

void *obj_get_zero_filled_segment(obj_type_t *type)
{
	int err;
	void *p;

	err = posix_memalign(&p, pagesize, type->segment_size);
	if (unlikely(err))
		return NULL;

	get_maps();
	memset(p, 0, type->segment_size);

	return p;
}

int obj_type_init(obj_type_t *pool, char *name, int size,
		  int type, obj_t *parent)
{
	pool->name = name;
	pool->size = size;
	pool->type = type;
	pool->parent = parent;

	INIT_LIST_HEAD(&pool->free_list);
	INIT_LIST_HEAD(&pool->chunk_list);
	pthread_spin_init(&pool->free_list_lock, PTHREAD_PROCESS_PRIVATE);

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

	return PTL_OK;
}

void obj_type_fini(obj_type_t *pool)
{
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
	 * if type has a cleanup routine call it
	 */
	if (pool->fini) {
		pthread_spin_lock(&pool->free_list_lock);
		list_for_each(l, &pool->free_list) {
			obj = list_entry(l, obj_t, obj_list);
			pool->fini(obj);
		}
		pthread_spin_unlock(&pool->free_list_lock);
	}

	pthread_spin_destroy(&pool->free_list_lock);

	list_for_each_safe(l, t, &pool->chunk_list) {
		list_del(l);
		chunk = list_entry(l, segment_list_t, chunk_list);

		for (i = 0; i < chunk->num_segments; i++) {
			free(chunk->segment_list[i].addr);

			/* hack see below TODO cleanup later */
			if (chunk->segment_list[i].priv) {
				struct ibv_mr *mr = chunk->segment_list[i].priv;

				ibv_dereg_mr(mr);
			}
		}

		free(chunk);
	}
}

/*
 * type_get_segment_list_pointer
 *	get pointer to segment_list with room to hold
 *	a new segment pointer
 *	caller should hold type->free_list_lock
 *	note that we never free object memory so there are
 *	never any holes in a segment list
 */
static int type_get_segment_list_pointer(obj_type_t *type, segment_list_t **pp)
{
	int err;
	struct list_head *l = type->chunk_list.next;
	segment_list_t *p = list_entry(l, segment_list_t, chunk_list);
	void *d;

	if (likely(!list_empty(&type->chunk_list)
	    && (p->num_segments < p->max_segments))) {
		*pp = p;
		return PTL_OK;
	}

	/* have to allocate a new segment list */
	err = posix_memalign(&d, pagesize, pagesize);
	if (unlikely(err))
		return PTL_NO_SPACE;

	p = d;

	get_maps();
	memset(p, 0, pagesize);

	p->num_segments = 0;
	p->max_segments = (type->segment_size - sizeof(*p))/sizeof(void *);
	list_add(&p->chunk_list, &type->chunk_list);

	*pp = p;
	return PTL_OK;
}

/*
 * type_alloc_segment
 *	allocate a new segment of objects for a given pool
 *	caller should hold pool->free_list_lock
 */
static int type_alloc_segment(obj_type_t *pool)
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
	if (pool->type == OBJ_TYPE_BUF) {
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
		obj->obj_type = pool;
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
	obj_type_t *type = obj->obj_type;
	unsigned int index = obj_handle_to_index(obj->obj_handle);

	err = index_free(index);
	if (err)
		WARN();

	obj->obj_handle = 0;

	if (type->free)
		type->free(obj);

	if (obj->obj_parent)
		obj_put(obj->obj_parent);
	obj->obj_free = 1;

	pthread_spin_lock(&type->free_list_lock);
	list_add_tail(l, &type->free_list);
	type->count--;
	pthread_spin_unlock(&type->free_list_lock);
}

/*
 * obj_alloc
 *	allocate a new object of indicated type
 *	zero fill after the base type
 */
int obj_alloc(obj_type_t *pool, obj_t **p_obj)
{
	int err;
        obj_t *obj;
	struct list_head *l;
	unsigned int index = 0;

	pthread_spin_lock(&pool->free_list_lock);

	if (list_empty(&pool->free_list)) {
		err = type_alloc_segment(pool);
		if (unlikely(err)) {
			pthread_spin_unlock(&pool->free_list_lock);
			return err;
		}
	}

	l = pool->free_list.next;
	list_del(l);
	obj = list_entry(l, obj_t, obj_list);
	pool->count++;

	pthread_spin_unlock(&pool->free_list_lock);

	if (pool->parent)
		obj_ref(pool->parent);

	err = index_get(obj, &index);
	if (err)
		return err;
	obj->obj_handle	= ((uint64_t)(pool->type) << 56) | index;

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
	unsigned int handle_type = (unsigned int)(handle >> 56);
	unsigned int index;

	if (handle == PTL_HANDLE_NONE)
		goto done;

	if (handle == PTL_INVALID_HANDLE) {
		WARN();
		goto err1;
	}

	if (type && handle_type && type != handle_type) {
		WARN();
		goto err1;
	}

	index = obj_handle_to_index(handle);

	err = index_lookup(index, &obj);
	if (err) {
		WARN();
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

	if (type && (type != obj->obj_type->type)) {
		WARN();
		goto err1;
	}

	obj_ref(obj);

done:
	*obj_p = obj;
	return PTL_OK;

err1:
	return PTL_ARG_INVALID;
}
