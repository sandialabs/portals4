#ifndef PTL_INTERNAL_IFACE_H
#define PTL_INTERNAL_IFACE_H

#include <xpmem.h>

#include "ptl_internal_nit.h"
#include "shared/ptl_connection_manager.h"
#include "shared/ptl_command_queue.h"

struct ptl_connection_data_t;

struct ptl_iface_t {
    int32_t            init_count;
    int32_t            connection_count;
    int32_t            connection_established;
    struct ptl_connection_data_t *connection_data;
    ptl_uid_t          uid;
    int                my_ppe_rank;
    ptl_cm_client_handle_t cm_h;
    ptl_cq_handle_t    cq_h;
    xpmem_segid_t      segid;
    ptl_internal_ni_t  ni[4];
};
typedef struct ptl_iface_t ptl_iface_t;
extern ptl_iface_t ptl_iface;


static inline int
PtlInternalLibraryInitialized(void)
{
    return (ptl_iface.init_count > 0) ? PTL_OK : PTL_FAIL;
}


static inline ptl_cq_handle_t
ptl_iface_get_cq(ptl_iface_t *iface)
{
    return iface->cq_h;
}


static inline int
ptl_iface_get_peer(ptl_iface_t *iface)
{
    return 0;
}


static inline int
ptl_iface_get_rank(ptl_iface_t *iface)
{
    return iface->my_ppe_rank;
}

#endif
/* vim:set expandtab: */

