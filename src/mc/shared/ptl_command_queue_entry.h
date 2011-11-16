#ifndef MC_COMMAND_QUEUE_ENTRY_H
#define MC_COMMAND_QUEUE_ENTRY_H

#include <xpmem.h>

#include "portals4.h"
#include "ptl_internal_handles.h"

struct ptl_circular_buffer_t;

typedef enum {
    PTLPROCATTACH = 1,
    PTLNIINIT,
    PTLNIFINI,
    PTLPTALLOC,
    PTLPTFREE,
    PTLSETMAP,
    PTLGETMAP,
    PTLMDBIND,
    PTLMDRELEASE,
    PTLMEAPPEND,
    PTLMEUNLINK,
    PTLMESEARCH,
    PTLGETID,
    PTLCTALLOC,
    PTLCTFREE,
    PTLCTWAIT,
    PTLCTSET,
    PTLCTINC,
    PTLCTCANCELTRIGGERED,
    PTLPUT,
    PTLGET,
    PTLEQALLOC,
    PTLEQFREE,
    PTLLEAPPEND,
    PTLLEUNLINK,
    PTLLESEARCH,
    PTLATOMIC,
    PTLFETCHATOMIC,
    PTLSWAP,
    PTLATOMICSYNC,
    PTLACK
} ptl_cmd_type_t;

struct ptl_cqe_base_t {
    unsigned char type      : 8;
    unsigned int  remote_id : 24;
};
typedef struct ptl_cqe_base_t ptl_cqe_base_t;

typedef struct {
    ptl_cqe_base_t base;
    int proc_id;
    xpmem_segid_t segid;
} ptl_cqe_proc_attach_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t ni_handle;
    ptl_pid_t       pid; 
    ptl_ni_limits_t *limits;
    void           *shared_data;
    long            shared_data_length;
    int             phys_addr;
    int             status_reg;
    int             les;
    int             mds;
    int             mes;
    int             cts;
    int             eqs;
    int             pts;
    int            *retval_ptr;
} ptl_cqe_niinit_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ni_handle;
    int            *retval_ptr;
} ptl_cqe_nifini_t;

typedef struct {
    ptl_cqe_base_t  base;
    ptl_internal_handle_converter_t     ni_handle;
    const ptl_process_t  *mapping;
    ptl_size_t      mapping_len;
    int            *retval_ptr;
} ptl_cqe_setmap_t;

typedef struct {
    ptl_cqe_base_t  base;
    ptl_internal_handle_converter_t     ni_handle;
    ptl_process_t  *mapping;
    ptl_size_t     *mapping_len;
    int            *retval_ptr;
} ptl_cqe_getmap_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ct_handle; 
} ptl_cqe_ctalloc_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ct_handle; 
} ptl_cqe_ctfree_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ct_handle; 
    ptl_ct_event_t  new_ct;
} ptl_cqe_ctset_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ct_handle; 
    ptl_ct_event_t  increment;
} ptl_cqe_ctinc_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t ct_handle;
} ptl_cqe_ctcanceltriggered_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     eq_handle; 
    struct ptl_circular_buffer_t *cb;
} ptl_cqe_eqalloc_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     eq_handle; 
    int            *retval_ptr;
} ptl_cqe_eqfree_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     md_handle; 
    ptl_md_t        md;
} ptl_cqe_mdbind_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     md_handle; 
} ptl_cqe_mdrelease_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ni_handle;
    unsigned int    options;
    ptl_internal_handle_converter_t     eq_handle;
    ptl_pt_index_t  pt_index;
} ptl_cqe_ptalloc_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ni_handle;
    ptl_pt_index_t  pt_index;
} ptl_cqe_ptfree_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t      md_handle;    
    ptl_size_t       local_offset;
    ptl_size_t       length;
    ptl_ack_req_t    ack_req;
    ptl_process_t    target_id;
    ptl_pt_index_t   pt_index;
    ptl_match_bits_t match_bits; 
    ptl_size_t       remote_offset;
    void            *user_ptr;
    ptl_hdr_data_t   hdr_data;
} ptl_cqe_put_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t      md_handle;    
    ptl_size_t       local_offset;
    ptl_size_t       length;
    ptl_process_t    target_id;
    ptl_pt_index_t   pt_index;
    ptl_match_bits_t match_bits; 
    ptl_size_t       remote_offset;
    void            *user_ptr;
} ptl_cqe_get_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     md_handle;
    ptl_pt_index_t  pt_index;
    ptl_me_t        me;
    ptl_list_t      list;
    void           *user_ptr;
} ptl_cqe_meappend_t ;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     me_handle;
} ptl_cqe_meunlink_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ni_handle;
    ptl_pt_index_t  pt_index;
    ptl_me_t        me;
    ptl_search_op_t ptl_search_op;
    void           *user_ptr;
} ptl_cqe_mesearch_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     le_handle;
    ptl_pt_index_t  pt_index;
    ptl_le_t        le;
    ptl_list_t      list;
    void           *user_ptr;
} ptl_cqe_leappend_t ;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     le_handle;
} ptl_cqe_leunlink_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t     ni_handle;
    ptl_pt_index_t  pt_index;
    ptl_le_t        le;
    ptl_search_op_t ptl_search_op;
    void           *user_ptr;
} ptl_cqe_lesearch_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t      md_handle;
    ptl_size_t       local_offset;
    ptl_size_t       length;
    ptl_ack_req_t    ack_req;
    ptl_process_t    target_id;
    ptl_pt_index_t   pt_index;
    ptl_match_bits_t match_bits;
    ptl_size_t       remote_offset;
    void            *user_ptr;
    ptl_hdr_data_t   hdr_data;
    ptl_op_t         operation;
    ptl_datatype_t   datatype;
} ptl_cqe_atomic_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t      get_md_handle;
    ptl_size_t       local_get_offset;
    ptl_internal_handle_converter_t      put_md_handle;
    ptl_size_t       local_put_offset;
    ptl_size_t       length;
    ptl_process_t    target_id;
    ptl_pt_index_t   pt_index;
    ptl_match_bits_t match_bits;
    ptl_size_t       remote_offset;
    void            *user_ptr;
    ptl_hdr_data_t   hdr_data;
    ptl_op_t         operation;
    ptl_datatype_t   datatype;
} ptl_cqe_fetchatomic_t;

typedef struct {
    ptl_cqe_base_t base;
    ptl_internal_handle_converter_t      get_md_handle;
    ptl_size_t       local_get_offset;
    ptl_internal_handle_converter_t      put_md_handle;
    ptl_size_t       local_put_offset;
    ptl_size_t       length;
    ptl_process_t    target_id;
    ptl_pt_index_t   pt_index;
    ptl_match_bits_t match_bits;
    ptl_size_t       remote_offset;
    void            *user_ptr;
    ptl_hdr_data_t   hdr_data;
    const void      *operand;
    ptl_op_t         operation;
    ptl_datatype_t   datatype;
} ptl_cqe_swap_t;

typedef struct {
    ptl_cqe_base_t base;
} ptl_cqe_atomicsync_t;

typedef struct {
    ptl_cqe_base_t base;
    int *retval_ptr;
    int retval;
} ptl_cqe_ack_t;

union ptl_cqe_t {
    ptl_cqe_base_t        base;
    ptl_cqe_proc_attach_t procAttach;
    ptl_cqe_niinit_t      niInit;
    ptl_cqe_nifini_t      niFini;
    ptl_cqe_setmap_t      setMap;
    ptl_cqe_getmap_t      getMap;
    ptl_cqe_ctalloc_t     ctAlloc;
    ptl_cqe_ctfree_t      ctFree;
    ptl_cqe_ctset_t       ctSet;
    ptl_cqe_ctinc_t       ctInc;
    ptl_cqe_ctcanceltriggered_t ctCancelTriggered;
    ptl_cqe_eqalloc_t     eqAlloc;
    ptl_cqe_eqfree_t      eqFree;
    ptl_cqe_mdbind_t      mdBind;
    ptl_cqe_mdrelease_t   mdRelease;
    ptl_cqe_put_t         put;
    ptl_cqe_get_t         get;
    ptl_cqe_ptalloc_t     ptAlloc;
    ptl_cqe_ptfree_t      ptFree;
    ptl_cqe_meappend_t    meAppend;
    ptl_cqe_meunlink_t    meUnlink;
    ptl_cqe_mesearch_t    meSearch;
    ptl_cqe_leappend_t    leAppend;
    ptl_cqe_leunlink_t    leUnlink;
    ptl_cqe_lesearch_t    leSearch;
    ptl_cqe_atomic_t      atomic;
    ptl_cqe_fetchatomic_t fetchAtomic;
    ptl_cqe_swap_t        swap;
    ptl_cqe_atomicsync_t  atomicSync;
    ptl_cqe_ack_t         ack;
};

#endif
