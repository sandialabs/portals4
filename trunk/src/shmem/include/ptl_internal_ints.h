#ifndef PTL_INTERNAL_INTS_H
#define PTL_INTERNAL_INTS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_UINT_FAST8_T
typedef uint_fast8_t uint8_t;
#endif

#ifndef HAVE_UINT_FAST32_T
typedef uint_fast32_t uint32_t;
#endif

#ifndef HAVE_UINT_FAST64_T
typedef uint_fast64_t uint64_t;
#endif

#endif
