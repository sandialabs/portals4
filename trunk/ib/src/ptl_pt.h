/*
 * ptl_pt.h
 */

#ifndef PTL_PT_H
#define PTL_PT_H

struct eq;

extern obj_type_t *type_pt;

enum {
	PT_API_DISABLE	= 1,
	PT_AUTO_DISABLE = 1 << 1
};

/* we are trying to make pt look like the other objects
 * but it is not exactly the same since it is just a table
 * entry in the ni while the other objects are allocated
 * and freed so don't get carried away and call pt_get for
 * example */
typedef struct pt {
	PTL_BASE_OBJ

	unsigned int		options;
	int			in_use;
	int			enabled;
	int			disable;
	int			num_xt_active;
	struct eq		*eq;
	unsigned int		priority_size;
	struct list_head	priority_list;
	unsigned int		overflow_size;
	struct list_head	overflow_list;
	pthread_spinlock_t	list_lock;
} pt_t;

#endif /* PTL_PT_H */
