#ifndef PTL_LOCKS_H
#define PTL_LOCKS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "ptl_sync.h"

#ifdef HAVE_PTHREAD_SPIN_INIT
# define PTL_FASTLOCK_TYPE pthread_spinlock_t
# define PTL_FASTLOCK_INIT(x)    pthread_spin_init((x), PTHREAD_PROCESS_PRIVATE)
# define PTL_FASTLOCK_INIT_SHARED(x)    pthread_spin_init((x), PTHREAD_PROCESS_SHARED)
# define PTL_FASTLOCK_DESTROY(x) pthread_spin_destroy(x)
# define PTL_FASTLOCK_LOCK(x)    pthread_spin_lock(x)
# define PTL_FASTLOCK_UNLOCK(x)  pthread_spin_unlock(x)
#else
typedef struct ptl_spin_exclusive_s {   /* stolen from Qthreads */
    unsigned long enter;
    unsigned long exit;
} ptl_spin_exclusive_t;
# define PTL_FASTLOCK_TYPE ptl_spin_exclusive_t
# define PTL_FASTLOCK_INIT(x)    do { (x)->enter = 0; (x)->exit = 0; } while (0)
# define PTL_FASTLOCK_INIT_SHARED(x)    do { (x)->enter = 0; (x)->exit = 0; } while (0)
# define PTL_FASTLOCK_DESTROY(x) do { (x)->enter = 0; (x)->exit = 0; } while (0)
# define PTL_FASTLOCK_LOCK(x)    { unsigned long val = __sync_fetch_and_add(&(x)->enter, 1); \
                                   __sync_synchronize();                                     \
                                   while (val != (x)->exit) SPINLOCK_BODY(); }
# define PTL_FASTLOCK_UNLOCK(x)  { __sync_fetch_and_add(&(x)->exit, 1); \
                                   __sync_synchronize(); }
#endif // ifdef HAVE_PTHREAD_SPIN_INIT

#endif // ifndef PTL_LOCKS_H
/* vim:set expandtab: */
