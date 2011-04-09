/*
 * ptl_obj.c -- object management
 */

#include "ptl_loc.h"

static unsigned int linesize;

/*
 * type_info
 *	list of object type specific info
 */
obj_type_t type_info[] = {
	[OBJ_TYPE_NONE] = {
		.name	= "obj",
		.size	= sizeof(obj_t),
		.type	= OBJ_TYPE_NONE,
	},

	[OBJ_TYPE_NI] = {
		.name	= "ni",
		.size	= sizeof(ni_t),
		.type	= OBJ_TYPE_NI,
	},

	[OBJ_TYPE_PT] = {
		.name	= "pt",
		.size	= sizeof(pt_t),
		.type	= OBJ_TYPE_PT,
	},

	[OBJ_TYPE_MR] = {
		.name	= "mr",
		.size	= sizeof(mr_t),
		.type	= OBJ_TYPE_MR,
		.fini	= mr_release,
	},

	[OBJ_TYPE_LE] = {
		.name	= "le",
		.size	= sizeof(le_t),
		.type	= OBJ_TYPE_LE,
		.init	= le_init,
		.fini	= le_release,
	},

	[OBJ_TYPE_ME] = {
		.name	= "me",
		.size	= sizeof(me_t),
		.type	= OBJ_TYPE_ME,
		.init	= me_init,
		.fini	= me_release,
	},

	[OBJ_TYPE_MD] = {
		.name	= "md",
		.size	= sizeof(md_t),
		.type	= OBJ_TYPE_MD,
		.fini	= md_release,
	},

	[OBJ_TYPE_EQ] = {
		.name	= "eq",
		.size	= sizeof(eq_t),
		.type	= OBJ_TYPE_EQ,
		.fini	= eq_release,
	},

	[OBJ_TYPE_CT] = {
		.name	= "ct",
		.size	= sizeof(ct_t),
		.type	= OBJ_TYPE_CT,
		.fini	= ct_release,
	},

	[OBJ_TYPE_XI] = {
		.name	= "xi",
		.size	= sizeof(xi_t),
		.type	= OBJ_TYPE_XI,
		.init	= xi_init,
		.fini	= xi_release,
	},

	[OBJ_TYPE_XT] = {
		.name	= "xt",
		.size	= sizeof(xt_t),
		.type	= OBJ_TYPE_XT,
		.init	= xt_init,
		.fini	= xt_release,
	},

	[OBJ_TYPE_BUF] = {
		.name	= "buf",
		.size	= sizeof(buf_t),
		.type	= OBJ_TYPE_BUF,
		.init	= buf_init,
		.fini	= buf_release,
	},
};

obj_type_t *type_ni = &type_info[OBJ_TYPE_NI];
obj_type_t *type_pt = &type_info[OBJ_TYPE_PT];
obj_type_t *type_mr = &type_info[OBJ_TYPE_MR];
obj_type_t *type_le = &type_info[OBJ_TYPE_LE];
obj_type_t *type_me = &type_info[OBJ_TYPE_ME];
obj_type_t *type_md = &type_info[OBJ_TYPE_MD];
obj_type_t *type_eq = &type_info[OBJ_TYPE_EQ];
obj_type_t *type_ct = &type_info[OBJ_TYPE_CT];
obj_type_t *type_xi = &type_info[OBJ_TYPE_XI];
obj_type_t *type_xt = &type_info[OBJ_TYPE_XT];
obj_type_t *type_buf = &type_info[OBJ_TYPE_BUF];

void _dump_type_counts()
{
	int i;

	for (i = 0; i < OBJ_TYPE_LAST; i++)
		printf("%s:%d ", type_info[i].name, type_info[i].count);
	printf("\n");
}

void *obj_get_zero_filled_page(void)
{
	int err;
	void *p;

	err = posix_memalign(&p, pagesize, pagesize);
	if (unlikely(err))
		return NULL;

	get_maps();
	memset(p, 0, pagesize);

	return p;
}

int obj_init(void)
{
	int err;
	int i;
	obj_type_t *type;

	err = index_init();
	if (err)
		return err;

	pagesize = sysconf(_SC_PAGESIZE);
	linesize = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

	for (i = 0; i < OBJ_TYPE_LAST; i++) {
		type = &type_info[i];

		INIT_LIST_HEAD(&type->free_list);
		INIT_LIST_HEAD(&type->chunk_list);
		pthread_spin_init(&type->free_list_lock,
			PTHREAD_PROCESS_PRIVATE);

		type->round_size = (type->size + linesize - 1)
				   & ~(linesize - 1);
		type->obj_per_page = pagesize/type->round_size;
		if (!type->obj_per_page) {
			ptl_error("Well that's embarassing but "
			          "can't fit any %s's in a page\n",
			          type->name);
			goto err;
		}
		type->count = 0;
	}

	return PTL_OK;

err:
	return PTL_FAIL;
}

void obj_fini(void)
{
	int i;
	int j;
	obj_type_t *type;
	struct list_head *l, *t;
	pagelist_t *chunk;

	for (i = 0; i < OBJ_TYPE_LAST; i++) {
		type = &type_info[i];

		if (type->count) {
			/*
			 * we shouldn't get here since we
			 * are supposed to clean up all the
			 * objects during processing PtlFini
			 * so this would be a library bug
			 */
			ptl_warn("leaked %d %s objects\n",
				 type->count, type->name);
			ptl_test_return = PTL_FAIL;
		}

		pthread_spin_destroy(&type->free_list_lock);

		list_for_each_safe(l, t, &type->chunk_list) {
			list_del(l);
			chunk = list_entry(l, pagelist_t, chunk_list);

			for (j = 0; j < chunk->num_pages; j++)
				free(chunk->page_list[j]);

			free(chunk);
		}
	}

	index_fini();
}

/*
 * type_get_pagelist_pointer
 *	get pointer to pagelist with room to hold
 *	a new page pointer
 *	caller should hold type->free_list_lock
 */
static int type_get_pagelist_pointer(obj_type_t *type, pagelist_t **pp)
{
	struct list_head *l = type->chunk_list.next;
	pagelist_t *p = list_entry(l, pagelist_t, chunk_list);

	if (likely(!list_empty(&type->chunk_list)
	    && (p->num_pages < p->max_pages))) {
		*pp = p;
		return PTL_OK;
	}

	p = obj_get_zero_filled_page();
	if (unlikely(!p))
		return PTL_NO_SPACE;

	p->num_pages = 0;
	p->max_pages = (pagesize - sizeof(*p))/sizeof(void *);
	list_add(&p->chunk_list, &type->chunk_list);

	*pp = p;
	return PTL_OK;
}

/*
 * type_alloc_page
 *	allocate a new page of objects of given type
 *	caller should hold type->free_list_lock
 */
static int type_alloc_page(obj_type_t *type)
{
	int err;
	pagelist_t *pp = NULL;
	uint8_t *p;
	int i;
	obj_t *obj;
	unsigned int index = 0;

	err = type_get_pagelist_pointer(type, &pp);
	if (unlikely(err))
		return err;

	p = obj_get_zero_filled_page();
	if (unlikely(!p))
		return PTL_NO_SPACE;

	pp->page_list[pp->num_pages++] = p;

	for (i = 0; i < type->obj_per_page; i++) {
		obj = (obj_t *)p;
		obj->obj_free = 1;
		obj->obj_type = type;
		err = index_get(obj, &index);
		if (err)
			return err;
		obj->obj_handle	= ((uint64_t)(type->type) << 56) | index;
		ref_set(&obj->obj_ref, 0);
		list_add(&obj->obj_list, &type->free_list);
		p += type->round_size;
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
		ptl_fatal("object corrupted unable to free_index\n");

	if (type->fini)
		type->fini(obj);

	if (obj->obj_parent)
		obj_put(obj->obj_parent);
	obj->obj_parent = NULL;
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
int obj_alloc(obj_type_t *type, obj_t *parent, obj_t **p_obj)
{
	int err;
        obj_t *obj;
	struct list_head *l;

	pthread_spin_lock(&type->free_list_lock);

	if (list_empty(&type->free_list)) {
		err = type_alloc_page(type);
		if (unlikely(err)) {
			pthread_spin_unlock(&type->free_list_lock);
			return err;
		}
	}

	l = type->free_list.next;
	list_del(l);
	obj = list_entry(l, obj_t, obj_list);
	type->count++;

	pthread_spin_unlock(&type->free_list_lock);

	if (parent)
		obj_ref(parent);
	obj->obj_parent = parent;

	obj->obj_ni = (type == type_ni) ? (ni_t *)obj : parent->obj_ni;

	ref_init(&obj->obj_ref);

        pthread_spin_init(&obj->obj_lock, PTHREAD_PROCESS_PRIVATE);

	/*
	 * for now we are zeroing this out
	 * to avoid strange scribbles
	 * eventually should consider making the
	 * types responsible for this since they
	 * fill in most of the space anyway
	 */
	memset((uint8_t *)obj + sizeof(obj_t), 0, type->size - sizeof(obj_t));

	if (type->init) {
		if (type->init(obj)) {
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
int obj_get(obj_type_t *type, ptl_handle_any_t handle, obj_t **obj_p)
{
	int err;
	obj_t *obj = NULL;
	unsigned int handle_type = (unsigned int)(handle >> 56);
	unsigned int index;
	unsigned int type_num = type ? type->type : 0;

	if (handle == PTL_HANDLE_NONE)
		goto done;

	if (handle == PTL_INVALID_HANDLE)
		goto err1;

	if (type_num && handle_type && type_num != handle_type)
		goto err1;

	index = obj_handle_to_index(handle);

	err = index_lookup(index, &obj);
	if (err)
		goto err1;

	if (obj->obj_free)
		goto err1;

	if (obj_handle_to_index(obj->obj_handle) != index)
		goto err1;

	if (type_num && (type_num != obj->obj_type->type))
		goto err1;

	obj_ref(obj);

done:
	*obj_p = obj;
	return PTL_OK;

err1:
	return PTL_ARG_INVALID;
}
