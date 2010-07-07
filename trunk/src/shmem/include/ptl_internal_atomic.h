#ifndef PTL_INTERNAL_ATOMIC_H
#define PTL_INTERNAL_ATOMIC_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef SANDIA_NEEDS_INTEL_INTRIN
# ifdef HAVE_IA64INTRIN_H
#  include <ia64intrin.h>
# elif defined(HAVE_IA32INTRIN_H)
#  include <ia32intrin.h>
# endif
#endif

#if defined(SANDIA_BUILTIN_INCR)
# define PtlInternalAtomicInc( ADDR, INCVAL ) __sync_fetch_and_add(ADDR, INCVAL)
#else
# define PtlInternalAtomicInc( ADDR, INCVAL ) \
    PtlInternalAtomicIncXX((volatile void*)(ADDR), (long int)(INCVAL), sizeof(*(ADDR)))

#error Need to implement my own atomics (suggest stealing from qthreads)
static inline unsigned long PtlInternalAtomicIncXX(
    volatile void *addr,
    const long int incr,
    const size_t length)
{
    switch (length) {
	case 4:
	    return PtlInternalAtomicInc32((volatile uint32_t *)addr, incr);
	case 8:
	    return PtlInternalAtomicInc64((volatile uint64_t *)addr, incr);
	default:
	    *(int *)(0) = 0;
    }
    return 0;
}
#endif

#endif
