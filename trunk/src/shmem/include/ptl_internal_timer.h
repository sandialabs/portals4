#ifndef PTL_INTERNAL_TIMER_H
#define PTL_INTERNAL_TIMER_H

#include "ptl_internal_assert.h"

#ifdef HAVE_MACH_TIMER
# include <mach/mach_time.h>
# define TIMER_TYPE uint64_t
# define MARK_TIMER(x) do { x = mach_absolute_time(); } while (0)
# define TIMER_INTS(x) (x)
# define MILLI_TO_TIMER_INTS(x) do { x *= 1000000; } while (0)
#elif defined(HAVE_GETTIME_TIMER)
# ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 199309L
# endif
# include <time.h>
# define TIMER_TYPE struct timespec
# define MARK_TIMER(x) ptl_assert(clock_gettime(CLOCK_MONOTONIC, &x), 0)
# define TIMER_INTS(x) (x.tv_sec * 1000000000 + x.tv_nsec)
# define MILLI_TO_TIMER_INTS(x) do { x *= 1000000; } while (0)
#else
# error No timers are defined!
#endif

#endif
/* vim:set expandtab */
