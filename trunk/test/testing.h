#ifndef TESTING_H
#define TESTING_H

#include <math.h> // for fabsl

#define CHECK_RETURNVAL(x) do { int ret; \
    switch (ret = x) { \
        case PTL_OK: break; \
        case PTL_FAIL: fprintf(stderr, "=> %s returned PTL_FAIL (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        case PTL_NO_SPACE: fprintf(stderr, "=> %s returned PTL_NO_SPACE (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        case PTL_ARG_INVALID: fprintf(stderr, "=> %s returned PTL_ARG_INVALID (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        case PTL_NO_INIT: fprintf(stderr, "=> %s returned PTL_NO_INIT (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        case PTL_IN_USE: fprintf(stderr, "=> %s returned PTL_IN_USE (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
        default: fprintf(stderr, "=> %s returned failcode %i (line %u)\n", #x, ret, (unsigned int)__LINE__); abort(); break; \
    } } while (0)

#define NO_FAILURES(ct,threshold) do { \
    ptl_ct_event_t ct_data; \
    CHECK_RETURNVAL(PtlCTWait(ct, threshold, &ct_data)); \
    if (ct_data.failure != 0) { \
        fprintf(stderr, "ct_data reports failure!!!!!!! {%u, %u} line %u\n", \
                (unsigned int)ct_data.success, (unsigned int)ct_data.failure, \
                (unsigned int)__LINE__); \
        abort(); \
    } \
} while (0)

/* floating-point equality is hard to check for, but this should work
 * for the comparisons of doubles used in our atomics test programs. */
#define fpequal(a,b) (fabsl((a)-(b)) < 0.00001)

#endif
