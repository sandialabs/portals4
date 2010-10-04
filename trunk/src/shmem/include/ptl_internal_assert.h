#ifndef PTL_INTERNAL_ASSERT_H
#define PTL_INTERNAL_ASSERT_H

#include <assert.h>

#ifndef NDEBUG
#define ptl_assert(x,y) assert((x) == (y))
#define ptl_assert_not(x,y) assert((x) != (y))
#else
#define ptl_assert(x,y) (void)x
#define ptl_assert_not(x,y) (void)x
#endif

#endif
