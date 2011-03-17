#ifndef PTL_INTERNAL_LOCKS_H
#define PTL_INTERNAL_LOCKS_H

#ifdef __tile__
# include <tmc/sync.h>
# define PTL_LOCK_TYPE tmc_sync_mutex_t
# define PTL_LOCK_INIT(x)   tmc_sync_mutex_init(&(x))
# define PTL_LOCK_LOCK(x)   tmc_sync_mutex_lock(&(x))
# define PTL_LOCK_UNLOCK(x) tmc_sync_mutex_unlock(&(x))
#elif defined(HAVE_PTHREAD_SPIN_INIT)
# include <pthread.h>
# define PTL_LOCK_TYPE pthread_spinlock_t
# define PTL_LOCK_INIT(x)   pthread_spin_init(&(x), PTHREAD_PROCESS_PRIVATE)
# define PTL_LOCK_LOCK(x)   ptl_assert(pthread_spin_lock(&(x)), 0)
# define PTL_LOCK_UNLOCK(x) ptl_assert(pthread_spin_unlock(&(x)), 0)
#else /* ifdef __tile__ */
# define PTL_LOCK_TYPE uint32_t
# define PTL_LOCK_INIT(x)   (x) = 0
# define PTL_LOCK_LOCK(x)   do { \
        while ((x) == 1) { \
            __asm__ __volatile__ ("pause" ::: "memory"); \
        } \
} while (!__sync_bool_compare_and_swap(&(x), 0, 1))
# define PTL_LOCK_UNLOCK(x) do { \
        __sync_synchronize(); \
        (x) = 0; \
} while (0)
/*#else
 # include <pthread.h>
 # define PTL_LOCK_TYPE pthread_mutex_t
 # define PTL_LOCK_INIT(x) pthread_mutex_init(&(x), NULL)
 # define PTL_LOCK_LOCK(x) ptl_assert(pthread_mutex_lock(&(x)), 0)
 # define PTL_LOCK_UNLOCK(x) ptl_assert(pthread_mutex_unlock(&(x)), 0)*/
#endif /* ifdef __tile__ */

#endif /* ifndef PTL_INTERNAL_LOCKS_H */
/* vim:set expandtab */
