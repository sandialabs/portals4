#ifndef PTL_INTERNAL_PAPI_H
#define PTL_INTERNAL_PAPI_H

#if defined(HAVE_LIBPAPI) || defined(USE_RDTSC)
# include "ptl_visibility.h"

enum ptl_internal_papi_func {
    PTL_PUT,                    /* 0 */
    PTL_ME_APPEND,              /* 1 */
    PTL_EQ_GET,                 /* 2 */
    PTL_EQ_PUSH,                /* 3 */
    PTL_LE_PROCESS,             /* 4 */
    PTL_ME_PROCESS,             /* 5 */

    NUM_INSTRUMENTED_FUNCS      // must be last entry
};

# define NUM_SAVE_POINTS 10

void INTERNAL PtlInternalPAPIInit(
    void);
void INTERNAL PtlInternalPAPITeardown(
    void);
void INTERNAL PtlInternalPAPIStartC(
    void);
void INTERNAL PtlInternalPAPISaveC(
    enum ptl_internal_papi_func func,
    int                         savept);
void INTERNAL PtlInternalPAPIDoneC(
    enum ptl_internal_papi_func func,
    int                         savept);
#else /* if defined(HAVE_LIBPAPI) || defined(USE_RDTSC) */
# define PtlInternalPAPIInit()
# define PtlInternalPAPITeardown()
# define PtlInternalPAPIStartC()
# define PtlInternalPAPISaveC(x, y)
# define PtlInternalPAPIDoneC(x, y)
#endif /* HAVE_LIBPAPI */

#endif /* ifndef PTL_INTERNAL_PAPI_H */
/* vim:set expandtab: */
