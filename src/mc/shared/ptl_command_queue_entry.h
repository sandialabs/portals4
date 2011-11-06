#ifndef MC_COMMAND_QUEUE_ENTRY_H
#define MC_COMMAND_QUEUE_ENTRY_H

#include "portals4.h"
#include "ptl_internal_handles.h"

typedef enum {
    PTLNINIT = 1,   
    PTLNIFINI,
    PTLPTALLOC,
    PTLPTFREE,
    PTLMDBIND,
    PTLMDRELEASE,
    PTLMEAPPEND,
    PTLMEUNLINK,
    PTLGETID,
    PTLCTALLOC,
    PTLCTFREE,
    PTLCTWAIT,
    PTLPUT,
    PTLGET,
    PTLTRIGGET,
    PTLEQALLOC,
    PTLEQFREE
} ptl_cmd_type_t;

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

typedef ptl_internal_handle_converter_t cmdHandle_t;
typedef unsigned long cmdAddr_t;

typedef struct {
    ptl_pid_t pid; 
    unsigned int options;
    cmdHandle_t ni_handle;
} cmdPtlNIInit_t;

typedef struct {
    cmdHandle_t ni_handle;
} cmdPtlNIFini_t;

typedef struct {
    cmdHandle_t handle; 
    ptl_ct_event_t*   addr;
} cmdPtlCTAlloc_t;

typedef struct {
    cmdHandle_t handle; 
} cmdPtlCTFree_t;

typedef struct {
    cmdHandle_t handle; 
    //PtlEventInternal*   addr;
    ptl_size_t          size;
} cmdPtlEQAlloc_t;

typedef struct {
    cmdHandle_t handle; 
} cmdPtlEQFree_t;

typedef struct {
    cmdHandle_t md_handle; 
    ptl_md_t md;
} cmdPtlMDBind_t;

typedef struct {
    cmdHandle_t md_handle; 
} cmdPtlMDRelease_t;

typedef struct {
    unsigned int options;
    cmdHandle_t eq_handle;
    ptl_pt_index_t pt_index;
} cmdPtlPTAlloc_t;

typedef struct {
    ptl_pt_index_t pt_index;
} cmdPtlPTFree_t;


typedef struct {
    cmdHandle_t md_handle;    
    ptl_size_t   local_offset;
    ptl_size_t   length;
    ptl_ack_req_t         ack_req;
    ptl_process_t    target_id;
    ptl_pt_index_t         pt_index;
    ptl_match_bits_t match_bits; 
    ptl_size_t       remote_offset;
    void*           user_ptr;
    ptl_hdr_data_t  hdr_data;
} cmdPtlPut_t;

typedef struct {
    cmdHandle_t md_handle;    
    ptl_size_t   local_offset;
    ptl_size_t   length;
    ptl_process_t    target_id;
    ptl_pt_index_t         pt_index;
    ptl_match_bits_t match_bits; 
    ptl_size_t       remote_offset;
    void*           user_ptr;
    ptl_hdr_data_t  hdr_data;
    ptl_handle_ct_t trig_ct_handle;
    ptl_size_t      threshold;
} cmdPtlGet_t;

typedef cmdPtlGet_t cmdPtlTrigGet_t;


typedef struct {
    cmdHandle_t handle;
    ptl_pt_index_t pt_index;
    ptl_me_t    me;
    ptl_list_t  list;
    void* user_ptr;
} cmdPtlMEAppend_t ;

typedef struct {
    cmdHandle_t handle;
} cmdPtlMEUnlink_t ;

typedef union {
    cmdPtlNIInit_t      niInit;
    cmdPtlNIFini_t      niFini;
    cmdPtlCTAlloc_t     ctAlloc;
    cmdPtlCTFree_t      ctFree;
    cmdPtlEQAlloc_t     eqAlloc;
    cmdPtlEQFree_t      eqFree;
    cmdPtlMDBind_t      mdBind;
    cmdPtlMDRelease_t   mdRelease;
    cmdPtlPut_t         ptlPut;
    cmdPtlGet_t         ptlGet;
    cmdPtlGet_t         ptlTrigGet;
    cmdPtlPTAlloc_t     ptAlloc;
    cmdPtlPTFree_t      ptFree;
    cmdPtlMEAppend_t    meAppend;
    cmdPtlMEUnlink_t    meUnlink;
} ptl_cmd_union_t;

struct ptl_cqe_base_t {
    char type; 
    ptl_cmd_union_t u;
};

typedef struct ptl_cqe_base_t ptl_cqe_base_t;

#endif
