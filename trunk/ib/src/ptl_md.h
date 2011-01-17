#ifndef PTL_MD_H
#define PTL_MD_H

struct eq;
struct ct;

extern obj_type_t *type_md;

typedef struct md {
	PTL_BASE_OBJ

	void			*start;
	unsigned int		num_iov;
	ptl_size_t		length;
	unsigned int		options;
	struct eq		*eq;
	struct ct		*ct;
	mr_t			*mr;
	mr_t			**mr_list;
	struct ibv_sge		*sge_list;

	struct list_head	list;
} md_t;

void md_release(void *arg);

static inline int md_alloc(ni_t *ni, md_t **md_p)
{
	return obj_alloc(type_md, (obj_t *)ni, (obj_t **)md_p);
}

static inline int md_get(ptl_handle_md_t handle, md_t **md_p)
{
	return obj_get(type_md, (ptl_handle_any_t) handle, (obj_t **)md_p);
}

static inline void md_ref(md_t *md)
{
	obj_ref((obj_t *)md);
}

static inline int md_put(md_t *md)
{
	return obj_put((obj_t *)md);
}

static inline ptl_handle_md_t md_to_handle(md_t *md)
{
        return (ptl_handle_md_t)md->obj_handle;
}

#endif /* PTL_MD_H */
