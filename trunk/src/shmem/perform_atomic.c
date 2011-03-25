#include <portals4.h>

/* System headers */
#include <stdlib.h>                    /* for abort() */
#include <complex.h>
#include <string.h> /* for memcpy() */

/* Internal headers */
#include "ptl_internal_performatomic.h"
#include "ptl_visibility.h"

#define ADD_OP(type, r, a, b)  r = (type)((a) + (b))
#define PROD_OP(type, r, a, b) r = (type)((a) * (b))
#define MAX_OP(type, r, a, b)  r = (type)(((a) > (b)) ? (a) : (b))
#define MIN_OP(type, r, a, b)  r = (type)(((a) < (b)) ? (a) : (b))
#define LOR_OP(type, r, a, b)  r = (type)(((a) || (b)) ? (1) : (0))
#define LAND_OP(type, r, a, b) r = (type)(((a) && (b)) ? (1) : (0))
#define LXOR_OP(type, r, a, b) r = (type)(((a) || (b)) ? (0) : (1))
#define BOR_OP(type, r, a, b)  r = (type)((a) | (b))
#define BAND_OP(type, r, a, b) r = (type)((a) & (b))
#define BXOR_OP(type, r, a, b) r = (type)((a) ^ (b))

#define ADD_MACRO(x, y) (*(x)); (*(x)) += (y)

// #define ACTUALLY_ATOMIC

#ifdef ACTUALLY_ATOMIC
# define ADD_OPERATION __sync_fetch_and_add
# define NONBUILTIN_CAS(Type, EqIntType, Op) do { \
        union { Type t; EqIntType i; } first, second; \
        do { \
            first.i = *(volatile EqIntType*)dest; \
            Op(Type, second.t, first.t, *(Type*)src); \
        } while (!__sync_bool_compare_and_swap((volatile EqIntType*)dest, first.i, second.i)); \
        *(Type*)src = first.t; \
} while (0)
# define NONBUILTIN_INT_CAS(Type, Op)        do { \
        Type first, second; \
        do { \
            first = *(volatile Type*)dest; \
            Op(Type, second, first, *(Type*)src); \
        } while (!__sync_bool_compare_and_swap((volatile Type*)dest, first, second)); \
        *(Type*)src = first; \
} while (0)
# define BUILTINSWAP(int_type)               do { \
        int_type before = *(volatile int_type *)dest, tmp; \
        while ((tmp = __sync_val_compare_and_swap((volatile int_type*)dest, before, *(int_type*)src)) != before) { \
                    before = tmp; \
                } \
                *(int_type*)src = before; \
               } while (0)
# define CAS(type)                           do { \
        type first, second; \
        second      = *(type*)&operand; \
        first       = __sync_val_compare_and_swap((volatile type*)dest, second, *(type*)src); \
        *(type*)src = first; \
} while (0)
# define MAS(type)                           do { \
        type       curv, newv; \
        const type mask = *(type*)&operand; \
        const type srcv = *(type*)src; \
        do { \
            curv = *(volatile type*)dest; \
            newv = (type)((curv & ~mask) | (srcv & mask)); \
        } while (!__sync_bool_compare_and_swap((volatile type*)dest, curv, newv)); \
        *(type*)src = curv; \
} while (0)
# define CAS_NE(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        do { \
            curv = *(volatile type*)dest; \
            if (curv == newv) { break; } \
        } while (!__sync_bool_compare_and_swap((volatile type*)dest, curv, newv)); \
        *(type*)src = curv; \
} while (0)
# define CAS_LE(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        do { \
            curv = *(volatile type*)dest; \
            if (curv > newv) { break; } \
        } while (!__sync_bool_compare_and_swap((volatile type*)dest, curv, newv)); \
        *(type*)src = curv; \
} while (0)
# define CAS_LT(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        do { \
            curv = *(volatile type*)dest; \
            if (curv >= newv) { break; } \
        } while (!__sync_bool_compare_and_swap((volatile type*)dest, curv, newv)); \
        *(type*)src = curv; \
} while (0)
# define CAS_GE(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        do { \
            curv = *(volatile type*)dest; \
            if (curv < newv) { break; } \
        } while (!__sync_bool_compare_and_swap((volatile type*)dest, curv, newv)); \
        *(type*)src = curv; \
} while (0)
# define CAS_GT(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        do { \
            curv = *(volatile type*)dest; \
            if (curv <= newv) { break; } \
        } while (!__sync_bool_compare_and_swap((volatile type*)dest, curv, newv)); \
        *(type*)src = curv; \
} while (0)
#else /* not actually atomic */
# define ADD_OPERATION ADD_MACRO
# define NONBUILTIN_CAS(Type, EqIntType, Op) do { \
        union { Type t; EqIntType i; } first, second; \
        first.i = *(volatile EqIntType*)dest; \
        Op(Type, second.t, first.t, *(Type*)src); \
        *(volatile EqIntType*)(dest) = second.i; \
        *(Type*)src                  = first.t; \
} while (0)
# define NONBUILTIN_INT_CAS(Type, Op)        do { \
        Type first, second; \
        first = *(volatile Type*)dest; \
        Op(Type, second, first, *(Type*)src); \
        *(volatile Type*)(dest) = second; \
        *(Type*)src             = first; \
} while (0)
# define BUILTINSWAP(int_type)               do { \
        uint8_t before[sizeof(int_type)]; \
        memcpy(before, (const void*)dest, sizeof(int_type)); \
        memcpy((void*)dest, (const void*)src, sizeof(int_type)); \
        memcpy((void*)src, before, sizeof(int_type)); \
} while (0)
# define CAS(type)                           do { \
        type first, second; \
        second = *(type*)&operand; \
        first  = *(type*)dest; \
        if (first == second) { *(type*)dest = *(type*)src; } \
        *(type*)src = first; \
} while (0)
# define MAS(type)                           do { \
        type       curv; \
        const type mask = *(type*)&operand; \
        const type srcv = *(type*)src; \
        curv = *(volatile type*)dest; \
        if (*(type*)dest == curv) { *(type*)dest = (type)((curv & ~mask) | (srcv & mask)); } \
        *(type*)src = curv; \
} while (0)
# define CAS_NE(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        curv = *(volatile type*)dest; \
        if (curv != newv) { \
            if (*(type*)dest == curv) { *(type*)dest = *(type*)src; } \
        } \
        *(type*)src = curv; \
} while (0)
# define CAS_LE(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        curv = *(volatile type*)dest; \
        if (curv <= newv) { \
            if (*(type*)dest == curv) { *(type*)dest = newv; } \
        } \
        *(type*)src = curv; \
} while (0)
# define CAS_LT(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        curv = *(volatile type*)dest; \
        if (curv < newv) { \
            if (*(type*)dest == curv) { *(type*)dest = newv; } \
        } \
        *(type*)src = curv; \
} while (0)
# define CAS_GE(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        curv = *(volatile type*)dest; \
        if (curv >= newv) { \
            if (*(type*)dest == curv) { *(type*)dest = newv; } \
        } \
        *(type*)src = curv; \
} while (0)
# define CAS_GT(type)                        do { \
        type       curv; \
        const type newv = *(type*)src; \
        curv = *(volatile type*)dest; \
        if (curv > newv) { \
            if (*(type*)dest == curv) { *(type*)dest = newv; } \
        } \
        *(type*)src = curv; \
} while (0)
#endif /* ifdef ACTUALLY_ATOMIC */

#define PERFORM_UNIVERSAL_DATATYPE_FUNC_COMPARISON(fname, op) \
    static void inline PtlInternalPerformAtomic ## fname(volatile char *dest, \
                                                         char *src, \
                                                         ptl_datatype_t dt) \
    { \
        switch (dt) { \
            case PTL_CHAR:   NONBUILTIN_INT_CAS(int8_t, op); break; \
            case PTL_UCHAR:  NONBUILTIN_INT_CAS(uint8_t, op); break; \
            case PTL_SHORT:  NONBUILTIN_INT_CAS(int16_t, op); break; \
            case PTL_USHORT: NONBUILTIN_INT_CAS(uint16_t, op); break; \
            case PTL_INT:    NONBUILTIN_INT_CAS(int32_t, op); break; \
            case PTL_UINT:   NONBUILTIN_INT_CAS(uint32_t, op); break; \
            case PTL_LONG:   NONBUILTIN_INT_CAS(int64_t, op); break; \
            case PTL_ULONG:  NONBUILTIN_INT_CAS(uint64_t, op); break; \
            case PTL_FLOAT:  NONBUILTIN_CAS(float, uint32_t, op); break; \
            case PTL_DOUBLE: NONBUILTIN_CAS(double, uint64_t, op); break; \
            case PTL_LONG_DOUBLE: NONBUILTIN_INT_CAS(long double, op); break; \
            default: abort(); \
        } \
    }
#define PERFORM_UNIVERSAL_DATATYPE_FUNC(fname, op) \
    static void inline PtlInternalPerformAtomic ## fname(volatile char *dest, \
                                                         char *src, \
                                                         ptl_datatype_t dt) \
    { \
        switch (dt) { \
            case PTL_CHAR:   NONBUILTIN_INT_CAS(int8_t, op); break; \
            case PTL_UCHAR:  NONBUILTIN_INT_CAS(uint8_t, op); break; \
            case PTL_SHORT:  NONBUILTIN_INT_CAS(int16_t, op); break; \
            case PTL_USHORT: NONBUILTIN_INT_CAS(uint16_t, op); break; \
            case PTL_INT:    NONBUILTIN_INT_CAS(int32_t, op); break; \
            case PTL_UINT:   NONBUILTIN_INT_CAS(uint32_t, op); break; \
            case PTL_LONG:   NONBUILTIN_INT_CAS(int64_t, op); break; \
            case PTL_ULONG:  NONBUILTIN_INT_CAS(uint64_t, op); break; \
            case PTL_FLOAT:  NONBUILTIN_CAS(float, uint32_t, op); break; \
            case PTL_DOUBLE: NONBUILTIN_CAS(double, uint64_t, op); break; \
            case PTL_LONG_DOUBLE: NONBUILTIN_INT_CAS(long double, op); break; \
            case PTL_LONG_DOUBLE_COMPLEX: NONBUILTIN_INT_CAS(long double complex, op); break; \
            case PTL_DOUBLE_COMPLEX: NONBUILTIN_INT_CAS(double complex, op); break; \
            case PTL_FLOAT_COMPLEX: NONBUILTIN_INT_CAS(float complex, op); break; \
        } \
    }
#define PERFORM_INTEGER_DATATYPE_FUNC(fname, op) static void inline PtlInternalPerformAtomic ## fname( \
                                                                                                      volatile char *dest, \
                                                                                                      char *src, \
                                                                                                      ptl_datatype_t dt) \
    { \
        switch (dt) { \
            case PTL_CHAR:   NONBUILTIN_INT_CAS(int8_t, op); break; \
            case PTL_UCHAR:  NONBUILTIN_INT_CAS(uint8_t, op); break; \
            case PTL_SHORT:  NONBUILTIN_INT_CAS(int16_t, op); break; \
            case PTL_USHORT: NONBUILTIN_INT_CAS(uint16_t, op); break; \
            case PTL_INT:    NONBUILTIN_INT_CAS(int32_t, op); break; \
            case PTL_UINT:   NONBUILTIN_INT_CAS(uint32_t, op); break; \
            case PTL_LONG:   NONBUILTIN_INT_CAS(int64_t, op); break; \
            case PTL_ULONG:  NONBUILTIN_INT_CAS(uint64_t, op); break; \
            default: abort(); \
        } \
    }

PERFORM_UNIVERSAL_DATATYPE_FUNC_COMPARISON(Min, MIN_OP)
PERFORM_UNIVERSAL_DATATYPE_FUNC_COMPARISON(Max, MAX_OP)
PERFORM_UNIVERSAL_DATATYPE_FUNC(Prod, PROD_OP)
PERFORM_INTEGER_DATATYPE_FUNC(Lor, LOR_OP)
PERFORM_INTEGER_DATATYPE_FUNC(Land, LAND_OP)
PERFORM_INTEGER_DATATYPE_FUNC(Lxor, LXOR_OP)
PERFORM_INTEGER_DATATYPE_FUNC(Bor, BOR_OP)
PERFORM_INTEGER_DATATYPE_FUNC(Band, BAND_OP)
PERFORM_INTEGER_DATATYPE_FUNC(Bxor, BXOR_OP)
#define INT_BUILTIN(int_type, builtin) do { \
        int_type before = builtin((int_type*)dest, *(int_type*)src); \
        *(int_type*)src = before; \
} while (0)
static void inline PtlInternalPerformAtomicSum(volatile char *dest,
                                               char          *src,
                                               ptl_datatype_t dt)
{
    switch (dt) {
        case PTL_CHAR:                INT_BUILTIN(int8_t, ADD_OPERATION); break;
        case PTL_UCHAR:               INT_BUILTIN(uint8_t, ADD_OPERATION); break;
        case PTL_SHORT:               INT_BUILTIN(int16_t, ADD_OPERATION); break;
        case PTL_USHORT:              INT_BUILTIN(uint16_t, ADD_OPERATION); break;
        case PTL_INT:                 INT_BUILTIN(int32_t, ADD_OPERATION); break;
        case PTL_UINT:                INT_BUILTIN(uint32_t, ADD_OPERATION); break;
        case PTL_LONG:                INT_BUILTIN(int64_t, ADD_OPERATION); break;
        case PTL_ULONG:               INT_BUILTIN(uint64_t, ADD_OPERATION); break;
        case PTL_FLOAT: NONBUILTIN_CAS(float, uint32_t, ADD_OP); break;
        case PTL_DOUBLE: NONBUILTIN_CAS(double, uint64_t, ADD_OP); break;
        case PTL_LONG_DOUBLE:         INT_BUILTIN(long double, ADD_OPERATION); break;
        case PTL_LONG_DOUBLE_COMPLEX: INT_BUILTIN(long double complex, ADD_OPERATION); break;
        case PTL_DOUBLE_COMPLEX:      INT_BUILTIN(double complex, ADD_OPERATION); break;
        case PTL_FLOAT_COMPLEX:       INT_BUILTIN(float complex, ADD_OPERATION); break;
    }
}

static void inline PtlInternalPerformAtomicSwap(volatile char *dest,
                                                char          *src,
                                                ptl_datatype_t dt)
{
    switch (dt) {
        case PTL_CHAR:                BUILTINSWAP(int8_t); break;
        case PTL_UCHAR:               BUILTINSWAP(uint8_t); break;
        case PTL_SHORT:               BUILTINSWAP(int16_t); break;
        case PTL_USHORT:              BUILTINSWAP(uint16_t); break;
        case PTL_INT:                 BUILTINSWAP(int32_t); break;
        case PTL_UINT:                BUILTINSWAP(uint32_t); break;
        case PTL_LONG:                BUILTINSWAP(int64_t); break;
        case PTL_ULONG:               BUILTINSWAP(uint64_t); break;
        case PTL_FLOAT:               BUILTINSWAP(uint32_t); break;
        case PTL_DOUBLE:              BUILTINSWAP(uint64_t); break;
        case PTL_FLOAT_COMPLEX:       BUILTINSWAP(uint64_t); break;
        case PTL_LONG_DOUBLE:         BUILTINSWAP(long double); break;
        case PTL_DOUBLE_COMPLEX:      BUILTINSWAP(double complex); break;
        case PTL_LONG_DOUBLE_COMPLEX: BUILTINSWAP(long double complex); break;
    }
}

static unsigned char datatype_size_table[] = { 1, 1, 2, 2, 4, 4, 8, 8, 4, 8 };

void INTERNAL PtlInternalPerformAtomic(char          *dest,
                                       char          *src,
                                       ptl_size_t     size,
                                       ptl_op_t       op,
                                       ptl_datatype_t dt)
{
    ptl_size_t sz = datatype_size_table[dt];

    if (sz == size) {
        switch (op) {
            case PTL_MIN:
                PtlInternalPerformAtomicMin(dest, src, dt);
                break;
            case PTL_MAX:
                PtlInternalPerformAtomicMax(dest, src, dt);
                break;
            case PTL_SUM:
                PtlInternalPerformAtomicSum(dest, src, dt);
                break;
            case PTL_PROD:
                PtlInternalPerformAtomicProd(dest, src, dt);
                break;
            case PTL_LOR:
                PtlInternalPerformAtomicLor(dest, src, dt);
                break;
            case PTL_BOR:
                PtlInternalPerformAtomicBor(dest, src, dt);
                break;
            case PTL_LAND:
                PtlInternalPerformAtomicLand(dest, src, dt);
                break;
            case PTL_BAND:
                PtlInternalPerformAtomicBand(dest, src, dt);
                break;
            case PTL_LXOR:
                PtlInternalPerformAtomicLxor(dest, src, dt);
                break;
            case PTL_BXOR:
                PtlInternalPerformAtomicBxor(dest, src, dt);
                break;
            case PTL_SWAP:
                PtlInternalPerformAtomicSwap(dest, src, dt);
                break;
            default:
                abort();
        }
    } else {
        size_t count = size / sz;
        switch (op) {
            case PTL_MIN:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicMin(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_MAX:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicMax(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_SUM:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicSum(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_PROD:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicProd(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_LOR:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicLor(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_BOR:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicBor(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_LAND:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicLand(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_BAND:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicBand(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_LXOR:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicLxor(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_BXOR:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicBxor(dest + i * sz, src + i * sz, dt);
                }
                break;
            case PTL_SWAP:
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicSwap(dest + i * sz, src + i * sz, dt);
                }
                break;
            default:
                abort();
        }
    }
}

void INTERNAL PtlInternalPerformAtomicArg(char          *dest,
                                          char          *src,
                                          uint64_t       operand,
                                          ptl_size_t     size,
                                          ptl_op_t       op,
                                          ptl_datatype_t dt)
{
    switch (op) {
        case PTL_SWAP:
        {
            ptl_size_t sz = datatype_size_table[dt];
            if (size == sz) {
                PtlInternalPerformAtomicSwap(dest, src, dt);
            } else {
                size_t count = size / sz;
                for (size_t i = 0; i < count; ++i) {
                    PtlInternalPerformAtomicSwap(dest + i * sz, src + i * sz, dt);
                }
            }
            break;
        }
        case PTL_CSWAP:
            switch (dt) {
                case PTL_CHAR:                CAS(int8_t); break;
                case PTL_UCHAR:               CAS(uint8_t); break;
                case PTL_SHORT:               CAS(int16_t); break;
                case PTL_USHORT:              CAS(uint16_t); break;
                case PTL_INT:                 CAS(int32_t); break;
                case PTL_UINT:                CAS(uint32_t); break;
                case PTL_FLOAT:               CAS(float); break;
                case PTL_LONG:                CAS(uint64_t); break;
                case PTL_ULONG:               CAS(uint64_t); break;
                case PTL_DOUBLE:              CAS(double); break;
                case PTL_LONG_DOUBLE:         CAS(long double); break;
                case PTL_FLOAT_COMPLEX:       CAS(float complex); break;
                case PTL_DOUBLE_COMPLEX:      CAS(double complex); break;
                case PTL_LONG_DOUBLE_COMPLEX: CAS(long double complex); break;
            }
            break;
        case PTL_CSWAP_NE:
            switch (dt) {
                case PTL_CHAR:                CAS_NE(int8_t); break;
                case PTL_UCHAR:               CAS_NE(uint8_t); break;
                case PTL_SHORT:               CAS_NE(int16_t); break;
                case PTL_USHORT:              CAS_NE(uint16_t); break;
                case PTL_INT:                 CAS_NE(int32_t); break;
                case PTL_UINT:                CAS_NE(uint32_t); break;
                case PTL_FLOAT:               CAS_NE(float); break;
                case PTL_LONG:                CAS_NE(uint64_t); break;
                case PTL_ULONG:               CAS_NE(uint64_t); break;
                case PTL_DOUBLE:              CAS_NE(double); break;
                case PTL_LONG_DOUBLE:         CAS_NE(long double); break;
                case PTL_FLOAT_COMPLEX:       CAS_NE(float complex); break;
                case PTL_DOUBLE_COMPLEX:      CAS_NE(double complex); break;
                case PTL_LONG_DOUBLE_COMPLEX: CAS_NE(long double complex); break;
            }
            break;
        case PTL_CSWAP_LE:
            switch (dt) {
                case PTL_CHAR:                CAS_LE(int8_t); break;
                case PTL_UCHAR:               CAS_LE(uint8_t); break;
                case PTL_SHORT:               CAS_LE(int16_t); break;
                case PTL_USHORT:              CAS_LE(uint16_t); break;
                case PTL_INT:                 CAS_LE(int32_t); break;
                case PTL_UINT:                CAS_LE(uint32_t); break;
                case PTL_FLOAT:               CAS_LE(float); break;
                case PTL_LONG:                CAS_LE(uint64_t); break;
                case PTL_ULONG:               CAS_LE(uint64_t); break;
                case PTL_DOUBLE:              CAS_LE(double); break;
                case PTL_LONG_DOUBLE:         CAS_LE(long double); break;
                default:                      abort();
            }
            break;
        case PTL_CSWAP_LT:
            switch (dt) {
                case PTL_CHAR:                CAS_LT(int8_t); break;
                case PTL_UCHAR:               CAS_LT(uint8_t); break;
                case PTL_SHORT:               CAS_LT(int16_t); break;
                case PTL_USHORT:              CAS_LT(uint16_t); break;
                case PTL_INT:                 CAS_LT(int32_t); break;
                case PTL_UINT:                CAS_LT(uint32_t); break;
                case PTL_FLOAT:               CAS_LT(float); break;
                case PTL_LONG:                CAS_LT(uint64_t); break;
                case PTL_ULONG:               CAS_LT(uint64_t); break;
                case PTL_DOUBLE:              CAS_LT(double); break;
                case PTL_LONG_DOUBLE:         CAS_LT(long double); break;
                default:                      abort();
            }
            break;
        case PTL_CSWAP_GE:
            switch (dt) {
                case PTL_CHAR:                CAS_GE(int8_t); break;
                case PTL_UCHAR:               CAS_GE(uint8_t); break;
                case PTL_SHORT:               CAS_GE(int16_t); break;
                case PTL_USHORT:              CAS_GE(uint16_t); break;
                case PTL_INT:                 CAS_GE(int32_t); break;
                case PTL_UINT:                CAS_GE(uint32_t); break;
                case PTL_FLOAT:               CAS_GE(float); break;
                case PTL_LONG:                CAS_GE(uint64_t); break;
                case PTL_ULONG:               CAS_GE(uint64_t); break;
                case PTL_DOUBLE:              CAS_GE(double); break;
                case PTL_LONG_DOUBLE:         CAS_GE(long double); break;
                default:                      abort();
            }
            break;
        case PTL_CSWAP_GT:
            switch (dt) {
                case PTL_CHAR:                CAS_GT(int8_t); break;
                case PTL_UCHAR:               CAS_GT(uint8_t); break;
                case PTL_SHORT:               CAS_GT(int16_t); break;
                case PTL_USHORT:              CAS_GT(uint16_t); break;
                case PTL_INT:                 CAS_GT(int32_t); break;
                case PTL_UINT:                CAS_GT(uint32_t); break;
                case PTL_FLOAT:               CAS_GT(float); break;
                case PTL_LONG:                CAS_GT(uint64_t); break;
                case PTL_ULONG:               CAS_GT(uint64_t); break;
                case PTL_DOUBLE:              CAS_GT(double); break;
                case PTL_LONG_DOUBLE:         CAS_GT(long double); break;
                default:                      abort();
            }
            break;
        case PTL_MSWAP:
            switch (dt) {
                case PTL_CHAR:
                case PTL_UCHAR:
                    MAS(uint8_t);
                    break;
                case PTL_SHORT:
                case PTL_USHORT:
                    MAS(uint16_t);
                    break;
                case PTL_INT:
                case PTL_UINT:
                    MAS(uint32_t);
                    break;
                case PTL_LONG:
                case PTL_ULONG:
                    MAS(uint64_t);
                    break;
                default:              /* should never happen */
                    *(int *)0 = 0;
            }
            break;
        default:                      /* should never happen */
            *(int *)0 = 0;
    }
}

/* vim:set expandtab: */
