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
#include "ptl_internal_ME.h"
#include "ptl_internal_DM.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_error.h"

ptl_internal_nit_t nit = { {0, 0, 0, 0}
, {0, 0, 0, 0}
, {0, 0, 0, 0}
, {{0, 0}
   , {0, 0}
   , {0, 0}
   , {0, 0}
   }
};
ptl_ni_limits_t nit_limits = { 0 };

static volatile uint32_t nit_limits_init = 0;

const ptl_interface_t PTL_IFACE_DEFAULT = UINT_MAX;
const ptl_handle_any_t PTL_INVALID_HADLE = { UINT_MAX };

int API_FUNC PtlNIInit(
    ptl_interface_t iface,
    unsigned int options,
    ptl_pid_t pid,
    ptl_ni_limits_t * desired,
    ptl_ni_limits_t * actual,
    ptl_size_t map_size,
    ptl_process_t * desired_mapping,
    ptl_process_t * actual_mapping,
    ptl_handle_ni_t * ni_handle)
{
    ptl_internal_handle_converter_t ni = {.s = {HANDLE_NI_CODE, 0, 0} };
    ptl_table_entry_t *tmp;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (iface != 0 && iface != PTL_IFACE_DEFAULT) {
	VERBOSE_ERROR("Invalid Interface (%i)\n", (int)iface);
	return PTL_ARG_INVALID;
    }
    if (pid != PTL_PID_ANY && pid != proc_number) {
	VERBOSE_ERROR("Weird PID (%i)\n", (int)pid);
	return PTL_ARG_INVALID;
    }
    if (options & PTL_NI_MATCHING && options & PTL_NI_NO_MATCHING) {
	VERBOSE_ERROR("Neither matching nor non-matching\n");
	return PTL_ARG_INVALID;
    }
    if (options & PTL_NI_LOGICAL && options & PTL_NI_PHYSICAL) {
	VERBOSE_ERROR("Neither logical nor physical\n");
	return PTL_ARG_INVALID;
    }
    if (pid > num_siblings && pid != PTL_PID_ANY) {
	VERBOSE_ERROR("pid(%i) > num_siblings(%i)\n", (int)pid,
		      (int)num_siblings);
	return PTL_ARG_INVALID;
    }
    if (ni_handle == NULL) {
	VERBOSE_ERROR("ni_handle == NULL\n");
	return PTL_ARG_INVALID;
    }
    if (map_size > 0 && (desired_mapping == NULL && actual_mapping == NULL)) {
	return PTL_ARG_INVALID;
    }
    if ((options & PTL_NI_LOGICAL) != 0 && map_size != num_siblings) {
	return PTL_SIZE_INVALID;
    }
    if ((options & PTL_NI_LOGICAL) == 0 &&
	(map_size > 0 &&
	 (desired_mapping != NULL || actual_mapping != NULL))) {
	return PTL_NI_NOT_LOGICAL;
    }
#endif
    if (iface == PTL_IFACE_DEFAULT) {
	iface = 0;
    }
    ni.s.code = iface;
    switch (options) {
	case (PTL_NI_MATCHING | PTL_NI_LOGICAL):
	    ni.s.ni = 0;
	    break;
	case PTL_NI_NO_MATCHING | PTL_NI_LOGICAL:
	    ni.s.ni = 1;
	    break;
	case (PTL_NI_MATCHING | PTL_NI_PHYSICAL):
	    ni.s.ni = 2;
	    break;
	case PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL:
	    ni.s.ni = 3;
	    break;
#ifndef NO_ARG_VALIDATION
	default:
	    return PTL_ARG_INVALID;
#endif
    }
    *ni_handle = ni.a.ni;
    if (desired != NULL &&
	PtlInternalAtomicCas32(&nit_limits_init, 0, 1) == 0) {
	/* nit_limits_init now marked as "being initialized" */
	if (desired->max_mes > 0 &&
	    desired->max_mes < (1 << HANDLE_CODE_BITS)) {
	    nit_limits.max_mes = desired->max_mes;
	}
	if (desired->max_over > 0) {
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
    if ((options & PTL_NI_LOGICAL) != 0 && actual_mapping != NULL) {
	for (int i = 0; i < map_size; ++i) {
	    if (i >= num_siblings) {
		actual_mapping[i].phys.nid = PTL_NID_ANY;	// aka "invalid"
		actual_mapping[i].phys.pid = PTL_PID_ANY;	// aka "invalid"
	    } else {
		actual_mapping[i].phys.nid = 0;
		actual_mapping[i].phys.pid = (ptl_pid_t) i;
	    }
	}
    }
    PtlInternalCTNISetup(ni.s.ni, nit_limits.max_cts);
    PtlInternalMDNISetup(ni.s.ni, nit_limits.max_mds);
    PtlInternalEQNISetup(ni.s.ni);
    if (options & PTL_NI_MATCHING) {
	PtlInternalMENISetup(ni.s.ni, nit_limits.max_mes);
    } else {
	PtlInternalLENISetup(ni.s.ni, nit_limits.max_mes);
    }
    /* Okay, now this is tricky, because it needs to be thread-safe, even with respect to PtlNIFini(). */
    while ((tmp =
	    PtlInternalAtomicCasPtr(&(nit.tables[ni.s.ni]), NULL,
				    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
	tmp = calloc(nit_limits.max_pt_index + 1, sizeof(ptl_table_entry_t));
	if (tmp == NULL) {
	    nit.tables[ni.s.ni] = NULL;
	    return PTL_NO_SPACE;
	}
	nit.unexpecteds[ni.s.ni] = calloc(nit_limits.max_over, sizeof(ptl_internal_header_t));
	if (nit.unexpecteds[ni.s.ni] == NULL) {
	    free(tmp);
	    nit.tables[ni.s.ni] = NULL;
	    return PTL_NO_SPACE;
	}
	for (size_t e = 0; e <= nit_limits.max_pt_index; ++e) {
	    PtlInternalPTInit(tmp + e);
	}
	nit.tables[ni.s.ni] = tmp;
    }
    assert(nit.tables[ni.s.ni] != NULL);
    __sync_synchronize();	       // full memory fence
    PtlInternalAtomicInc(&(nit.refcount[ni.s.ni]), 1);
    PtlInternalDMSetup();	       // This MUST happen AFTER the tables are set up
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
	while (nit.internal_refcount[ni.s.ni] != 0) ;
	PtlInternalDMTeardown();
	PtlInternalCTNITeardown(ni.s.ni);
	PtlInternalMDNITeardown(ni.s.ni);
	PtlInternalEQNITeardown(ni.s.ni);
	switch (ni.s.ni) {
	    case 0:
	    case 2:
		PtlInternalMENITeardown(ni.s.ni);
		break;
	    case 1:
	    case 3:
		PtlInternalLENITeardown(ni.s.ni);
		break;
	}
	/* deallocate NI */
	free(nit.unexpecteds[ni.s.ni]);
	free(nit.tables[ni.s.ni]);
	nit.unexpecteds[ni.s.ni] = NULL;
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

int INTERNAL PtlInternalNIValidator(
    const ptl_internal_handle_converter_t ni)
{
#ifndef NO_ARG_VALIDATION
    if (ni.s.selector != HANDLE_NI_CODE) {
	return PTL_ARG_INVALID;
    }
    if (ni.s.ni > 3 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
	return PTL_ARG_INVALID;
    }
#endif
    return PTL_OK;
}

ptl_internal_header_t INTERNAL *PtlInternalAllocUnexpectedHeader(
    const unsigned int ni)
{
    ptl_internal_header_t *hdr = nit.unexpecteds[ni];
    if (hdr != NULL) {
	ptl_internal_header_t *foundhdr;
	while ((foundhdr =
		PtlInternalAtomicCasPtr(&nit.unexpecteds[ni], hdr,
					hdr->next)) != hdr) {
	    hdr = foundhdr;
	}
    }
    return hdr;
}

void INTERNAL PtlInternalDeallocUnexpectedHeader(
    ptl_internal_header_t * const hdr)
{
    const unsigned int ni = hdr->ni;
    ptl_internal_header_t *expectednext, *foundnext;

    expectednext = hdr->next = nit.unexpecteds[ni];
    while ((foundnext =
	    PtlInternalAtomicCasPtr(&nit.unexpecteds[ni], expectednext,
				    hdr)) != expectednext) {
	expectednext = hdr->next = foundnext;
    }
}
