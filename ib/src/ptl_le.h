#ifndef PTL_LE_H
#define PTL_LE_H

struct ct;

#define TYPE_LE			(0)

/*
 * common struct members between LE and ME objects
 */
#define PTL_LE_OBJ				\
	int			type;		\
	struct list_head	list;		\
	ptl_pt_index_t		pt_index;	\
	pt_t			*pt;		\
	ptl_list_t		ptl_list;	\
	struct eq       *eq;		\
	const void			*user_ptr;	\
	void			*start;		\
	unsigned int		num_iov;	\
	ptl_size_t		length;		\
	struct ct		*ct;		\
	uint32_t		uid;		\
	unsigned int		options;	\
	mr_t			*sge_list_mr;	\
	struct ibv_sge		*sge_list;

typedef struct le {
	obj_t			obj;
	PTL_LE_OBJ
} le_t;

int le_init(void *arg, void *unused);
void le_cleanup(void *arg);

static inline int le_alloc(ni_t *ni, le_t **le_p)
{
	int err;
	obj_t *obj;

	err = obj_alloc(&ni->le_pool, &obj);
	if (err) {
		*le_p = NULL;
		return err;
	}

	*le_p = container_of(obj, le_t, obj);
	return PTL_OK;
}

static inline int to_le(ptl_handle_le_t handle, le_t **le_p)
{
	int err;
	obj_t *obj;

	err = to_obj(POOL_LE, (ptl_handle_any_t)handle, &obj);
	if (err) {
		*le_p = NULL;
		return err;
	}

	*le_p = container_of(obj, le_t, obj);
	return PTL_OK;
}

static inline void le_get(le_t *le)
{
	obj_get(&le->obj);
}

static inline int le_put(le_t *le)
{
	return obj_put(&le->obj);
}

static inline ptl_handle_le_t le_to_handle(le_t *le)
{
        return (ptl_handle_le_t)le->obj.obj_handle;
}

int le_append_check(int type, ni_t *ni, ptl_pt_index_t pt_index,
					const ptl_le_t *le_init, ptl_list_t ptl_list,
					ptl_search_op_t search_op,
					ptl_handle_le_t *le_handle);

int le_get_mr(ni_t *ni, const ptl_le_t *le_init, le_t *le);

int le_append_pt(ni_t *ni, le_t *le);

void le_unlink(le_t *le, int send_event);

#endif /* PTL_LE_H */
