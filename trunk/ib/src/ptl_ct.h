#ifndef PTL_CT_H
#define PTL_CT_H

typedef struct ct {
	obj_t			obj;
	ptl_ct_event_t		event;
	struct list_head	xi_list;
	struct list_head        list;
	int			interrupt;
} ct_t;

void ct_release(void *arg);

static inline int ct_alloc(ni_t *ni, ct_t **ct_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->ct_pool, &obj);
	if (err) {
		*ct_p = NULL;
		return err;
	}

	*ct_p = container_of(obj, ct_t, obj);
	return PTL_OK;
}

static inline int ct_get(ptl_handle_ct_t ct_handle, ct_t **ct_p)
{
	int err;
	obj_t *obj;

	err = obj_get(OBJ_TYPE_CT, (ptl_handle_any_t)ct_handle, &obj);
	if (err) {
		*ct_p = NULL;
		return err;
	}

	*ct_p = container_of(obj, ct_t, obj);
	return PTL_OK;
}

static inline void ct_ref(ct_t *ct)
{
	obj_ref(&ct->obj);
}

static inline int ct_put(ct_t *ct)
{
	return obj_put(&ct->obj);
}

static inline ptl_handle_ct_t ct_to_handle(ct_t *ct)
{
        return (ptl_handle_ct_t)ct->obj.obj_handle;
}

void post_ct(xi_t *xi, ct_t *ct);

void make_ct_event(ct_t *ct, ptl_ni_fail_t ni_fail,
		   ptl_size_t length, int bytes);

#endif /* PTL_CT_H */
