#ifndef PTL_INTERNAL_PERFORMATOMIC_H
#define PTL_INTERNAL_PERFORMATOMIC_H

void PtlInternalPerformAtomic(
    char *         dest,
    char *         src,
    ptl_size_t     size,
    ptl_op_t       op,
    ptl_datatype_t dt);

void PtlInternalPerformAtomicArg(
    char *         dest,
    char *         src,
    uint64_t       operand,
    ptl_size_t     size,
    ptl_op_t       op,
    ptl_datatype_t dt);

#endif /* ifndef PTL_INTERNAL_PERFORMATOMIC_H */
/* vim:set expandtab: */
