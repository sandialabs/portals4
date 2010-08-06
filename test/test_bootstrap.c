#include <portals4.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define CHECK_RETURNVAL(x, call) do { switch (x) { \
	    case PTL_OK: break; \
	    case PTL_FAIL: fprintf(stderr, "=> %s returned PTL_FAIL (line %u)\n", call, __LINE__); abort(); break; \
	    case PTL_ARG_INVALID: fprintf(stderr, "=> %s returned PTL_ARG_INVALID (line %u)\n", call, __LINE__); abort(); break; \
	    case PTL_NO_INIT: fprintf(stderr, "=> %s returned PTL_NO_INIT (line %u)\n", call, __LINE__); abort(); break; \
	} } while (0)

static void noFailures(
    ptl_handle_ct_t ct,
    ptl_size_t threshold)
{
    ptl_ct_event_t ct_data;
    CHECK_RETURNVAL(PtlCTWait(ct, threshold), "PtlCTWait");
    //printf("bootstrap-> CTWait returned!\n");
    CHECK_RETURNVAL(PtlCTGet(ct, &ct_data), "PtlCTGet");
    assert(ct_data.failure == 0);
    //printf("bootstrap-> CTWait reports no failures!\n");
}

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

    CHECK_RETURNVAL(PtlInit(), "PtlInit");

    CHECK_RETURNVAL(PtlNIInit
	   (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
	    PTL_PID_ANY, NULL, NULL, 0, NULL, NULL, &ni_physical), "PtlNIInit");
    CHECK_RETURNVAL(PtlGetId(ni_physical, &myself), "PtlGetId");
    /* \begin{runtime_stuff} */
    assert(getenv("PORTALS4_COLLECTOR_NID") != NULL);
    assert(getenv("PORTALS4_COLLECTOR_PID") != NULL);
    COLLECTOR.phys.nid = atoi(getenv("PORTALS4_COLLECTOR_NID"));
    COLLECTOR.phys.pid = atoi(getenv("PORTALS4_COLLECTOR_PID"));
    assert(getenv("PORTALS4_RANK") != NULL);
    rank = atoi(getenv("PORTALS4_RANK"));
    assert(getenv("PORTALS4_NUM_PROCS") != NULL);
    maxrank = atoi(getenv("PORTALS4_NUM_PROCS")) - 1;
    /* \end{runtime_stuff} */
    mapping = calloc(maxrank + 1, sizeof(ptl_process_id_t));
    assert(mapping != NULL);
    if (myself.phys.pid == COLLECTOR.phys.pid) {
	/* this will never happen in user code, because the collector stuff is
	 * handled by Yod, I'm just putting the code here to show both sides */

	/* set up a landing pad to collect & distribute everyone's information. */
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
	noFailures(le.ct_handle, maxrank + 1);
	/* cleanup */
	assert(PtlCTFree(le.ct_handle) == PTL_OK);
	assert(PtlLEUnlink(le_handle) == PTL_OK);
	/* now distribute the mapping */
	md.options = PTL_MD_EVENT_CT_ACK;
	md.eq_handle = PTL_EQ_NONE;
	assert(PtlCTAlloc(ni_physical, PTL_CT_OPERATION, &md.ct_handle) ==
	       PTL_OK);
	assert(PtlMDBind(ni_physical, &md, &md_handle) == PTL_OK);
	for (uint64_t r = 0; r <= maxrank; ++r) {
	    assert(PtlPut
		   (md_handle, 0, (maxrank + 1) * sizeof(ptl_process_id_t),
		    PTL_CT_ACK_REQ, mapping[r], 0, 0, 0, NULL, 0) == PTL_OK);
	}
	/* wait for the puts to finish */
	noFailures(md.ct_handle, maxrank + 1);
	/* cleanup */
	assert(PtlCTFree(md.ct_handle) == PTL_OK);
	assert(PtlMDRelease(md_handle) == PTL_OK);
	assert(PtlNIFini(ni_physical) == PTL_OK);
    } else {
	/* for distributing my ID */
	md.start = &myself;
	md.length = sizeof(ptl_process_id_t);
	md.options = PTL_MD_EVENT_DISABLE | PTL_MD_EVENT_CT_SEND;	// count sends, but don't trigger events
	md.eq_handle = PTL_EQ_NONE;    // i.e. don't queue send events
	CHECK_RETURNVAL(PtlCTAlloc
			(ni_physical, PTL_CT_OPERATION, &md.ct_handle),
			"PtlCTAlloc");
	/* for receiving the mapping */
	le.start = mapping;
	le.length = (maxrank + 1) * sizeof(ptl_process_id_t);
	le.ac_id.uid = PTL_UID_ANY;
	le.options = PTL_LE_OP_PUT | PTL_LE_USE_ONCE | PTL_LE_EVENT_CT_PUT;
	CHECK_RETURNVAL(PtlCTAlloc
			(ni_physical, PTL_CT_OPERATION, &le.ct_handle),
			"PtlCTAlloc");
	/* post this now to avoid a race condition later */
	CHECK_RETURNVAL(PtlLEAppend
			(ni_physical, 0, le, PTL_PRIORITY_LIST, NULL,
			 &le_handle), "PtlLEAppend");
	/* now send my ID to the collector */
	CHECK_RETURNVAL(PtlMDBind(ni_physical, &md, &md_handle), "PtlMDBind");
	CHECK_RETURNVAL(PtlPut
			(md_handle, 0, sizeof(ptl_process_id_t),
			 PTL_OC_ACK_REQ, COLLECTOR, 0, 0,
			 rank * sizeof(ptl_process_id_t), NULL, 0), "PtlPut");
	/* wait for the send to finish */
	noFailures(md.ct_handle, 1);
	/* cleanup */
	CHECK_RETURNVAL(PtlMDRelease(md_handle), "PtlMDRelease");
	CHECK_RETURNVAL(PtlCTFree(md.ct_handle), "PtlCTFree");
	printf("progress: %i\n", __LINE__);
	/* wait to receive the mapping from the COLLECTOR */
	noFailures(le.ct_handle, 1);
	printf("progress: %i\n", __LINE__);
	/* cleanup the counter */
	CHECK_RETURNVAL(PtlCTFree(le.ct_handle), "PtlCTFree");
	printf("progress: %i\n", __LINE__);
	/* feed the accumulated mapping into NIInit to create the rank-based
	 * interface */
	CHECK_RETURNVAL(PtlNIInit
	       (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_LOGICAL,
		myself.phys.pid, NULL, NULL, maxrank, mapping, NULL,
		&ni_logical), "PtlNIInit");
	printf("progress: %i\n", __LINE__);
	/* don't need this anymore, so free up resources */
	CHECK_RETURNVAL(PtlNIFini(ni_physical), "PtlNIFini");
	printf("progress: %i\n", __LINE__);
    }

    /* now I can communicate between ranks with ni_logical */
    /* ... do stuff ... */
    printf("progress: %i\n", __LINE__);

    /* cleanup */
    assert(PtlNIFini(ni_logical) == PTL_OK);
    printf("progress: %i\n", __LINE__);
    PtlFini();
    printf("progress: %i\n", __LINE__);

    return 0;
}
