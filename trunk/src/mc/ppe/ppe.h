#ifndef MC_PPE_PPE_H
#define MC_PPE_PPE_H

#include <stdio.h>
#include <xpmem.h>

#include "portals4.h"

#include "shared/ptl_double_list.h"
#include "shared/ptl_command_queue.h"
#include "shared/ptl_connection_manager.h"
#include "shared/ptl_internal_handles.h"
#include "ppe/ppe_xpmem.h"

#include "lib/include/ptl_internal_MD.h"
#include "lib/include/ptl_internal_ME.h"
#include "lib/include/ptl_internal_LE.h"
#include "lib/include/ptl_internal_CT.h"
#include "lib/include/ptl_internal_EQ.h"
#include "lib/include/ptl_internal_PT.h"


#define PPE_DBG( fmt, args...) \
fprintf(stderr,"%s():%i: " fmt, __FUNCTION__, __LINE__, ## args);

/* MJL, fix p3 headers so we don't have to include all of these for lib_ni_t */
#include <limits.h>
#include "nal/p3.3/include/p3/lock.h"
#include "nal/p3.3/include/p3/handle.h"
#include "nal/p3.3/include/p3api/types.h"
#include "nal/p3.3/include/p3lib/types.h"
#include "nal/p3.3/include/p3lib/nal.h"

typedef ptl_internal_handle_converter_t ptl_handle_generic_t;

struct ptl_ppe_eq_t {
    ptl_ppe_xpmem_ptr_t    *xpmem_ptr;
};
typedef struct ptl_ppe_eq_t ptl_ppe_eq_t;

struct ptl_ppe_md_t {
    int                  ref_cnt;
    ptl_ppe_xpmem_ptr_t *xpmem_ptr;
    unsigned int         options;
    ptl_handle_generic_t eq_h;
    ptl_handle_generic_t ct_h;
};
typedef struct ptl_ppe_md_t ptl_ppe_md_t;

typedef enum {PRIORITY, OVERFLOW} ptl_internal_listtype_t;

typedef struct {
    void                           *next;     // for nemesis
    void                           *user_ptr;
    ptl_internal_handle_converter_t me_handle;
    size_t                          local_offset;
    size_t                          messages, announced;     // for knowing when to issue PTL_EVENT_FREE
    ptl_match_bits_t                dont_ignore_bits;
    uint_fast8_t                    unlinked;
} ptl_internal_appendME_t;


struct ptl_ppe_me_t {

    ptl_internal_appendME_t Qentry;
    ptl_me_t visible;
    ptl_list_t ptl_list;
    ptl_pt_index_t          pt_index;

    // ------------- for Mike's ---------------------------
    ptl_double_list_item_t  base;

    int                     ref_cnt;

    ptl_list_t              list;
    void*                  *user_ptr;

    // from ptl_me_t
    ptl_ppe_xpmem_ptr_t    *xpmem_ptr; // contains start, length 
    ptl_handle_generic_t    ct_h; 
    ptl_uid_t               uid;
    unsigned int            options;
    ptl_process_t           match_id;
    ptl_match_bits_t        match_bits;
    ptl_match_bits_t        ignore_bits;
    ptl_size_t              min_free;
};
typedef struct ptl_ppe_me_t ptl_ppe_me_t;

struct ptl_ppe_le_t {

    ptl_double_list_item_t  base;

    int                     ref_cnt;

    ptl_pt_index_t          pt_index;
    ptl_list_t              list;
    void*                  *user_ptr;

    // from ptl_le_t
    ptl_ppe_xpmem_ptr_t    *xpmem_ptr; // contains start, length 
    ptl_handle_generic_t    ct_h; 
    ptl_uid_t               uid;
    unsigned int            options;
};
typedef struct ptl_ppe_le_t ptl_ppe_le_t;

struct ptl_ppe_pt_t {

    // from Kyle' ptl_internal_me_t
    ptl_handle_eq_t         EQ;
    struct PTqueue {
        void *head, *tail;
    } priority,
      overflow,
      buffered_headers;

    // ------------- for Mike's ---------------------------
    unsigned int            options;
//    ptl_handle_generic_t    eq_h;
    
    ptl_double_list_t         list[2];
};
typedef struct ptl_ppe_pt_t ptl_ppe_pt_t;

typedef ptl_ppe_me_t ptl_internal_me_t;
typedef ptl_ppe_pt_t ptl_table_entry_t;
typedef ptl_event_t ptl_internal_event_t;

struct ptl_ppe_ni_t {
    ptl_ni_limits_t     *limits;
    ptl_ppe_xpmem_ptr_t *limits_ptr;
    void                *client_address;
    ptl_ppe_xpmem_ptr_t *client_ptr;
    ptl_sr_value_t      *client_status_registers;
    ptl_internal_le_t   *client_le;
    ptl_internal_md_t   *client_md;
    ptl_shared_me_t   *client_me;
    ptl_internal_ct_t   *client_ct;
    ptl_internal_eq_t   *client_eq;
    ptl_internal_pt_t   *client_pt;
    ptl_ppe_md_t        *ppe_md;
    ptl_ppe_me_t        *ppe_me;
    ptl_ppe_le_t        *ppe_le;
    ptl_ppe_pt_t        *ppe_pt;
    ptl_ppe_eq_t        *ppe_eq;
};
typedef struct ptl_ppe_ni_t ptl_ppe_ni_t;


struct ptl_ppe_client_t {
    int connected;
    xpmem_segid_t segid;
    ptl_ppe_xpmem_t xpmem_segments;
    ptl_pid_t pid;
    ptl_ppe_ni_t nis[4];
};
typedef struct ptl_ppe_client_t ptl_ppe_client_t;


struct ptl_ppe_t {
    ptl_cq_handle_t cq_h;
    ptl_cm_server_handle_t cm_h;
    ptl_cq_info_t *info;
    size_t infolen;
    int shutdown;
    long page_size;
    ptl_nid_t nid;
    signed char pids[PTL_PID_MAX];
    ptl_ppe_client_t clients[MC_PEER_COUNT];
    lib_ni_t ni;
};
typedef struct ptl_ppe_t ptl_ppe_t;


int ptl_ppe_init(ptl_ppe_t *ptl_ppe, int send_queue_size, int recv_queue_size);
int ptl_ppe_fini(ptl_ppe_t *ptl_ppe);
int ptl_ppe_teardown_peer(ptl_ppe_t *ptl_ppe, int remote_id, int forced);

#endif
