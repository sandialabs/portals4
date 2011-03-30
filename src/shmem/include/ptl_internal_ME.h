#ifndef PTL_INTERNAL_ME_H
#define PTL_INTERNAL_ME_H

#include "ptl_internal_PT.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_ints.h"

void INTERNAL PtlInternalMENISetup(const uint_fast8_t ni,
                                   const ptl_size_t   limit);
void INTERNAL      PtlInternalMENITeardown(const uint_fast8_t ni);
ptl_pid_t INTERNAL PtlInternalMEDeliver(ptl_table_entry_t *restrict     t,
                                        ptl_internal_header_t *restrict h);

#endif /* ifndef PTL_INTERNAL_ME_H */
/* vim:set expandtab: */
