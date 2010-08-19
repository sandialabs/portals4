#include <portals4.h>

/* System headers */
#include <stdlib.h> /* for abort() */

/* Internal headers */
#include "ptl_internal_performatomic.h"
#include "ptl_visibility.h"

#define ADD_OP(type,r,a,b) r = (type)((a) + (b))
#define PROD_OP(type,r,a,b) r = (type)((a) * (b))
#define MAX_OP(type,r,a,b) r = (type)(((a) > (b))?(a):(b))
#define MIN_OP(type,r,a,b) r = (type)(((a) < (b))?(a):(b))
#define LOR_OP(type,r,a,b) r = (type)(((a) || (b))?(1):(0))
#define LAND_OP(type,r,a,b) r = (type)(((a) && (b))?(1):(0))
#define LXOR_OP(type,r,a,b) r = (type)(((a) || (b))?(0):(1))
#define BOR_OP(type,r,a,b) r = (type)((a) | (b))
#define BAND_OP(type,r,a,b) r = (type)((a) & (b))
#define BXOR_OP(type,r,a,b) r = (type)((a) ^ (b))
#define NONBUILTIN_INT_CAS(Type, Op) do { \
    Type first, second; \
    do { \
	first = *(volatile Type*)dest; \
	Op(Type, second, first, *(Type*)src); \
    } while (__sync_bool_compare_and_swap((volatile Type*)dest, first, second)); \
    *(Type*)src = second; \
} while (0)
#define NONBUILTIN_CAS(Type, EqIntType, Op) do { \
    union { Type t; EqIntType i; } first, second; \
    do { \
	first.i = *(volatile EqIntType*)dest; \
	Op(Type, second.t, first.t, *(Type*)src); \
    } while (__sync_bool_compare_and_swap((volatile EqIntType*)dest, first.i, second.i)); \
    *(Type*)src = second.t; \
} while (0)
#define PERFORM_UNIVERSAL_DATATYPE_FUNC(fname,op) static void inline PtlInternalPerformAtomic##fname( \
    volatile char *dest, \
    char *src, \
    ptl_datatype_t dt) \
{ \
    switch (dt) { \
	case PTL_CHAR:   NONBUILTIN_INT_CAS(   int8_t, op); break; \
	case PTL_UCHAR:  NONBUILTIN_INT_CAS(  uint8_t, op); break; \
	case PTL_SHORT:  NONBUILTIN_INT_CAS(  int16_t, op); break; \
	case PTL_USHORT: NONBUILTIN_INT_CAS( uint16_t, op); break; \
	case PTL_INT:    NONBUILTIN_INT_CAS(  int32_t, op); break; \
	case PTL_UINT:   NONBUILTIN_INT_CAS( uint32_t, op); break; \
	case PTL_LONG:   NONBUILTIN_INT_CAS(  int64_t, op); break; \
	case PTL_ULONG:  NONBUILTIN_INT_CAS( uint64_t, op); break; \
	case PTL_FLOAT:  NONBUILTIN_CAS(   float, uint32_t, op); break; \
	case PTL_DOUBLE: NONBUILTIN_CAS(  double, uint64_t, op); break; \
    } \
}
#define PERFORM_INTEGER_DATATYPE_FUNC(fname,op) static void inline PtlInternalPerformAtomic##fname( \
    volatile char *dest, \
    char *src, \
    ptl_datatype_t dt) \
{ \
    switch (dt) { \
	case PTL_CHAR:   NONBUILTIN_INT_CAS(   int8_t, op); break; \
	case PTL_UCHAR:  NONBUILTIN_INT_CAS(  uint8_t, op); break; \
	case PTL_SHORT:  NONBUILTIN_INT_CAS(  int16_t, op); break; \
	case PTL_USHORT: NONBUILTIN_INT_CAS( uint16_t, op); break; \
	case PTL_INT:    NONBUILTIN_INT_CAS(  int32_t, op); break; \
	case PTL_UINT:   NONBUILTIN_INT_CAS( uint32_t, op); break; \
	case PTL_LONG:   NONBUILTIN_INT_CAS(  int64_t, op); break; \
	case PTL_ULONG:  NONBUILTIN_INT_CAS( uint64_t, op); break; \
	default: abort(); \
    } \
}


PERFORM_UNIVERSAL_DATATYPE_FUNC(Min, MIN_OP)
PERFORM_UNIVERSAL_DATATYPE_FUNC(Max, MAX_OP)
PERFORM_UNIVERSAL_DATATYPE_FUNC(Prod,PROD_OP)
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

static void inline PtlInternalPerformAtomicSum(
    volatile char *dest,
    char *src,
    ptl_datatype_t dt)
{
    switch (dt) {
	case PTL_CHAR:   INT_BUILTIN(  int8_t, __sync_fetch_and_add); break;
	case PTL_UCHAR:  INT_BUILTIN( uint8_t, __sync_fetch_and_add); break;
	case PTL_SHORT:  INT_BUILTIN( int16_t, __sync_fetch_and_add); break;
	case PTL_USHORT: INT_BUILTIN(uint16_t, __sync_fetch_and_add); break;
	case PTL_INT:    INT_BUILTIN( int32_t, __sync_fetch_and_add); break;
	case PTL_UINT:   INT_BUILTIN(uint32_t, __sync_fetch_and_add); break;
	case PTL_LONG:   INT_BUILTIN( int64_t, __sync_fetch_and_add); break;
	case PTL_ULONG:  INT_BUILTIN(uint64_t, __sync_fetch_and_add); break;
	case PTL_FLOAT:  NONBUILTIN_CAS( float, uint32_t, ADD_OP); break;
	case PTL_DOUBLE: NONBUILTIN_CAS(double, uint64_t, ADD_OP); break;
    }
}

#define BUILTINSWAP(int_type) do { \
    int_type before = *(volatile int_type *)dest; \
    *(int_type*)src = __sync_val_compare_and_swap((volatile int_type*)dest, before, *(int_type*)src); \
} while (0)
#define BUILTINSWAPF(f_type, i_type) do { \
    union { f_type f; i_type i; } before, after; \
    after.f = *(volatile f_type *)src; \
    do { \
	before.f = *(volatile f_type *)dest; \
    } while (__sync_bool_compare_and_swap((volatile i_type*)dest, before.i, after.i)); \
} while (0)
static void inline PtlInternalPerformAtomicSwap(
    volatile char *dest,
    char *src,
    ptl_datatype_t dt)
{
    switch (dt) {
	case PTL_CHAR:   BUILTINSWAP(int8_t); break;
	case PTL_UCHAR:  BUILTINSWAP(uint8_t); break;
	case PTL_SHORT:  BUILTINSWAP(int16_t); break;
	case PTL_USHORT: BUILTINSWAP(uint16_t); break;
	case PTL_INT:    BUILTINSWAP(int32_t); break;
	case PTL_UINT:   BUILTINSWAP(uint32_t); break;
	case PTL_LONG:   BUILTINSWAP(int64_t); break;
	case PTL_ULONG:  BUILTINSWAP(uint64_t); break;
	case PTL_FLOAT:  BUILTINSWAPF(float, uint32_t); break;
	case PTL_DOUBLE: BUILTINSWAPF(double, uint64_t); break;
    }
}

#define CAS(type) do { \
    type first, second; \
    second = *(type*)&operand; \
    first = __sync_val_compare_and_swap((volatile type*)dest, second, *(type*)src); \
    *(type*)src = first; \
} while (0)
#define MAS(type) do { \
    type curv, newv; \
    const type mask = *(type*)&operand; \
    const type srcv = *(type*)src; \
    do { \
	curv = *(volatile type*)dest; \
	newv = (type)((curv & ~mask) | (srcv & mask)); \
    } while (__sync_bool_compare_and_swap((volatile type*)dest, curv, newv)); \
    *(type*)src = curv; \
} while (0)


static void inline PtlInternalPerformAtomicCswap(
    volatile char *dest,
    char *src,
    uint64_t operand,
    ptl_datatype_t dt)
{
    switch (dt) {
	case PTL_CHAR:
	case PTL_UCHAR:
	    CAS(uint8_t);
	    break;
	case PTL_SHORT:
	case PTL_USHORT:
	    CAS(uint16_t);
	    break;
	case PTL_INT:
	case PTL_UINT:
	case PTL_FLOAT:
	    CAS(uint32_t);
	    break;
	case PTL_LONG:
	case PTL_ULONG:
	case PTL_DOUBLE:
	    CAS(uint64_t);
	    break;
    }
}

static void inline PtlInternalPerformAtomicMswap(
    volatile char *dest,
    char *src,
    uint64_t operand,
    ptl_datatype_t dt)
{
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
    }
}

static unsigned char datatype_size_table[] = {1,1,2,2,4,4,8,8,4,8};

void INTERNAL PtlInternalPerformAtomic(
    char *dest,
    char *src,
    ptl_size_t size,
    ptl_op_t op,
    ptl_datatype_t dt)
{
    ptl_size_t sz = datatype_size_table[dt];
    if (sz == size) {
	switch (op) {
	    case PTL_MIN:  PtlInternalPerformAtomicMin( dest, src, dt); break;
	    case PTL_MAX:  PtlInternalPerformAtomicMax( dest, src, dt); break;
	    case PTL_SUM:  PtlInternalPerformAtomicSum( dest, src, dt); break;
	    case PTL_PROD: PtlInternalPerformAtomicProd(dest, src, dt); break;
	    case PTL_LOR:  PtlInternalPerformAtomicLor( dest, src, dt); break;
	    case PTL_BOR:  PtlInternalPerformAtomicBor( dest, src, dt); break;
	    case PTL_LAND: PtlInternalPerformAtomicLand(dest, src, dt); break;
	    case PTL_BAND: PtlInternalPerformAtomicBand(dest, src, dt); break;
	    case PTL_LXOR: PtlInternalPerformAtomicLxor(dest, src, dt); break;
	    case PTL_BXOR: PtlInternalPerformAtomicBxor(dest, src, dt); break;
	    case PTL_SWAP: PtlInternalPerformAtomicSwap(dest, src, dt); break;
	    default:
		abort();
	}
    } else {
#warning Handling PtlAtomic with non-unit sizes is unimplemented
	abort();
    }
}

void INTERNAL PtlInternalPerformAtomicArg(
	char *dest,
	char *src,
	uint64_t operand,
	ptl_size_t size,
	ptl_op_t op,
	ptl_datatype_t dt)
{
    switch (op) {
	case PTL_SWAP:
	    if (size == datatype_size_table[dt]) {
		PtlInternalPerformAtomicSwap(dest, src, dt); break;
	    } else {
#warning Handling PtlSwap with PTL_SWAP and non-unit sizes is unimplemented
		abort();
	    }
	    break;
	case PTL_CSWAP: PtlInternalPerformAtomicCswap(dest, src, operand, dt); break;
	case PTL_MSWAP: PtlInternalPerformAtomicMswap(dest, src, operand, dt); break;
    }
}
