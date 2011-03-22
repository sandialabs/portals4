#ifndef PTL_ME_H
#define PTL_ME_H

struct ct;

extern obj_type_t *type_me;

#define TYPE_ME			(1)

typedef struct me {
	PTL_BASE_OBJ
	PTL_LE_OBJ

	ptl_size_t		offset;
	ptl_size_t		min_free;
	uint64_t		match_bits;
	uint64_t		ignore_bits;
	ptl_process_t   id;
} me_t;

int me_init(void *arg);
void me_release(void *arg);

static inline int me_alloc(ni_t *ni, me_t **me_p)
{
	return obj_alloc(type_me, (obj_t *)ni, (obj_t **)me_p);
}

static inline int me_get(ptl_handle_me_t handle, me_t **me_p)
{
	return obj_get(type_me, (ptl_handle_any_t) handle, (obj_t **)me_p);
}

static inline void me_ref(me_t *me)
{
	obj_ref((obj_t *)me);
}

static inline int me_put(me_t *me)
{
	return obj_put((obj_t *)me);
}

static inline ptl_handle_me_t me_to_handle(me_t *me)
{
        return (ptl_handle_me_t)me->obj_handle;
}

void me_unlink(me_t *me);

#endif /* PTL_ME_H */
