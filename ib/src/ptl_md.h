#ifndef PTL_MD_H
#define PTL_MD_H

struct eq;
struct ct;

typedef struct md {
	obj_t			obj;
	void			*start;
	unsigned int		num_iov;
	ptl_size_t		length;
	unsigned int		options;
	struct eq		*eq;
	struct ct		*ct;

	mr_t *sge_list_mr;
	struct ibv_sge		*sge_list;

	struct list_head	list;
} md_t;

void md_cleanup(void *arg);

static inline int md_alloc(ni_t *ni, md_t **md_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->md_pool, &obj);
	if (err) {
		*md_p = NULL;
		return err;
	}

	*md_p = container_of(obj, md_t, obj);
	return PTL_OK;
}

static inline int to_md(ptl_handle_md_t handle, md_t **md_p)
{
	int err;
	obj_t *obj;

	err = to_obj(POOL_MD, (ptl_handle_any_t)handle, &obj);
	if (err) {
		*md_p = NULL;
		return err;
	}

	*md_p = container_of(obj, md_t, obj);
	return PTL_OK;
}

static inline void md_ref(md_t *md)
{
	obj_ref(&md->obj);
}

static inline int md_put(md_t *md)
{
	return obj_put(&md->obj);
}

static inline ptl_handle_md_t md_to_handle(md_t *md)
{
        return (ptl_handle_md_t)md->obj.obj_handle;
}

#endif /* PTL_MD_H */
