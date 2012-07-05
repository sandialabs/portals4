/**
 * @file ptl_obj.c
 */

#ifndef PTL_LOCKFREE_H
#define PTL_LOCKFREE_H

#if SANDIA_BUILTIN_CAS128 || HAVE_CMPXCHG16B
/*
 * Lock free linked list. Enqueue/dequeue at the head (FIFO). Can be
 * used by multiple enqueuers/dequeuers.
 *
 * Important: the first pointer in the object linked is the next field.
 *
 * There is regular version where all the enqueuers reside in the same
 * process.

 * The alien version allows the queue to reside in the
 * address space of a process, and some enqueuers/dequeuers to be in
 * other processes; the memory and the objects must be shared.
 */

/* Since we use a lock free linked list, we can (and did) fall victim
 * of the ABA problem
 * (https://en.wikipedia.org/wiki/ABA_problem). Work around by using a
 * counter for the pointer. That implies that the platform can support
 * 16 bytes compare and swap. Every time the pointer must be changed,
 * a 64 bits counter is incremented. */
typedef unsigned int ptlinternal_uint128_t __attribute__((mode(TI)));
union counted_ptr {
	struct {
		void *head;
		unsigned long counter;
	};
	unsigned int __attribute__((mode(TI))) c16; /* 16 bytes */
};

static inline ptlinternal_uint128_t PtlInternalAtomicCas128(ptlinternal_uint128_t *addr,
															const union counted_ptr oldval,
															const union counted_ptr newval)
{
    union counted_ptr ret;

    assert(((uintptr_t)addr & 0xf) == 0);

#if defined SANDIA_BUILTIN_CAS128
	ret.c16 = __sync_val_compare_and_swap(addr, oldval.c16, newval.c16);

#elif defined HAVE_CMPXCHG16B
    __asm__ __volatile__ ("lock cmpxchg16b %0\n\t"
                          : "+m" (*addr),
                          "=a" (ret.head),
                          "=d" (ret.counter)
                          : "a"  (oldval.head),
                          "d"  (oldval.counter),
                          "b"  (newval.head),
                          "c"  (newval.counter)
                          : "cc",
                          "memory");

#else  /* ifdef HAVE_CMPXCHG16B */
# error No known 128-bit atomic CAS operations are available
#endif  /* ifdef HAVE_CMPXCHG16B */

    return ret.c16;
}

/**
 * Return an object from pool freelist.
 *
 * @param pool the pool
 *
 * @return the object
 */
static inline void *ll_dequeue_obj_alien(union counted_ptr *free_list, void *vaddr, void *alien_vaddr)
{
	union counted_ptr oldv, newv, retv;

	retv.c16 = free_list->c16;

	do {
		oldv = retv;
		if (retv.head != NULL) {
			/* first object field is the next ptr */
			newv.head = *(void **)(retv.head + (vaddr - alien_vaddr)); 
		} else {
			newv.head = NULL;
		}
		newv.counter = oldv.counter + 1;

		retv.c16 = PtlInternalAtomicCas128(&free_list->c16, oldv, newv);
	} while (retv.c16 != oldv.c16);

	if (retv.head)
		return retv.head + (vaddr - alien_vaddr);
	else
		return NULL;
}

/**
 * Add an object to pool freelist.
 *
 * This algorithm is thanks to Kyle and does not
 * require a holding a lock.
 *
 * @param pool the pool
 * @param obj the object
 */
static inline void ll_enqueue_obj_alien(union counted_ptr *free_list, void *obj, void *vaddr, void *alien_vaddr)
{
	union counted_ptr oldv, newv, tmpv;
	void *alien_obj = ((void *)obj) + (alien_vaddr - vaddr); /* vaddr of obj in alien process */

	tmpv.c16 = free_list->c16;

	do {
		oldv = tmpv;
		/* first field of obj is the next ptr */
		*(void **)obj = tmpv.head; 
		newv.head = alien_obj;
		newv.counter = oldv.counter + 1;
		tmpv.c16 = PtlInternalAtomicCas128(&free_list->c16, oldv, newv);
	} while (tmpv.c16 != oldv.c16);
}

static inline void ll_init(union counted_ptr *free_list)
{
	free_list->head = NULL;
	free_list->counter = 0;
}

#else

#include "ptl_locks.h"

/* No cpmxchg128 available. Use a regular list head + shared spinlock. */
union counted_ptr {
	struct {
		void *head;
		PTL_FASTLOCK_TYPE lock;
	};
};

static inline void *ll_dequeue_obj_alien(union counted_ptr *free_list, void *vaddr, void *alien_vaddr)
{
	void *ret;

	PTL_FASTLOCK_LOCK(&free_list->lock);
	ret = free_list->head;
	if (ret)
		free_list->head = *(void **)(free_list->head + (vaddr - alien_vaddr));
	PTL_FASTLOCK_UNLOCK(&free_list->lock);

	if (ret)
		return ret + (vaddr - alien_vaddr);
	else
		return NULL;
}

static inline void ll_enqueue_obj_alien(union counted_ptr *free_list, void *obj, void *vaddr, void *alien_vaddr)
{
	void *alien_obj = ((void *)obj) + (alien_vaddr - vaddr); /* vaddr of obj in alien process */
	
	PTL_FASTLOCK_LOCK(&free_list->lock);

	/* first field of obj is the next ptr */
	*(void **)obj = free_list->head;
	free_list->head = alien_obj;

	PTL_FASTLOCK_UNLOCK(&free_list->lock);
}

static inline void ll_init(union counted_ptr *free_list)
{
	free_list->head = NULL;
	PTL_FASTLOCK_INIT_SHARED(&free_list->lock);
}

#endif

static inline void *ll_dequeue_obj(union counted_ptr *free_list)
{
	return ll_dequeue_obj_alien(free_list, NULL, NULL);
}

static inline void ll_enqueue_obj(union counted_ptr *free_list, void *obj)
{
	ll_enqueue_obj_alien(free_list, obj, NULL, NULL);
}

#endif
