/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <stdlib.h>
#include <limits.h>		       /* for UINT_MAX */
#include <string.h>		       /* for memcpy() */

#include <stdio.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_handles.h"

ptl_internal_nit_t nit;
ptl_ni_limits_t nit_limits;

const ptl_nid_t PTL_NID_ANY = UINT_MAX;
const ptl_rank_t PTL_RANK_ANY = UINT_MAX;
const ptl_interface_t PTL_IFACE_DEFAULT = UINT_MAX;
const ptl_handle_any_t PTL_INVALID_HADLE = { UINT_MAX };

int API_FUNC PtlNIInit(
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
    ptl_handle_encoding_t ni = { HANDLE_NI_CODE, 0, 0 };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
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
    if (iface == PTL_IFACE_DEFAULT) {
	iface = 0;
    }
    ni.code = iface;
    switch (options) {
	case (PTL_NI_MATCHING | PTL_NI_LOGICAL):
	    ni.ni = 0;
	    break;
	case PTL_NI_NO_MATCHING | PTL_NI_LOGICAL:
	    ni.ni = 1;
	    break;
	case (PTL_NI_MATCHING | PTL_NI_PHYSICAL):
	    ni.ni = 2;
	    break;
	case PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL:
	    ni.ni = 3;
	    break;
#ifndef NO_ARG_VALIDATION
	default:
	    return PTL_ARG_INVALID;
#endif
    }
    memcpy(ni_handle, &ni, sizeof(ptl_handle_ni_t));
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
    /* Okay, now this is tricky, because it needs to be thread-safe, even with respect to PtlNIFini(). */
    ptl_table_entry_t *tmp =
	PtlInternalAtomicCasPtr(&(nit.tables[ni.ni]), NULL, (void *)1);
    if (tmp == NULL) {
	tmp = calloc(nit_limits.max_pt_index + 1, sizeof(ptl_table_entry_t));
	if (tmp == NULL) {
	    nit.tables[ni.ni] = NULL;
	    return PTL_NO_SPACE;
	}
	nit.tables[ni.ni] = tmp;
    }
    __sync_synchronize();	       // full memory fence
    PtlInternalAtomicInc(&(nit.refcount[ni.ni]), 1);
    return PTL_OK;
}

int API_FUNC PtlNIFini(
    ptl_handle_ni_t ni_handle)
{
    ptl_handle_encoding_t ni;
    memcpy(&ni, &ni_handle, sizeof(ptl_handle_ni_t));
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ni.ni >= 4 || ni.code != 0 || (nit.refcount[ni.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    if (PtlInternalAtomicInc(&(nit.refcount[ni.ni]), -1) == 1) {
	/* deallocate NI */
	free(nit.tables[ni.ni]);
	nit.tables[ni.ni] = NULL;
    }
    return PTL_OK;
}

int API_FUNC PtlNIStatus(
    ptl_handle_ni_t ni_handle,
    ptl_sr_index_t status_register,
    ptl_sr_value_t * status)
{
    ptl_handle_encoding_t ni;
    memcpy(&ni, &ni_handle, sizeof(ptl_handle_ni_t));
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ni.ni >= 4 || ni.code != 0 || (nit.refcount[ni.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (status == NULL) {
	return PTL_ARG_INVALID;
    }
    if (status_register >= 2) {
	return PTL_ARG_INVALID;
    }
#endif
    *status = nit.regs[ni.ni][status_register];
    return PTL_FAIL;
}

int API_FUNC PtlNIHandle(
    ptl_handle_any_t handle,
    ptl_handle_ni_t * ni_handle)
{
    ptl_handle_encoding_t ehandle;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
#endif
    memcpy(&ehandle, &handle, sizeof(uint32_t));
    switch (ehandle.selector) {
	case HANDLE_NI_CODE:
	    *ni_handle = handle.ni;
	    break;
	case HANDLE_EQ_CODE:
	case HANDLE_CT_CODE:
	case HANDLE_MD_CODE:
	case HANDLE_LE_CODE:
	case HANDLE_ME_CODE:
	    ehandle.code = 0;
	    ehandle.selector = HANDLE_NI_CODE;
	    memcpy(ni_handle, &ehandle, sizeof(uint32_t));
	    break;
	default:
	    return PTL_ARG_INVALID;
    }
    return PTL_OK;
}
