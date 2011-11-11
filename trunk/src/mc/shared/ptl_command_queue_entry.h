#ifndef MC_COMMAND_QUEUE_ENTRY_H
#define MC_COMMAND_QUEUE_ENTRY_H

#include "portals4.h"
#include "ptl_internal_handles.h"

typedef enum {
    PTLNIINIT = 1,   
    PTLNIFINI,
    PTLPTALLOC,
    PTLPTFREE,
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
    PTLPUT,
    PTLGET,
    PTLTRIGGET,
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

#if 0
#define CMD_NAMES {\
    "",\
    "PtlNIInit",\
    "PtlNIFini",\
    "PtlPTAlloc",\
    "PtlPTFree",\
    "PtlMDBind",\
    "PtlMDRelease",\
    "PtlMEAppend",\
    "PtlMEUnlink",\
    "PtlGetId",\
    "PtlCTAlloc",\
    "PtlCTFree",\
    "PtlCTWait",\
    "PtlPut",\
    "PtlGet",\
    "PtlTrigGet",\
    "PtlEQAlloc",\
    "PtlEQFree",\
    "ContextInit",\
    "ContextFini"\
}
#endif

typedef ptl_internal_handle_converter_t cmdHandle_t;
typedef unsigned long cmdAddr_t;

typedef struct {
    ptl_internal_handle_converter_t ni_handle;
    unsigned int    options;
    ptl_pid_t       pid; 
    ptl_ni_limits_t *limits;
    void            *lePtr;
    void            *mdPtr;
    void            *mePtr;
    void            *ctPtr;
    void            *eqPtr;
    void            *ptPtr;
    int             *retval_ptr;
} cmdPtlNIInit_t;

typedef struct {
    cmdHandle_t     ni_handle;
    int            *retval_ptr;
} cmdPtlNIFini_t;


typedef struct {
    cmdHandle_t     ct_handle; 
} cmdPtlCTAlloc_t;

typedef struct {
    cmdHandle_t     ct_handle; 
} cmdPtlCTFree_t;

typedef struct {
    cmdHandle_t     ct_handle; 
    ptl_ct_event_t  new_ct;
} cmdPtlCTSet_t;

typedef struct {
    cmdHandle_t     ct_handle; 
    ptl_ct_event_t  increment;
} cmdPtlCTInc_t;

typedef struct {
    cmdHandle_t     eq_handle; 
    ptl_size_t      count;
} cmdPtlEQAlloc_t;

typedef struct {
    cmdHandle_t     eq_handle; 
} cmdPtlEQFree_t;

typedef struct {
    cmdHandle_t     md_handle; 
    ptl_md_t        md;
} cmdPtlMDBind_t;

typedef struct {
    cmdHandle_t     md_handle; 
} cmdPtlMDRelease_t;

typedef struct {
    cmdHandle_t     ni_handle;
    unsigned int    options;
    cmdHandle_t     eq_handle;
    ptl_pt_index_t  pt_index;
} cmdPtlPTAlloc_t;

typedef struct {
    cmdHandle_t     ni_handle;
    ptl_pt_index_t  pt_index;
} cmdPtlPTFree_t;


typedef struct {
    cmdHandle_t      md_handle;    
    ptl_size_t       local_offset;
    ptl_size_t       length;
    ptl_ack_req_t    ack_req;
    ptl_process_t    target_id;
    ptl_pt_index_t   pt_index;
    ptl_match_bits_t match_bits; 
    ptl_size_t       remote_offset;
    void            *user_ptr;
    ptl_hdr_data_t   hdr_data;
} cmdPtlPut_t;

typedef struct {
    cmdHandle_t      md_handle;    
    ptl_size_t       local_offset;
    ptl_size_t       length;
    ptl_process_t    target_id;
    ptl_pt_index_t   pt_index;
    ptl_match_bits_t match_bits; 
    ptl_size_t       remote_offset;
    void            *user_ptr;
} cmdPtlGet_t;

typedef cmdPtlGet_t cmdPtlTrigGet_t;


typedef struct {
    cmdHandle_t     md_handle;
    ptl_pt_index_t  pt_index;
    ptl_me_t        me;
    ptl_list_t      list;
    void           *user_ptr;
} cmdPtlMEAppend_t ;

typedef struct {
    cmdHandle_t     me_handle;
} cmdPtlMEUnlink_t;

typedef struct {
    cmdHandle_t     ni_handle;
    ptl_pt_index_t  pt_index;
    ptl_me_t        me;
    ptl_search_op_t ptl_search_op;
    void           *user_ptr;
} cmdPtlMESearch_t;

typedef struct {
    cmdHandle_t     le_handle;
    ptl_pt_index_t  pt_index;
    ptl_le_t        le;
    ptl_list_t      list;
    void           *user_ptr;
} cmdPtlLEAppend_t ;

typedef struct {
    cmdHandle_t     le_handle;
} cmdPtlLEUnlink_t;

typedef struct {
    cmdHandle_t     ni_handle;
    ptl_pt_index_t  pt_index;
    ptl_le_t        le;
    ptl_search_op_t ptl_search_op;
    void           *user_ptr;
} cmdPtlLESearch_t;


typedef struct {
    cmdHandle_t      md_handle;
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
} cmdPtlAtomic_t;

typedef struct {
    cmdHandle_t      get_md_handle;
    ptl_size_t       local_get_offset;
    cmdHandle_t      put_md_handle;
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
} cmdPtlFetchAtomic_t;

typedef struct {
    cmdHandle_t      get_md_handle;
    ptl_size_t       local_get_offset;
    cmdHandle_t      put_md_handle;
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
} cmdPtlSwap_t;

typedef struct {
    int my_id;
} cmdPtlAtomicSync_t;

typedef struct {
    int *retval_ptr;
    int retval;
} cmdPtlAck_t;

typedef union {
    cmdPtlNIInit_t      niInit;
    cmdPtlNIFini_t      niFini;
    cmdPtlCTAlloc_t     ctAlloc;
    cmdPtlCTFree_t      ctFree;
    cmdPtlCTSet_t       ctSet;
    cmdPtlCTInc_t       ctInc;
    cmdPtlEQAlloc_t     eqAlloc;
    cmdPtlEQFree_t      eqFree;
    cmdPtlMDBind_t      mdBind;
    cmdPtlMDRelease_t   mdRelease;
    cmdPtlPut_t         put;
    cmdPtlGet_t         get;
    cmdPtlGet_t         trigGet;
    cmdPtlPTAlloc_t     ptAlloc;
    cmdPtlPTFree_t      ptFree;
    cmdPtlMEAppend_t    meAppend;
    cmdPtlMEUnlink_t    meUnlink;
    cmdPtlMESearch_t    meSearch;
    cmdPtlLEAppend_t    leAppend;
    cmdPtlLEUnlink_t    leUnlink;
    cmdPtlLESearch_t    leSearch;
    cmdPtlAtomic_t      atomic;
    cmdPtlFetchAtomic_t      fetchAtomic;
    cmdPtlSwap_t        swap;
    cmdPtlAtomicSync_t  atomicSync;
    cmdPtlAck_t  ack;
} ptl_cmd_union_t;

struct ptl_cqe_t {
    char type; 
    ptl_cmd_union_t u;
};

#endif
