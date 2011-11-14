/*
 * This Cplant(TM) source code is part of the Portals3 Reference
 * Implementation.
 *
 * This Cplant(TM) source code is the property of Sandia National
 * Laboratories.
 *
 * This Cplant(TM) source code is copyrighted by Sandia National
 * Laboratories.
 *
 * The redistribution of this Cplant(TM) source code is subject to the
 * terms of version 2 of the GNU General Public License.
 * (See COPYING, or http://www.gnu.org/licenses/lgpl.html.)
 *
 * Cplant(TM) Copyright 1998-2006 Sandia Corporation. 
 *
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the US Government.
 * Export of this program may require a license from the United States
 * Government.
 */


/* Portals3 is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License,
 * as published by the Free Software Foundation.
 *
 * Portals3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Portals3; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Questions or comments about this library should be sent to:
 *
 * Jim Schutt
 * Sandia National Laboratories, New Mexico
 * P.O. Box 5800
 * Albuquerque, NM 87185-0806
 *
 * jaschut@sandia.gov
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <time.h>

#ifndef _PTL3_USER_P3UTILS_H_
#define _PTL3_USER_P3UTILS_H_

/* Some errors are so egregious we can't possibly continue.  We have to roll
 * over and die, whether we're in user or kernel space.
 */
#ifdef NDEBUG
#define PTL_ROAD()  abort()
#else
#include <assert.h>
#define PTL_ROAD()  assert(0)
#endif

/* Make sure everybody uses the same MIN/MAX macros.
 */

#ifdef MAX
#undef MAX
#endif
#define MAX(a,b) \
({ __typeof__(a) __a = a; __typeof__(b) __b = b; __a > __b ? __a : __b; })

#ifdef MIN
#undef MIN
#endif
#define MIN(a,b) \
({ __typeof__(a) __a = a; __typeof__(b) __b = b; __a < __b ? __a : __b; })

#ifdef USER_PROGRESS_THREAD
#include <pthread.h>
extern pthread_mutex_t _thread_memory_mutex;
#define memory_barrier_start()	\
do {							\
	pthread_mutex_lock(&_thread_memory_mutex);	\
} while (0)
#define memory_barrier_end()	\
do {							\
	pthread_mutex_unlock(&_thread_memory_mutex);	\
} while (0)
#else
#define memory_barrier_start() do {} while (0)
#define memory_barrier_end()   do {} while (0)
#endif

void p3utils_init(void);
extern FILE *p3_out;

#define P3MSG_EMERG   "EMERG: "
#define P3MSG_ALERT   "ALERT: "
#define P3MSG_CRIT    "CRIT: "
#define P3MSG_ERR     "ERR: "
#define P3MSG_WARNING "WARNING: "
#define P3MSG_NOTICE  "NOTICE: "
#define P3MSG_INFO    "INFO: "
#define P3MSG_DEBUG   "DEBUG: "

#define p3_flush() do { fflush(p3_out); } while (0)

#if 1
#define p3_print(args...) \
do {							\
	struct timeval now = {0,};			\
	gettimeofday(&now, NULL);			\
	fprintf(p3_out, "%ld.%06ld: ",			\
		(long)now.tv_sec, (long)now.tv_usec);	\
	fprintf(p3_out, args);				\
} while (0)
#else
#define p3_print(args...) do { fprintf(p3_out, args); } while (0)
#endif

static inline 
void *p3_malloc(size_t sz)
{
	return malloc(sz);
}

static inline 
void *p3_realloc(void* ptr, size_t old_sz, size_t new_sz)
{
	return realloc(ptr, new_sz);
}

static inline
void p3_free(void *ptr)
{
	free(ptr);
}

static inline
char *p3_strdup(const char *ptr)
{
	char *dup = NULL;

	if (ptr && (dup = p3_malloc(strlen(ptr)+1)))
		strcpy(dup, ptr);

	return dup;
}

static inline
void p3_yield(void)
{
#ifdef _POSIX_PRIORITY_SCHEDULING
	sched_yield();
#else
#warn Expect poor performance - no sched_yield() available
	nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 0}, NULL);
#endif
}


#include <sys/time.h>

typedef struct timeval timeout_val_t;

static inline
void set_timeout(timeout_val_t *to, unsigned msec_delay)
{
		gettimeofday(to, NULL);
		to->tv_sec += msec_delay / 1000;
		to->tv_usec += (msec_delay % 1000) * 1000;

		to->tv_sec += to->tv_usec / 1000000;
		to->tv_usec = to->tv_usec % 1000000;
}

static inline
void clear_timeout(timeout_val_t *to)
{
	timerclear(to);
}

static inline
int timeout_set(timeout_val_t *to)
{
	return timerisset(to);
}

/* Returns 0 if timeout has not expired, none-zero if it has.
 * BY definition, an unset timeout is never expired.
 */
static inline
int test_timeout(timeout_val_t *to)
{
	struct timeval now;

	if (!timerisset(to))
		return 0;
	gettimeofday(&now, NULL);

	return timercmp(&now, to, <) ? 0 : 1;
}

#endif /* _PTL3_USER_P3UTILS_H_ */
