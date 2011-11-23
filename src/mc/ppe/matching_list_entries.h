int _PtlMEAppend( ptl_handle_me_t me_handle,
                         ptl_pt_index_t   pt_index,
                         ptl_me_t        *me,
                         ptl_list_t       ptl_list,
                         void            *user_ptr );

int _PtlMEUnlink(ptl_handle_me_t me_handle);


int _PtlMESearch(ptl_handle_ni_t ni_handle,
                         ptl_pt_index_t  pt_index,
                         const ptl_me_t *me,
                         ptl_search_op_t ptl_search_op,
                         void           *user_ptr);

ptl_pid_t PtlInternalMEDeliver(ptl_table_entry_t *restrict     t,
                                        ptl_internal_header_t *restrict hdr);


