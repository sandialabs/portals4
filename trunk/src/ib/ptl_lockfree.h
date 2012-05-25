/**
 * @file ptl_obj.c
 */

#ifndef PTL_LOCKFREE_H
#define PTL_LOCKFREE_H

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
		void *obj;				/* the object to be linked */
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
                          "=a" (ret.obj),
                          "=d" (ret.counter)
                          : "a"  (oldval.obj),
                          "d"  (oldval.counter),
                          "b"  (newval.obj),
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
static inline void *dequeue_free_obj_alien(union counted_ptr *free_list, void *vaddr, void *alien_vaddr)
{
	union counted_ptr oldv, newv, retv;

	retv.c16 = free_list->c16;

	do {
		oldv = retv;
		if (retv.obj != NULL) {
			/* first object field is the next ptr */
			newv.obj = *(void **)(retv.obj + (vaddr - alien_vaddr)); 
		} else {
			newv.obj = NULL;
		}
		newv.counter = oldv.counter + 1;

		retv.c16 = PtlInternalAtomicCas128(&free_list->c16, oldv, newv);
	} while (retv.c16 != oldv.c16);

	if (retv.obj)
		return retv.obj + (vaddr - alien_vaddr);
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
static inline void enqueue_free_obj_alien(union counted_ptr *free_list, void *obj, void *vaddr, void *alien_vaddr)
{
	union counted_ptr oldv, newv, tmpv;
	void *alien_obj = ((void *)obj) + (alien_vaddr - vaddr); /* vaddr of obj in alien process */

	tmpv.c16 = free_list->c16;

	do {
		oldv = tmpv;
		/* first field of obj is the next ptr */
		*(void **)obj = tmpv.obj; 
		newv.obj = alien_obj;
		newv.counter = oldv.counter + 1;
		tmpv.c16 = PtlInternalAtomicCas128(&free_list->c16, oldv, newv);
	} while (tmpv.c16 != oldv.c16);
}

static inline void *dequeue_free_obj(union counted_ptr *free_list)
{
	return dequeue_free_obj_alien(free_list, NULL, NULL);
}

static inline void enqueue_free_obj(union counted_ptr *free_list, void *obj)
{
	enqueue_free_obj_alien(free_list, obj, NULL, NULL);
}

#endif
