#ifndef PTL_MR_H
#define PTL_MR_H

typedef ptl_handle_any_t ptl_handle_mr_t;

typedef struct mr {
	obj_t			obj;
	struct ibv_mr		*ibmr;
	struct list_head	list;
} mr_t;

void mr_release(void *arg);

static inline int mr_alloc(ni_t *ni, mr_t **mr_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->mr_pool, &obj);
	if (err) {
		*mr_p = NULL;
		return err;
	}

	*mr_p = container_of(obj, mr_t, obj);
	return PTL_OK;
}

static inline int mr_get(ptl_handle_mr_t handle, mr_t **mr_p)
{
	int err;
	obj_t *obj;

	err = obj_get(OBJ_TYPE_MR, (ptl_handle_any_t)handle, &obj);
	if (err) {
		*mr_p = NULL;
		return err;
	}

	*mr_p = container_of(obj, mr_t, obj);
	return PTL_OK;
}

static inline void mr_ref(mr_t *mr)
{
	obj_ref(&mr->obj);
}

static inline int mr_put(mr_t *mr)
{
	return obj_put(&mr->obj);
}

static inline ptl_handle_mr_t mr_to_handle(mr_t *mr)
{
        return (ptl_handle_mr_t)mr->obj.obj_handle;
}

int mr_lookup(ni_t *ni, void *start, ptl_size_t length, mr_t **mr);

#endif /* PTL_MR_H */
