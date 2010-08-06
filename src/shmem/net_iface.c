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
#include "ptl_internal_CT.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_LE.h"
#include "ptl_internal_DM.h"

ptl_internal_nit_t nit = { {0, 0, 0, 0}
, {0, 0, 0, 0}
, {{0, 0}
   , {0, 0}
   , {0, 0}
   , {0, 0}
   }
};
ptl_ni_limits_t nit_limits = { 0 };

static volatile uint32_t nit_limits_init = 0;

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
    ptl_table_entry_t *tmp;
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
    if (desired != NULL &&
	PtlInternalAtomicCas32(&nit_limits_init, 0, 1) == 0) {
	/* nit_limits_init now marked as "being initialized" */
	if (desired->max_mes > 0 &&
	    desired->max_mes < (1 << HANDLE_CODE_BITS)) {
	    nit_limits.max_mes = desired->max_mes;
	}
	if (desired->max_over > 0 &&
	    desired->max_over < (1 << HANDLE_CODE_BITS)) {
	    nit_limits.max_over = desired->max_over;
	}
	if (desired->max_mds > 0 &&
	    desired->max_mds < (1 << HANDLE_CODE_BITS)) {
	    nit_limits.max_mds = desired->max_mds;
	}
	if (desired->max_cts > 0 &&
	    desired->max_cts < (1 << HANDLE_CODE_BITS)) {
	    nit_limits.max_cts = desired->max_cts;
	}
	if (desired->max_eqs > 0 &&
	    desired->max_eqs < (1 << HANDLE_CODE_BITS)) {
	    nit_limits.max_eqs = desired->max_eqs;
	}
	if (desired->max_pt_index >= 63) {	// XXX: there may need to be more restrictions on this
	    nit_limits.max_pt_index = desired->max_pt_index;
	}
	//nit_limits.max_iovecs = INT_MAX;      // ???
	if (desired->max_me_list > 0 &&
	    desired->max_me_list < (1ULL << (sizeof(uint32_t) * 8))) {
	    nit_limits.max_me_list = desired->max_me_list;
	}
	if (desired->max_msg_size > 0 &&
	    desired->max_msg_size < nit_limits.max_msg_size) {
	    nit_limits.max_msg_size = desired->max_msg_size;
	}
	if (desired->max_atomic_size > 0 && desired->max_atomic_size <= 8) {
	    nit_limits.max_atomic_size = desired->max_atomic_size;
	}
	nit_limits_init = 2;	       // mark it as done being initialized
    }
    PtlInternalAtomicCas32(&nit_limits_init, 0, 2);	/* if not yet initialized, it is now */
    while (nit_limits_init == 1) ;     /* if being initialized by another thread, wait for it to be initialized */
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
    PtlInternalCTNISetup(ni.ni, nit_limits.max_cts);
    PtlInternalMDNISetup(ni.ni, nit_limits.max_mds);
    PtlInternalLENISetup(nit_limits.max_mes);
    PtlInternalDMSetup(nit_limits.max_msg_size);
    /* Okay, now this is tricky, because it needs to be thread-safe, even with respect to PtlNIFini(). */
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(nit.tables[ni.ni]), NULL,
				    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
	tmp = calloc(nit_limits.max_pt_index + 1, sizeof(ptl_table_entry_t));
	if (tmp == NULL) {
	    nit.tables[ni.ni] = NULL;
	    return PTL_NO_SPACE;
	}
	for (size_t e = 0; e <= nit_limits.max_pt_index; ++e) {
	    assert(pthread_mutex_init(&tmp[e].lock, NULL) == 0);
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
    const ptl_internal_handle_converter_t ni = { ni_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ni.s.ni >= 4 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    if (PtlInternalAtomicInc(&(nit.refcount[ni.s.ni]), -1) == 1) {
	PtlInternalCTNITeardown(ni.s.ni);
	PtlInternalMDNITeardown(ni.s.ni);
	/* deallocate NI */
	free(nit.tables[ni.s.ni]);
	nit.tables[ni.s.ni] = NULL;
    }
    return PTL_OK;
}

int API_FUNC PtlNIStatus(
    ptl_handle_ni_t ni_handle,
    ptl_sr_index_t status_register,
    ptl_sr_value_t * status)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ni.s.ni >= 4 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
    if (status == NULL) {
	return PTL_ARG_INVALID;
    }
    if (status_register >= 2) {
	return PTL_ARG_INVALID;
    }
#endif
    *status = nit.regs[ni.s.ni][status_register];
    return PTL_FAIL;
}

int API_FUNC PtlNIHandle(
    ptl_handle_any_t handle,
    ptl_handle_ni_t * ni_handle)
{
    ptl_internal_handle_converter_t ehandle;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
#endif
    ehandle.a = handle;
    switch (ehandle.s.selector) {
	case HANDLE_NI_CODE:
	    *ni_handle = ehandle.i;
	    break;
	case HANDLE_EQ_CODE:
	case HANDLE_CT_CODE:
	case HANDLE_MD_CODE:
	case HANDLE_LE_CODE:
	case HANDLE_ME_CODE:
	    ehandle.s.code = 0;
	    ehandle.s.selector = HANDLE_NI_CODE;
	    *ni_handle = ehandle.i;
	    break;
	default:
	    return PTL_ARG_INVALID;
    }
    return PTL_OK;
}
