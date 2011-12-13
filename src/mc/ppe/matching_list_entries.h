
#ifndef MC_PPE_MATCHING_LIST_ENTRIES_H
#define MC_PPE_MATCHING_LIST_ENTRIES_H

#include "ppe/nal.h"
int _PtlMEAppend( ptl_ppe_ni_t *, ptl_handle_me_t me_handle,
                         ptl_pt_index_t   pt_index,
                         const ptl_cqe_me_t        *me,
                         ptl_list_t       ptl_list,
                         void            *user_ptr );

int _PtlMEUnlink( ptl_ppe_ni_t *, ptl_handle_me_t me_handle);


int _PtlMESearch( ptl_ppe_ni_t *, int ni,
                         ptl_pt_index_t  pt_index,
                         const ptl_cqe_me_t *me,
                         ptl_search_op_t ptl_search_op,
                         void           *user_ptr);

ptl_pid_t PtlInternalMEDeliver(nal_ctx_t *, ptl_table_entry_t *restrict  t,
                                        ptl_internal_header_t *restrict hdr);


void PtlInternalAnnounceMEDelivery( nal_ctx_t *,
                               const ptl_handle_eq_t             eq_handle,
                               const ptl_handle_ct_t             ct_handle,
                               const unsigned int                options,
                               const uint_fast64_t               mlength,
                               const uintptr_t                   start,
                               const ptl_internal_listtype_t     foundin,
                               void *const                       use_ptr,
                               ptl_internal_header_t *restrict   hdr );

#endif
