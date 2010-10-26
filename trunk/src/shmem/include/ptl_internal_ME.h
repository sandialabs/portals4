#ifndef PTL_INTERNAL_ME_H
#define PTL_INTERNAL_ME_H

#include "ptl_internal_PT.h"
#include "ptl_internal_commpad.h"

void PtlInternalMENISetup(
    unsigned int ni,
    ptl_size_t limit);

void PtlInternalMENITeardown(
    unsigned int ni);

ptl_pid_t PtlInternalMEDeliver(
    ptl_table_entry_t * restrict t,
    ptl_internal_header_t * restrict h);

#endif
/* vim:set expandtab */
