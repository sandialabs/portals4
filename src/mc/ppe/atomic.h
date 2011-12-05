#include "ppe/ppe.h"

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
