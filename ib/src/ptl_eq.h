#ifndef PTL_EQ_H
#define PTL_EQ_H

typedef struct eqe {
	unsigned int		generation;
	ptl_event_t		event;
} eqe_t;

typedef struct eq {
	obj_t			obj;
	eqe_t			*eqe_list;
	unsigned int		count;
	unsigned int		producer;
	unsigned int		consumer;
	unsigned int		prod_gen;
	unsigned int		cons_gen;
	int			interrupt;
	int			overflow;
} eq_t;

void eq_release(void *arg);

static inline int eq_alloc(ni_t *ni, eq_t **eq_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->eq_pool, &obj);
	if (err) {
		*eq_p = NULL;
		return err;
	}

	*eq_p = container_of(obj, eq_t, obj);
	return PTL_OK;
}

static inline int to_eq(ptl_handle_eq_t eq_handle, eq_t **eq_p)
{
	int err;
	obj_t *obj;

	err = to_obj(POOL_EQ, (ptl_handle_any_t)eq_handle, &obj);
	if (err) {
		*eq_p = NULL;
		return err;
	}

	*eq_p = container_of(obj, eq_t, obj);
	return PTL_OK;
}

static inline void eq_ref(eq_t *eq)
{
	obj_ref(&eq->obj);
}

static inline int eq_put(eq_t *eq)
{
	return obj_put(&eq->obj);
}

static inline ptl_handle_eq_t eq_to_handle(eq_t *eq)
{
        return (ptl_handle_eq_t)eq->obj.obj_handle;
}

void make_init_event(xi_t *xi, eq_t *eq, ptl_event_kind_t type, void *start);

void make_target_event(xt_t *xt, eq_t *eq, ptl_event_kind_t type, void *user_ptr, void *start);

void make_le_event(le_t *le, eq_t *eq, ptl_event_kind_t type, ptl_ni_fail_t fail_type);

#endif /* PTL_EQ_H */
