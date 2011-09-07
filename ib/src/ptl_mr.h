#ifndef PTL_MR_H
#define PTL_MR_H

typedef ptl_handle_any_t ptl_handle_mr_t;

typedef struct mr {
	obj_t			obj;
	struct ibv_mr		*ibmr;
	uint64_t knem_cookie;
	RB_ENTRY(mr) entry;
} mr_t;

void mr_cleanup(void *arg);

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

static inline int to_mr(ptl_handle_mr_t handle, mr_t **mr_p)
{
	int err;
	obj_t *obj;

	err = to_obj(POOL_MR, (ptl_handle_any_t)handle, &obj);
	if (err) {
		*mr_p = NULL;
		return err;
	}

	*mr_p = container_of(obj, mr_t, obj);
	return PTL_OK;
}

static inline void mr_get(mr_t *mr)
{
	obj_get(&mr->obj);
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
void cleanup_mr_tree(ni_t *ni);

#endif /* PTL_MR_H */
