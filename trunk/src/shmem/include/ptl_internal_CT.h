#ifndef PTL_INTERNAL_CT_H
#define PTL_INTERNAL_CT_H

#include "ptl_visibility.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_trigger.h"
#include "ptl_internal_ints.h"

void INTERNAL PtlInternalCTNISetup(const uint_fast8_t ni,
                                   const ptl_size_t   limit);
void INTERNAL PtlInternalCTNITeardown(const uint_fast8_t ni_num);
int INTERNAL  PtlInternalCTHandleValidator(ptl_handle_ct_t handle,
                                           uint_fast8_t    none_ok);
void INTERNAL PtlInternalCTSuccessInc(ptl_handle_ct_t ct_handle,
                                      ptl_size_t      increment);
void INTERNAL PtlInternalCTFailureInc(ptl_handle_ct_t ct_handle);
void INTERNAL PtlInternalCTFree(ptl_internal_header_t *restrict hdr);
void INTERNAL PtlInternalCTPullTriggers(ptl_handle_ct_t ct_handle);
void INTERNAL PtlInternalCTUnorderedEnqueue(ptl_internal_header_t *restrict hdr);
void INTERNAL PtlInternalCTTriggerCheck(ptl_handle_ct_t ct);
void INTERNAL PtlInternalAddTrigger(ptl_handle_ct_t         ct_handle,
                                    ptl_internal_trigger_t *t);

ptl_internal_trigger_t INTERNAL *PtlInternalFetchTrigger(const uint_fast8_t ni);

#endif /* ifndef PTL_INTERNAL_CT_H */
/* vim:set expandtab: */
