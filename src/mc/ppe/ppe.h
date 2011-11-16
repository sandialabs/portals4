#ifndef MC_PPE_PPE_H
#define MC_PPE_PPE_H

#include <stdio.h>
#include <xpmem.h>

#include "portals4.h"

#include "shared/ptl_command_queue.h"
#include "shared/ptl_connection_manager.h"
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


struct ptl_ppe_ni_t {
    ptl_ni_limits_t     *limits;
    ptl_ppe_xpmem_ptr_t *limits_ptr;
    void                *client_address;
    ptl_ppe_xpmem_ptr_t *client_ptr;
    ptl_sr_value_t      *client_status_registers;
    ptl_internal_le_t   *client_le;
    ptl_internal_md_t   *client_md;
    ptl_internal_me_t   *client_me;
    ptl_internal_ct_t   *client_ct;
    ptl_internal_eq_t   *client_eq;
    ptl_internal_pt_t   *client_pt;
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
