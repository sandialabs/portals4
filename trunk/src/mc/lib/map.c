
#include "config.h"

#include "portals4.h"

#include "ptl_internal_iface.h"
#include "ptl_internal_global.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "shared/ptl_internal_handles.h"
#include "shared/ptl_command_queue_entry.h"

int
PtlSetMap(ptl_handle_ni_t      ni_handle,
          ptl_size_t           map_size,
          const ptl_process_t *mapping)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    int ret, cmd_ret = PTL_STATUS_LAST;
    ptl_cqe_t *entry;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        VERBOSE_ERROR("NI handle is invalid.\n");
        return PTL_ARG_INVALID;
    }
    if (ni.s.ni > 1) {
        VERBOSE_ERROR("NI handle is for physical interface\n");
        return PTL_ARG_INVALID;
    }
    if (map_size == 0) {
        VERBOSE_ERROR("Input map_size is zero\n");
        return PTL_ARG_INVALID;
    }
    if (mapping == NULL) {
        VERBOSE_ERROR("Input mapping is NULL\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (ret < 0) return PTL_FAIL;
    entry->base.type = PTLSETMAP;
    entry->setMap.ni_handle = ni;
    entry->setMap.ni_handle.s.selector = ptl_iface_get_rank(&ptl_iface);
    entry->setMap.mapping = mapping;
    entry->setMap.mapping_len = map_size;
    entry->setMap.retval_ptr = &cmd_ret;
    ret = ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), 
                            ptl_iface_get_peer(&ptl_iface), 
                            entry, sizeof(ptl_cqe_setmap_t));
    if (ret < 0) return PTL_FAIL;

    /* wait for result */
    do {
        ret = ptl_ppe_progress(&ptl_iface, 1);
        if (ret < 0) return PTL_FAIL;
        __sync_synchronize();
    } while (PTL_STATUS_LAST == cmd_ret);

    return cmd_ret;
}


int
PtlGetMap(ptl_handle_ni_t ni_handle,
          ptl_size_t      map_size,
          ptl_process_t  *mapping,
          ptl_size_t     *actual_map_size)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    int ret, cmd_ret = PTL_STATUS_LAST;
    ptl_cqe_t *entry;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (ptl_iface.ni[ni.s.ni].refcount == 0)) {
        VERBOSE_ERROR("NI handle is invalid.\n");
        return PTL_ARG_INVALID;
    }
    if ((map_size == 0) && ((mapping != NULL) || (actual_map_size == NULL))) {
        VERBOSE_ERROR("Input map_size is zero\n");
        return PTL_ARG_INVALID;
    }
    if ((mapping == NULL) && ((map_size != 0) || (actual_map_size == NULL))) {
        VERBOSE_ERROR("Output mapping ptr is NULL\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */

    *actual_map_size = map_size;

    ret = ptl_cq_entry_alloc(ptl_iface_get_cq(&ptl_iface), &entry);
    if (ret < 0) return PTL_FAIL;
    entry->base.type = PTLSETMAP;
    entry->getMap.ni_handle = ni;
    entry->getMap.ni_handle.s.selector = ptl_iface_get_rank(&ptl_iface);
    entry->getMap.mapping = mapping;
    entry->getMap.mapping_len = actual_map_size;
    entry->getMap.retval_ptr = &cmd_ret;
    ret = ptl_cq_entry_send(ptl_iface_get_cq(&ptl_iface), 
                            ptl_iface_get_peer(&ptl_iface), 
                            entry, sizeof(ptl_cqe_setmap_t));
    if (ret < 0) return PTL_FAIL;

    /* wait for result */
    do {
        ret = ptl_ppe_progress(&ptl_iface, 1);
        if (ret < 0) return PTL_FAIL;
        __sync_synchronize();
    } while (PTL_STATUS_LAST == cmd_ret);

    return cmd_ret;
}
