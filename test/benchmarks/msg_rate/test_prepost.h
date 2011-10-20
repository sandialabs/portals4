
#include <portals4.h>
#include <support/support.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h> /* for exit() */

#ifdef __APPLE__
# include <sys/time.h>
#endif

#if 0 
#define Debug(fmt, args... ) \
    printf( "%d:%s():%d: "fmt, rank, __FUNCTION__,__LINE__, ## args )
#else
#define Debug(fmt, args... )
#endif

#include <assert.h>

#ifndef NDEBUG 
#define ptl_assert(x,y) assert((x) == (y))
#define ptl_assert_not(x,y) assert((x) != (y))
#else
#define ptl_assert(x,y) (void)x
#define ptl_assert_not(x,y) (void)x
#endif


#define TestOneWayIndex (1)
#define TestSameDirectionIndex  (2)
#define SEND_BUF_SIZE   (npeers * nmsgs * nbytes)
#define RECV_BUF_SIZE   (SEND_BUF_SIZE)
#define magic_tag 1

extern int machine_output;

extern int *send_peers;
extern int *recv_peers;
extern char *send_buf;
extern char *recv_buf;

extern int rank;
extern int world_size;

extern void
test_prepostME(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers,
                int nmsgs, int nbytes, int niters );

extern void
test_prepostLE(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers,
                int nmsgs, int nbytes, int niters );

static inline double
timer(void)
{
#ifdef __APPLE__
    struct timeval tm;
    gettimeofday(&tm, NULL);
    return tm.tv_sec + tm.tv_usec * 1e-6;
#else
    struct timespec tm;

    clock_gettime(CLOCK_REALTIME, &tm);
    return tm.tv_sec + tm.tv_nsec / 1000000000.0;
#endif
}  /* end of timer() */

static inline  void     
cache_invalidate(int cache_size, int *cache_buf)
{                   
    int i;              
                    
    if (cache_size != 0) {
        cache_buf[0]= 1;
        for (i= 1 ; i < cache_size; i++)   {
            cache_buf[i]= cache_buf[i - 1];
        }
    }
}  /* end of cache_invalidate() */


static void
display_result(const char *test, const double result)
{
    if (0 == rank)   {
        if (machine_output)   {
            printf("%.2f ", result);
        } else   {
            printf("%20s: %.2f\n", test, result);
        }
    }

}  /* end of display_result() */

