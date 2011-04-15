/*
 * ptl_pt.h
 */

#ifndef PTL_PT_H
#define PTL_PT_H

struct eq;

enum {
	PT_API_DISABLE	= 1,
	PT_AUTO_DISABLE = 1 << 1
};

typedef struct pt {
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
	pthread_spinlock_t	lock;
	pthread_spinlock_t	list_lock;
} pt_t;

#endif /* PTL_PT_H */
