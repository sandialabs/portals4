#ifndef PTL_INTERNAL_PERFORMATOMIC_H
#define PTL_INTERNAL_PERFORMATOMIC_H

void PtlInternalPerformAtomic(uint8_t *restrict dest,
                              uint8_t *restrict src,
                              ptl_size_t        size,
                              ptl_op_t          op,
                              ptl_datatype_t    dt);

void PtlInternalPerformAtomicArg(uint8_t *restrict dest,
                                 uint8_t *restrict src,
                                 uint8_t           operand[32],
                                 ptl_size_t        size,
                                 ptl_op_t          op,
                                 ptl_datatype_t    dt);

#endif /* ifndef PTL_INTERNAL_PERFORMATOMIC_H */
/* vim:set expandtab ft=c: */
