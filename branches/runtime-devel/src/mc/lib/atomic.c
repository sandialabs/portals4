#include "config.h"

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "shared/ptl_command_queue_entry.h"

int
PtlAtomicSync(void)
{
    ptl_cqe_t   *entry;
    int ret;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
#endif

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (0 != ret) return PTL_FAIL;

    entry->base.type = PTLATOMICSYNC;
    entry->base.remote_id = ptl_iface_get_rank(&ptl_iface);

    ret = ptl_cq_entry_send_block(ptl_iface_get_cq(&ptl_iface),
                                  ptl_iface_get_peer(&ptl_iface), 
                                  entry, sizeof(ptl_cqe_atomicsync_t));
    if (0 != ret) return PTL_FAIL;

    return PTL_OK;
}

/* vim:set expandtab */
