#ifndef PTL_INTERNAL_PAPI_H
#define PTL_INTERNAL_PAPI_H

#include "ptl_visibility.h"

void INTERNAL PtlInternalPAPIInit(
    void);
void INTERNAL PtlInternalPAPITeardown(
    void);
void INTERNAL PtlInternalPAPIStartC(
    void);
void INTERNAL PtlInternalPAPISaveC(
    int func,
    int savept);
void INTERNAL PtlInternalPAPIDoneC(
    int func,
    int savept);

#endif
