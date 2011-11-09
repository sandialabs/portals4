/**
 * @file ptl_sync.h
 */

#ifndef PTL_SYNC_H
#define PTL_SYNC_H

typedef struct {
	volatile int val;
} atomic_t;

static inline void atomic_set(atomic_t *var, int val)
{
	var->val = val;
}

static inline int atomic_read(atomic_t *var)
{
	return var->val;
}

static inline int atomic_inc(atomic_t *var)
{
	return __sync_fetch_and_add(&var->val, 1);
}

static inline int atomic_add(atomic_t *var, int val)
{
	return __sync_fetch_and_add(&var->val, val);
}

static inline int atomic_dec(atomic_t *var)
{
	return __sync_sub_and_fetch(&var->val, 1);
}

static inline void *atomic_swap_ptr(void *volatile *addr, void *newval)
{
	void *oldval = *addr;
	void *tmp;

	while ((tmp = __sync_val_compare_and_swap(addr, oldval, newval)) !=
		oldval) {
		oldval = tmp;
	}

	return oldval;
}

#endif /* PTL_SYNC_H */
