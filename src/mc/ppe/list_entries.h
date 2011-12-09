
#ifndef MC_PPE_LIST_ENTRIES_H
#define MC_PPE_LIST_ENTRIES_H

#include <portals4.h>
#include "ppe/ppe.h"
#include "ppe/nal.h"

int 
_PtlLEAppend( ptl_ppe_ni_t *ppe_ni, ptl_handle_le_t le_h,
                         ptl_pt_index_t   pt_index,
                         const ptl_le_t  *le,
                         ptl_list_t       ptl_list,
                         void            *user_ptr );

void PtlInternalAnnounceLEDelivery( nal_ctx_t *nal_ctx,
                              const ptl_handle_eq_t                 eq_handle,
                              const ptl_handle_ct_t                 ct_handle,
                              const unsigned int                    options,
                              const uint_fast64_t                   mlength,
                              const uintptr_t                       start,
                              const ptl_internal_listtype_t         foundin,
                              void *const                           user_ptr,
                              ptl_internal_header_t *const restrict hdr);


int
_PtlLESearch( ptl_ppe_ni_t *ppe_ni, int ni,
                         ptl_pt_index_t  pt_index,
                         const ptl_le_t *le,
                         ptl_search_op_t ptl_search_op,
                         void           *user_ptr);

int
_PtlLEUnlink( ptl_ppe_ni_t *ppe_ni, ptl_handle_le_t le_handle);


ptl_pid_t PtlInternalLEDeliver( nal_ctx_t *,
                    ptl_table_entry_t *restrict     t,     
                                        ptl_internal_header_t *restrict hdr);

#endif
