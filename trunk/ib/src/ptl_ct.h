#ifndef PTL_CT_H
#define PTL_CT_H

extern obj_type_t *type_ct;

typedef struct ct {
	PTL_BASE_OBJ

	ptl_ct_event_t		event;
	struct list_head	xi_list;
	struct list_head        list;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	int			waiting;
	int			interrupt;
} ct_t;

void ct_release(void *arg);

static inline int ct_alloc(ni_t *ni, ct_t **ct_p)
{
	return obj_alloc(type_ct, (obj_t *)ni, (obj_t **)ct_p);
}

static inline int ct_get(ptl_handle_ct_t ct_handle, ct_t **ct_p)
{
	return obj_get(type_ct, (ptl_handle_any_t)ct_handle, (obj_t **)ct_p);
}

static inline void ct_ref(ct_t *ct)
{
	obj_ref((obj_t *)ct);
}

static inline int ct_put(ct_t *ct)
{
	return obj_put((obj_t *)ct);
}

static inline ptl_handle_ct_t ct_to_handle(ct_t *ct)
{
        return (ptl_handle_ct_t)ct->obj_handle;
}

void post_ct(xi_t *xi, ct_t *ct);

void make_ct_event(ct_t *ct, ptl_ni_fail_t ni_fail,
		    ptl_size_t length, int bytes);

#endif /* PTL_CT_H */
