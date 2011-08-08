#ifndef PTL_INTERNAL_FRAGMENTS_H
#define PTL_INTERNAL_FRAGMENTS_H

#include "ptl_visibility.h"

extern size_t SMALL_FRAG_SIZE;
extern size_t SMALL_FRAG_PAYLOAD;
extern size_t SMALL_FRAG_COUNT;
extern size_t LARGE_FRAG_SIZE;
extern size_t LARGE_FRAG_PAYLOAD;
extern size_t LARGE_FRAG_COUNT;

void INTERNAL  PtlInternalFragmentSetup(volatile uint8_t *buf);
void INTERNAL  PtlInternalFragmentInitPid(int pid);
void INTERNAL *PtlInternalFragmentFetch(size_t payload_size);
void INTERNAL  PtlInternalFragmentFree(void *data);
void INTERNAL  PtlInternalFragmentToss(void     *frag,
                                       ptl_pid_t dest);
void INTERNAL *   PtlInternalFragmentReceive(void);
uint64_t INTERNAL PtlInternalFragmentSize(void *frag);

#endif /* ifndef PTL_INTERNAL_FRAGMENTS_H */
/* vim:set expandtab: */
