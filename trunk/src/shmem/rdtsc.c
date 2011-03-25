#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "ptl_internal_papi.h"
#include "ptl_internal_commpad.h"

static FILE *papi_out = NULL;

typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
static inline uint64_t rdtsc(void)
{
    uint32_t low, high;

    __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)high << 32) | low;
}

static uint64_t _ptlRdtscStart;
static uint64_t _ptlRdtscSums[NUM_INSTRUMENTED_FUNCS][NUM_SAVE_POINTS];
static uint64_t _ptlRdtscMeasurements[NUM_INSTRUMENTED_FUNCS][NUM_SAVE_POINTS];

void INTERNAL PtlInternalPAPIInit(void)
{
    memset(_ptlRdtscSums, 0, sizeof(_ptlRdtscSums));
    memset(_ptlRdtscMeasurements, 0, sizeof(_ptlRdtscMeasurements));

    {
        char fname[50];
        snprintf(fname, 50, "papi.r%i.out", (int)proc_number);
        papi_out = fopen(fname, "w");
        assert(papi_out);
    }
}

void INTERNAL PtlInternalPAPITeardown(void)
{
    for (int func = 0; func < NUM_INSTRUMENTED_FUNCS; func++) {
        for (int savept = 0; savept < NUM_SAVE_POINTS; savept++ ) {
            uint64_t sum, cnt;
            cnt = _ptlRdtscMeasurements[func][savept];
            sum = _ptlRdtscSums[func][savept];
            if ( cnt ) {
                fprintf(papi_out, "func%i pt%i ", func, savept);
                fprintf(papi_out, " %f ", ((double)sum) / ((double)cnt));
                fprintf(papi_out, "measured %li\n", cnt);
            }
        }
    }
}

void PtlInternalPAPIStartC(void)
{
    _ptlRdtscStart = rdtsc();
}

void PtlInternalPAPISaveC(enum ptl_internal_papi_func func,
                          int                         savept)
{
    uint64_t now = rdtsc();

    _ptlRdtscSums[func][savept] += now - _ptlRdtscStart;
    ++_ptlRdtscMeasurements[func][savept];
    _ptlRdtscStart = rdtsc();
}

void PtlInternalPAPIDoneC(enum ptl_internal_papi_func func,
                          int                         savept)
{
    uint64_t now = rdtsc();

    _ptlRdtscSums[func][savept] += now - _ptlRdtscStart;
    ++_ptlRdtscMeasurements[func][savept];
}

/* vim:set expandtab: */
