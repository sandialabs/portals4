#ifndef PTL_INTERNAL_LE_H
#define PTL_INTERNAL_LE_H

#include "ptl_internal_PT.h"
#include "ptl_internal_commpad.h"

void INTERNAL      PtlInternalLENISetup(unsigned int ni,
                                        ptl_size_t limit);
void INTERNAL      PtlInternalLENITeardown(unsigned int ni);
ptl_pid_t INTERNAL PtlInternalLEDeliver(ptl_table_entry_t * restrict t,
                                        ptl_internal_header_t * restrict h);

#endif /* ifndef PTL_INTERNAL_LE_H */
/* vim:set expandtab: */
