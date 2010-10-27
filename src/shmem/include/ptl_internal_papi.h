#ifndef PTL_INTERNAL_PAPI_H
#define PTL_INTERNAL_PAPI_H

#ifdef HAVE_LIBPAPI
# include "ptl_visibility.h"

enum {
    PTL_PUT,
    PTL_ME_APPEND,
    PTL_EQ_GET,
    PTL_EQ_PUSH,
    PTL_LE_PROCESS,
    PTL_ME_PROCESS,

    NUM_INSTRUMENTED_PROCS // must be last entry
} ptl_internal_papi_func;

void INTERNAL PtlInternalPAPIInit(
    void);
void INTERNAL PtlInternalPAPITeardown(
    void);
void INTERNAL PtlInternalPAPIStartC(
    void);
void INTERNAL PtlInternalPAPISaveC(
    ptl_internal_papi_func func,
    int savept);
void INTERNAL PtlInternalPAPIDoneC(
    ptl_internal_papi_func func,
    int savept);
#else
# define PtlInternalPAPIInit()
# define PtlInternalPAPITeardown()
# define PtlInternalPAPIStartC()
# define PtlInternalPAPISaveC(x,y)
# define PtlInternalPAPIDoneC(x,y)
#endif /* HAVE_LIBPAPI */

#endif
/* vim:set expandtab: */
