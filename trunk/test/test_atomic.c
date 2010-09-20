#include <portals4.h>
#include <portals4_runtime.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#define CHECK_RETURNVAL(x) do { int ret; \
    switch (ret = x) { \
	case PTL_OK: break; \
	case PTL_FAIL: fprintf(stderr, "=> %s returned PTL_FAIL (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
	case PTL_NO_SPACE: fprintf(stderr, "=> %s returned PTL_NO_SPACE (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
	case PTL_ARG_INVALID: fprintf(stderr, "=> %s returned PTL_ARG_INVALID (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
	case PTL_NO_INIT: fprintf(stderr, "=> %s returned PTL_NO_INIT (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
	default: fprintf(stderr, "=> %s returned failcode %i (line %u)\n", #x, ret, (unsigned int)__LINE__); abort(); break; \
    } } while (0)

static void noFailures(
    ptl_handle_ct_t ct,
    ptl_size_t threshold,
    size_t line)
{
    ptl_ct_event_t ct_data;
    CHECK_RETURNVAL(PtlCTWait(ct, threshold, &ct_data));
    if (ct_data.failure != 0) {
	fprintf(stderr, "ct_data reports failure!!!!!!! {%u, %u} line %u\n",
		(unsigned int)ct_data.success, (unsigned int)ct_data.failure,
		(unsigned int)line);
	abort();
    }
}

int main(
    int argc,
    char *argv[])
{
    ptl_handle_ni_t ni_physical, ni_logical;
    ptl_process_t myself;
    /* used in bootstrap */
    uint64_t rank, maxrank;
    ptl_process_t COLLECTOR;
    ptl_pt_index_t phys_pt_index, logical_pt_index;
    ptl_process_t *dmapping, *amapping;
    ptl_le_t le;
    ptl_handle_le_t le_handle;
    ptl_md_t md;
    ptl_handle_md_t md_handle;
    /* used in logical test */
    uint64_t value, readval;
    ptl_le_t value_le;
    ptl_handle_le_t value_le_handle;
    ptl_md_t read_md;
    ptl_handle_md_t read_md_handle;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(PtlNIInit
		    (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
		     PTL_PID_ANY, NULL, NULL, 0, NULL, NULL, &ni_physical));
    CHECK_RETURNVAL(PtlGetId(ni_physical, &myself));
    CHECK_RETURNVAL(PtlPTAlloc
		    (ni_physical, 0, PTL_EQ_NONE, 0, &phys_pt_index));
    assert(phys_pt_index == 0);
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
    dmapping = calloc(maxrank + 1, sizeof(ptl_process_t));
    assert(dmapping != NULL);
    amapping = calloc(maxrank + 1, sizeof(ptl_process_t));
    assert(amapping != NULL);
    /* for distributing my ID */
    md.start = &myself;
    md.length = sizeof(ptl_process_t);
    md.options = PTL_MD_EVENT_DISABLE | PTL_MD_EVENT_CT_SEND;	// count sends, but don't trigger events
    md.eq_handle = PTL_EQ_NONE;	       // i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_physical, &md.ct_handle));
    /* for receiving the mapping */
    le.start = dmapping;
    le.length = (maxrank + 1) * sizeof(ptl_process_t);
    le.ac_id.uid = PTL_UID_ANY;
    le.options = PTL_LE_OP_PUT | PTL_LE_USE_ONCE | PTL_LE_EVENT_DISABLE | PTL_LE_EVENT_CT_PUT;
    CHECK_RETURNVAL(PtlCTAlloc(ni_physical, &le.ct_handle));
    /* post this now to avoid a race condition later */
    CHECK_RETURNVAL(PtlLEAppend
		    (ni_physical, 0, le, PTL_PRIORITY_LIST, NULL,
		     &le_handle));
    /* now send my ID to the collector */
    CHECK_RETURNVAL(PtlMDBind(ni_physical, &md, &md_handle));
    CHECK_RETURNVAL(PtlPut
		    (md_handle, 0, sizeof(ptl_process_t), PTL_OC_ACK_REQ,
		     COLLECTOR, phys_pt_index, 0,
		     rank * sizeof(ptl_process_t), NULL, 0));
    /* wait for the send to finish */
    noFailures(md.ct_handle, 1, __LINE__);
    /* cleanup */
    CHECK_RETURNVAL(PtlMDRelease(md_handle));
    CHECK_RETURNVAL(PtlCTFree(md.ct_handle));
    /* wait to receive the mapping from the COLLECTOR */
    noFailures(le.ct_handle, 1, __LINE__);
    /* cleanup the counter */
    CHECK_RETURNVAL(PtlCTFree(le.ct_handle));
    /* feed the accumulated mapping into NIInit to create the rank-based
     * interface */
    CHECK_RETURNVAL(PtlNIInit
		    (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_LOGICAL,
		     PTL_PID_ANY, NULL, NULL, maxrank + 1, dmapping, amapping,
		     &ni_logical));
    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    CHECK_RETURNVAL(PtlPTAlloc
		    (ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY,
		     &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    value = myself.rank + 0xdeadbeef;
    if (myself.rank == 0) {
	value_le.start = &value;
	value_le.length = sizeof(value);
	value_le.ac_id.uid = PTL_UID_ANY;
	value_le.options =
	    PTL_LE_OP_PUT | PTL_LE_OP_GET | PTL_LE_EVENT_CT_ATOMIC;
	CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &value_le.ct_handle));
	CHECK_RETURNVAL(PtlLEAppend
			(ni_logical, 0, value_le, PTL_PRIORITY_LIST, NULL,
			 &value_le_handle));
    }
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    runtime_barrier();
    /* don't need this anymore, so free up resources */
    free(amapping);
    free(dmapping);
    CHECK_RETURNVAL(PtlPTFree(ni_physical, phys_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_physical));

    /* now I can communicate between ranks with ni_logical */

    /* set up the landing pad so that I can read others' values */
    readval = 1;
    read_md.start = &readval;
    read_md.length = sizeof(uint64_t);
    read_md.options = PTL_MD_EVENT_DISABLE | PTL_MD_EVENT_CT_REPLY;
    read_md.eq_handle = PTL_EQ_NONE;   // i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &read_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &read_md, &read_md_handle));

    /* twiddle rank 0's value */
    {
	ptl_ct_event_t ctc;
	ptl_process_t r0 = {.rank=0};
	CHECK_RETURNVAL(PtlAtomic
			(read_md_handle, 0, sizeof(uint64_t), PTL_OC_ACK_REQ,
			 r0, logical_pt_index, 0, 0, NULL, 0,
			 PTL_SUM, PTL_ULONG));
	CHECK_RETURNVAL(PtlCTWait(read_md.ct_handle, 1, &ctc));
	assert(ctc.failure == 0);
    }
    printf("%i readval: %llx\n", (int)myself.rank,
	   (unsigned long long)readval);

    if (myself.rank == 0) {
	noFailures(value_le.ct_handle, maxrank + 1, __LINE__);
	printf("0 value: %llx\n", (unsigned long long)value);
	CHECK_RETURNVAL(PtlLEUnlink(value_le_handle));
	CHECK_RETURNVAL(PtlCTFree(value_le.ct_handle));
    }
    CHECK_RETURNVAL(PtlMDRelease(read_md_handle));
    CHECK_RETURNVAL(PtlCTFree(read_md.ct_handle));

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    PtlFini();

    return 0;
}
