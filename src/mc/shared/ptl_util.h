#ifndef PTL_UTIL_H
#define PTL_UTIL_H

#include <assert.h>

#ifndef NDEBUG
static inline int
bitcount(unsigned long n)
{
    int count = 0 ;
    while (n)  {
        count++ ;
        n &= (n - 1) ;
    }
    return count ;
}
#endif

static inline long
ptl_find_lower_boundary(long data, long boundary)
{
    assert(1 == bitcount(boundary));
    return (data & ~(boundary - 1));
}


static inline long
ptl_find_upper_boundary(long data, long boundary)
{
    long tmp = ~(boundary - 1);
    assert(1 == bitcount(boundary));
    if ((data & tmp ) == data) {
        return data;
    } else {
        return (data & tmp) + boundary;
    }
}


static inline long 
ptl_find_higher_pow2(long val)
{
    long pow2 = 1;
    while (pow2 < val) pow2 <<= 1;
    return pow2;
}

#endif
