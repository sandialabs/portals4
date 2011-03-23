#ifndef PTL_EQ_H
#define PTL_EQ_H

extern obj_type_t *type_eq;

typedef struct eqe {
	unsigned int		generation;
	ptl_event_t		event;
} eqe_t;

typedef struct eq {
	PTL_BASE_OBJ

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
	return obj_alloc(type_eq, (obj_t *)ni, (obj_t **)eq_p);
}

static inline int eq_get(ptl_handle_eq_t eq_handle, eq_t **eq_p)
{
	return obj_get(type_eq, (ptl_handle_any_t)eq_handle, (obj_t **)eq_p);
}

static inline void eq_ref(eq_t *eq)
{
	obj_ref((obj_t *)eq);
}

static inline int eq_put(eq_t *eq)
{
	return obj_put((obj_t *)eq);
}

static inline ptl_handle_eq_t eq_to_handle(eq_t *eq)
{
        return (ptl_handle_eq_t)eq->obj_handle;
}

int make_init_event(xi_t *xi, eq_t *eq, ptl_event_kind_t type, void *start);

int make_target_event(xt_t *xt, eq_t *eq, ptl_event_kind_t type, void *start);

#endif /* PTL_EQ_H */
