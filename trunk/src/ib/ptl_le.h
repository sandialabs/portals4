/**
 * @file ptl_le.h
 *
 * @brief Header for ptl_le.c.
 */
#ifndef PTL_LE_H
#define PTL_LE_H

/* forward declarations */
struct pt;
struct eq;
struct ct;
struct mr;

/**
 * @brief Used to distinguish between LE and ME objects.
 */
enum list_element_type {
	TYPE_LE,
	TYPE_ME,
};

#if IS_PPE
#define LE_XPMEM_MAPPING											\
	struct {														\
	/* The client was overriden, but we still need it. */			\
		union {														\
			void *client_start;										\
			ptl_iovec_t *client_iovecs;								\
		};															\
	} ppe
#else
#define LE_XPMEM_MAPPING
#endif

/**
 * @brief Common struct members for LE and ME objects.
 */
#define PTL_LE_OBJ				\
	struct list_head	list;		\
	struct pt		*pt;		\
	struct eq		*eq;		\
	struct ct		*ct;		\
	void			*user_ptr;	\
	void			*start;		\
	ptl_size_t		length;		\
	ptl_pt_index_t		pt_index;	\
	ptl_list_t		ptl_list;	\
	ptl_uid_t		uid;		\
	unsigned int		type;		\
	unsigned int		options;	\
	unsigned int		num_iov;	\
	unsigned int		do_auto_free; \
	LE_XPMEM_MAPPING;

/**
 * @brief Portals list element.
 */
struct le {
	obj_t			obj;
	PTL_LE_OBJ
};

typedef struct le le_t;

int le_init(void *arg, void *unused);

void le_cleanup(void *arg);

int le_get_mr(ni_t * restrict ni, const ptl_le_t *le_init, le_t *le);

int le_append_pt(ni_t *ni, le_t *le);

void le_unlink(le_t *le, int send_event);

int le_append_check(int type, ni_t *ni, ptl_pt_index_t pt_index,
		    const ptl_le_t *le_init, ptl_list_t ptl_list,
		    ptl_search_op_t search_op,
		    ptl_handle_le_t *le_handle);

int __check_overflow(le_t *le, int delete);

int check_overflow_search_only(le_t *le);

int check_overflow_search_delete(le_t *le);

void flush_le_references(le_t *le);

/**
 * @brief Allocate a new list element.
 *
 * @param[in] ni The network interface for which to allocate LE.
 * @param[out] le_p The address of the returned LE.
 *
 * @return status
 */
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

/**
 * @brief Convert a list element handle to a list element object.
 *
 * @param[in] handle The list element handle to convert.
 * @param[out] le_p The address of the returned LE object.
 *
 * @return status
 */
static inline int to_le(ptl_handle_le_t handle, le_t **le_p)
{
	obj_t *obj;

	obj = to_obj(POOL_LE, (ptl_handle_any_t)handle);
	if (!obj) {
		*le_p = NULL;
		return PTL_ARG_INVALID;
	}

	*le_p = container_of(obj, le_t, obj);
	return PTL_OK;
}

/**
 * @brief Return the ref count on an LE.
 *
 * @param[in] le the LE object.
 *
 * @return the ref count.
 */
static inline int le_ref_cnt(le_t *le)
{
	return obj_ref_cnt(&le->obj);
}

/**
 * @brief Take a reference to a list element.
 *
 * @param[in] le the LE object to reference.
 */
static inline void le_get(le_t *le)
{
	obj_get(&le->obj);
}

/**
 * @brief Drop a reference to a list element.
 *
 * If the last reference is dropped free the object.
 *
 * @param[in] le the LE object to dereference.
 *
 * @return status
 */
static inline int le_put(le_t *le)
{
	return obj_put(&le->obj);
}

/**
 * @brief convert an LE object to its handle.
 *
 * @param[in] le the LE object to convert.
 *
 * @return The LE handle.
 */
static inline ptl_handle_le_t le_to_handle(le_t *le)
{
        return (ptl_handle_le_t)le->obj.obj_handle;
}

#endif /* PTL_LE_H */
