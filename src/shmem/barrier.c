#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <portals4.h>
#include <portals4_runtime.h>

/* System headers */
#include <stdlib.h>
#include <assert.h>

/* Internal headers */
#include "ptl_internal_handles.h"
#include "ptl_visibility.h"

static ptl_process_t COLLECTOR;
static int barrier_inited = 0;

#define NI_PHYS_NOMATCH 3

void API_FUNC runtime_barrier(
    void)
{
    ptl_handle_le_t leh;
    ptl_le_t le;
    ptl_handle_md_t mdh;
    ptl_md_t md;
    ptl_ct_event_t ctc;
    const ptl_internal_handle_converter_t ni = {.s =
	    {HANDLE_NI_CODE, NI_PHYS_NOMATCH, 0} };

    if (barrier_inited == 0) {
	assert(getenv("PORTALS4_COLLECTOR_NID") != NULL);
	assert(getenv("PORTALS4_COLLECTOR_PID") != NULL);
	COLLECTOR.phys.nid =
	    strtol(getenv("PORTALS4_COLLECTOR_NID"), NULL, 10);
	COLLECTOR.phys.pid =
	    strtol(getenv("PORTALS4_COLLECTOR_PID"), NULL, 10);
    }
    le.start = md.start = NULL;
    le.length = md.length = 0;
    le.ac_id.uid = PTL_UID_ANY;
    le.options = PTL_LE_OP_PUT | PTL_LE_USE_ONCE | PTL_LE_EVENT_CT_PUT;
    md.options = PTL_MD_EVENT_DISABLE;
    le.ct_handle = md.ct_handle = PTL_CT_NONE;
    md.eq_handle = PTL_EQ_NONE;
    assert(PtlCTAlloc(ni.a.ni, &le.ct_handle) == PTL_OK);
    /* post my sensor */
    assert(PtlLEAppend(ni.a.ni, 0, le, PTL_PRIORITY_LIST, NULL, &leh) ==
	   PTL_OK);
    /* prepare my messenger */
    assert(PtlMDBind(ni.a.ni, &md, &mdh) == PTL_OK);
    /* alert COLLECTOR of my presence */
    assert(PtlPut(mdh, 0, 0, PTL_CT_ACK_REQ, COLLECTOR, 0, 0, 0, NULL, 0) ==
	   PTL_OK);
    /* wait for COLLECTOR to respond */
    assert(PtlCTWait(le.ct_handle, 1, &ctc) == PTL_OK);
    assert(ctc.failure == 0);
    assert(PtlMDRelease(mdh) == PTL_OK);
    assert(PtlCTFree(le.ct_handle) == PTL_OK);
}
