#ifndef PTL_INTERNAL_ALIGNMENT_H
#define PTL_INTERNAL_ALIGNMENT_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef SANDIA_ALIGNEDDATA_ALLOWED
# define ALIGNED(x) __attribute__((aligned(x)))
#else
# define ALIGNED(x)
#endif

#endif
