/**
 * @file ptl_atomic.h
 *
 * This file contains declarations from ptl_atomic.c
 */

#ifndef PTL_ATOMIC_H
#define PTL_ATOMIC_H

/**
 * Union of portals data types.
 */
union datatype {
    int8_t s8;
    uint8_t u8;
    int16_t s16;
    uint16_t u16;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
    float f;
    float complex fc;
    double d;
    double complex dc;
    long double ld;
    long double complex ldc;
};

typedef union datatype datatype_t;

typedef int (*atom_op_t) (void *src, void *dst, ptl_size_t length);

extern atom_op_t atom_op[PTL_OP_LAST][PTL_DATATYPE_LAST];

/*
 * Useful information about atomic operations.
 */
struct atom_op_info {
    int float_ok;
    int complex_ok;
    int atomic_ok;
    int swap_ok;
    int use_operand;
};

extern struct atom_op_info op_info[PTL_OP_LAST];

extern int atom_type_size[PTL_DATATYPE_LAST];

int swap_data_in(ptl_op_t atom_op, ptl_datatype_t atom_type, void *dest,
                 void *source, datatype_t *operand);

#endif /* PTL_ATOMIC_H */
