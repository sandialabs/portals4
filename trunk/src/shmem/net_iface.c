/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <stdlib.h>
#include <limits.h>		       /* for UINT_MAX */

#include <stdio.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_atomic.h"

ptl_internal_nit_t nit;
ptl_ni_limits_t nit_limits;

const ptl_nid_t PTL_NID_ANY = UINT_MAX;
const ptl_rank_t PTL_RANK_ANY = UINT_MAX;
const ptl_interface_t PTL_IFACE_DEFAULT = UINT_MAX;

int PtlNIInit(
    ptl_interface_t iface,
    unsigned int options,
    ptl_pid_t pid,
    ptl_ni_limits_t * desired,
    ptl_ni_limits_t * actual,
    ptl_size_t map_size,
    ptl_process_id_t * desired_mapping,
    ptl_process_id_t * actual_mapping,
    ptl_handle_ni_t * ni_handle)
{
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
#ifndef NO_ARG_VALIDATION
    if (iface != 0 && iface != PTL_IFACE_DEFAULT) {
	return PTL_ARG_INVALID;
    }
    if (pid != PTL_PID_ANY && pid != proc_number) {
	return PTL_ARG_INVALID;
    }
    if (options & PTL_NI_MATCHING && options & PTL_NI_NO_MATCHING) {
	return PTL_ARG_INVALID;
    }
    if (options & PTL_NI_LOGICAL && options & PTL_NI_PHYSICAL) {
	return PTL_ARG_INVALID;
    }
    if (pid > num_siblings && pid != PTL_PID_ANY) {
	return PTL_ARG_INVALID;
    }
    if (ni_handle == NULL) {
	return PTL_ARG_INVALID;
    }
    if (map_size > 0 && (desired_mapping == NULL || actual_mapping == NULL)) {
	return PTL_ARG_INVALID;
    }
#endif
    switch (options) {
	case (PTL_NI_MATCHING | PTL_NI_LOGICAL):
	    *ni_handle = 0;
	    break;
	case PTL_NI_NO_MATCHING | PTL_NI_LOGICAL:
	    *ni_handle = 1;
	    break;
	case (PTL_NI_MATCHING | PTL_NI_PHYSICAL):
	    *ni_handle = 2;
	    break;
	case PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL:
	    *ni_handle = 3;
	    break;
#ifndef NO_ARG_VALIDATION
	default:
	    return PTL_ARG_INVALID;
#endif
    }
    if (actual != NULL) {
	*actual = nit_limits;
    }
    if (options & PTL_NI_LOGICAL) {
	for (int i = 0; i < map_size; ++i) {
	    if (i >= num_siblings) {
		actual_mapping[i].phys.nid = PTL_NID_ANY;	// aka "invalid"
		actual_mapping[i].phys.pid = PTL_PID_ANY;	// aka "invalid"
	    } else {
		actual_mapping[i].phys.nid = 0;
		actual_mapping[i].phys.pid = i;
	    }
	}
    }
    nit.tables[*ni_handle] =
	calloc(nit_limits.max_pt_index + 1, sizeof(ptl_table_entry_t));
    if (nit.tables[*ni_handle] == NULL) {
	return PTL_NO_SPACE;
    }
    __sync_synchronize();	       // full memory fence
    {
	uint32_t oldval, newval = nit.enabled;
	do {
	    oldval = newval;
	    newval |= (1 << (*ni_handle));
	} while ((newval = PtlInternalAtomicCas32(&(nit.enabled), oldval, newval)) != oldval);
    }
    return PTL_OK;
}

int PtlNIFini(
    ptl_handle_ni_t ni_handle)
{
    return PTL_OK;
}

int PtlNIStatus(
    ptl_handle_ni_t ni_handle,
    ptl_sr_index_t status_register,
    ptl_sr_value_t * status)
{
    return PTL_FAIL;
}

int PtlNIHandle(
    ptl_handle_any_t handle,
    ptl_handle_ni_t * ni_handle)
{
    return PTL_FAIL;
}
