#ifndef PTL_MR_H
#define PTL_MR_H

typedef ptl_handle_any_t ptl_handle_mr_t;

extern obj_type_t *type_mr;

typedef struct mr {
	PTL_BASE_OBJ
	struct ibv_mr		*ibmr;
	struct list_head	list;
} mr_t;

void mr_release(void *arg);

static inline int mr_alloc(ni_t *ni, mr_t **mr_p)
{
	return obj_alloc(type_mr, (obj_t *)ni, (obj_t **)mr_p);
}

static inline int mr_get(ptl_handle_mr_t handle, mr_t **mr_p)
{
	return obj_get(type_mr, (ptl_handle_any_t) handle, (obj_t **)mr_p);
}

static inline void mr_ref(mr_t *mr)
{
	obj_ref((obj_t *)mr);
}

static inline int mr_put(mr_t *mr)
{
	return obj_put((obj_t *)mr);
}

static inline ptl_handle_mr_t mr_to_handle(mr_t *mr)
{
        return (ptl_handle_mr_t)mr->obj_handle;
}

int mr_lookup(ni_t *ni, void *start, ptl_size_t length, mr_t **mr);

#endif /* PTL_MR_H */
