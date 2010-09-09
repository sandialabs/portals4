#ifndef PTL_INTERNAL_LE_H
#define PTL_INTERNAL_LE_H

void PtlInternalLENISetup(
    unsigned int ni,
    ptl_size_t limit);

void PtlInternalLENITeardown(
    unsigned int ni);

int PtlInternalLEDeliver(
    ptl_table_entry_t * restrict t,
    ptl_internal_header_t * restrict h);

#endif
