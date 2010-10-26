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

#if defined(SANDIA_BUILTIN_CAS)
# define PtlInternalAtomicCas32( ADDR, OLDVAL, NEWVAL ) \
    (uint32_t)__sync_val_compare_and_swap((ADDR), (OLDVAL), (NEWVAL))
# define PtlInternalAtomicCas64( ADDR, OLDVAL, NEWVAL ) \
    (uint64_t)__sync_val_compare_and_swap((ADDR), (OLDVAL), (NEWVAL))
# define PtlInternalAtomicCasPtr( ADDR, OLDVAL, NEWVAL ) \
    (void*)__sync_val_compare_and_swap((ADDR), (OLDVAL), (NEWVAL))
#else
#error Need to implement my own CAS (suggest stealing from qthreads)
static inline uint32_t PtlInternalAtomicCas32(
    volatile uint32_t * addr,
    uint32_t oldval,
    uint32_t newval)
{
}

static inline uint64_t PtlInternalAtomicCas64(
    volatile uint64_t * addr,
    uint64_t oldval,
    uint64_t newval)
{
}

static inline void *PtlInternalAtomicCasPtr(
    void *volatile *addr,
    void *oldval,
    void *newval)
{
#if (SIZEOF_VOIDP == 4)
    PtlInternalAtomicCas32((volatile uint32_t *)addr, (uint32_t) oldval,
                           (uint32_t) newval);
#elif (SIZEOF_VOIDP == 8)
    PtlInternalAtomicCas64((volatile uint64_t *)addr, (uint64_t) oldval,
                           (uint64_t) newval);
#else
#error unknown size of void*
#endif
}
#endif /* SANDIA_BUILTIN_CAS */

static inline void *PtlInternalAtomicSwapPtr(
    void *volatile *addr,
    void *newval)
{
    void *oldval = *addr;
    void *tmp;
    while ((tmp = PtlInternalAtomicCasPtr(addr, oldval, newval)) != oldval) {
        oldval = tmp;
    }
    return oldval;
}

#endif /* PTL_INTERNAL_ATOMIC_H */
