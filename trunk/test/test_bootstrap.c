#include <portals4.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

int main(
    int argc,
    char *argv[])
{
    ptl_handle_ni_t ni_physical, ni_logical;
    ptl_process_id_t myself;
    ptl_process_id_t COLLECTOR;
    uint64_t rank, maxrank;
    ptl_process_id_t *mapping;
    ptl_le_t le;
    ptl_handle_le_t le_handle;
    ptl_md_t md;
    ptl_handle_md_t md_handle;

    assert(PtlInit() == PTL_OK);

    assert(PtlNIInit
	   (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
	    PTL_PID_ANY, NULL, NULL, 0, NULL, NULL, &ni_physical) == PTL_OK);
    assert(PtlGetId(ni_physical, &myself) == PTL_OK);
    /* \begin{runtime_stuff} */
    COLLECTOR.phys.nid = atoi(getenv("PORTALS4_COLLECTOR_NID"));
    COLLECTOR.phys.pid = atoi(getenv("PORTALS4_COLLECTOR_PID"));
    rank = atoi(getenv("PORTALS4_RANK"));
    maxrank = atoi(getenv("PORTALS4_NUM_PROCS")) - 1;
    /* \end{runtime_stuff} */
    mapping = calloc(maxrank + 1, sizeof(ptl_process_id_t));
    if (myself.phys.pid == COLLECTOR.phys.pid) {
	/* this will never happen in user code, because the collector stuff is
	 * handled by Yod, I'm just putting the code here to show both sides */

	/* set up a landing pad to collect everyone's information. */
	md.start = le.start = mapping;
	md.length = le.length = (maxrank + 1) * sizeof(ptl_process_id_t);
	le.ac_id.uid = PTL_UID_ANY;
	le.options =
	    PTL_LE_OP_PUT | PTL_LE_OP_GET | PTL_LE_EVENT_CT_PUT |
	    PTL_LE_EVENT_CT_GET;
	assert(PtlCTAlloc(ni_physical, PTL_CT_OPERATION, &le.ct_handle)
	       == PTL_OK);
	assert(PtlLEAppend
	       (ni_physical, 0, le, PTL_PRIORITY_LIST, NULL,
		&le_handle) == PTL_OK);
	/* wait for everyone to post to the mapping */
	assert(PtlCTWait(le.ct_handle, maxrank + 1) == PTL_OK);
	/* now distribute the mapping */
	md.options = PTL_MD_EVENT_DISABLE;
	md.eq_handle = PTL_EQ_NONE;
	md.ct_handle = PTL_CT_NONE;
	assert(PtlMDBind(ni_physical, &md, &md_handle) == PTL_OK);
	for (uint64_t r = 0; r <= maxrank; ++r) {
	    assert(PtlPut
		   (md_handle, 0, (maxrank + 1) * sizeof(ptl_process_id_t),
		    PTL_NO_ACK_REQ, mapping[r], 0, 0, 0, NULL, 0) == PTL_OK);
	}
	/* cleanup */
	assert(PtlMDRelease(md_handle) == PTL_OK);
	assert(PtlCTFree(le.ct_handle) == PTL_OK);
	assert(PtlLEUnlink(le_handle) == PTL_OK);
	assert(PtlNIFini(ni_physical) == PTL_OK);
    } else {

	md.start = &myself;
	md.length = sizeof(ptl_process_id_t);
	md.options = PTL_MD_EVENT_DISABLE;	// i.e. don't count sends
	md.eq_handle = PTL_EQ_NONE;    // i.e. don't queue send events
	md.ct_handle = PTL_CT_NONE;    // i.e. don't count sends
	assert(PtlMDBind(ni_physical, &md, &md_handle) == PTL_OK);
	assert(PtlPut
	       (md_handle, 0, sizeof(ptl_process_id_t), PTL_NO_ACK_REQ,
		COLLECTOR, 0, 0, rank * sizeof(ptl_process_id_t), NULL,
		0) == PTL_OK);
	assert(PtlMDRelease(md_handle) == PTL_OK);
	/* wait to receive the mapping from the COLLECTOR */
	le.start = mapping;
	le.length = (maxrank + 1) * sizeof(ptl_process_id_t);
	le.ac_id.uid = PTL_UID_ANY;
	le.options = PTL_LE_OP_PUT | PTL_LE_USE_ONCE | PTL_LE_EVENT_CT_PUT;
	assert(PtlCTAlloc(ni_physical, PTL_CT_OPERATION, &le.ct_handle) ==
	       PTL_OK);
	assert(PtlLEAppend
	       (ni_physical, 0, le, PTL_PRIORITY_LIST, NULL,
		&le_handle) == PTL_OK);
	assert(PtlCTWait(le.ct_handle, 1) == PTL_OK);
	/* cleanup the counter */
	assert(PtlCTFree(le.ct_handle) == PTL_OK);
	/* feed the accumulated mapping into NIInit to create the rank-based
	 * interface */
	assert(PtlNIInit
	       (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_LOGICAL,
		myself.phys.pid, NULL, NULL, maxrank, mapping, NULL,
		&ni_logical) == PTL_OK);
	/* don't need this anymore, so free up resources */
	assert(PtlNIFini(ni_physical) == PTL_OK);
    }

    /* now I can communicate between ranks with ni_logical */
    /* ... do stuff ... */

    /* cleanup */
    assert(PtlNIFini(ni_logical) == PTL_OK);
    PtlFini();

    return 0;
}
