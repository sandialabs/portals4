/**
 * @file ptl_sync.h
 */

#ifndef PTL_SYNC_H
#define PTL_SYNC_H

typedef struct {
	volatile int val __attribute__ ((aligned (8)));
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

static inline int atomic_sub(atomic_t *var, int val)
{
	return __sync_sub_and_fetch(&var->val, val);
}

static inline int atomic_swap(atomic_t *var, int newval)
{
	int oldval = var->val;
	int tmp;

	while ((tmp = __sync_val_compare_and_swap(&var->val, oldval, newval)) != oldval) {
		oldval = tmp;
	}

	return oldval;
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

static inline void SPINLOCK_BODY(void)
{
	__asm__ __volatile__ ("pause" ::: "memory");
}

/* branch prediction hints for compiler */
#define unlikely(x)	__builtin_expect((x),0)
#define likely(x)	__builtin_expect((x),1)

#endif /* PTL_SYNC_H */
