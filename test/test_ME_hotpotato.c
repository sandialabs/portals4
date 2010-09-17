#include <portals4.h>
#include <portals4_runtime.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#define LOOPS 1000000

#define CHECK_RETURNVAL(x) do { switch (x) { \
	    case PTL_OK: break; \
	    case PTL_FAIL: fprintf(stderr, "=> %s returned PTL_FAIL (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
	    case PTL_ARG_INVALID: fprintf(stderr, "=> %s returned PTL_ARG_INVALID (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
	    case PTL_NO_INIT: fprintf(stderr, "=> %s returned PTL_NO_INIT (line %u)\n", #x, (unsigned int)__LINE__); abort(); break; \
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
    struct timeval start, stop;
    double accumulate = 0.0;
    int potato = 0;
    ptl_me_t potato_catcher;
    ptl_handle_me_t potato_catcher_handle;
    ptl_md_t potato_launcher;
    ptl_handle_md_t potato_launcher_handle;

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
		    (PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_LOGICAL,
		     PTL_PID_ANY, NULL, NULL, maxrank + 1, dmapping, amapping,
		     &ni_logical));
    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    CHECK_RETURNVAL(PtlPTAlloc
		    (ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY,
		     &logical_pt_index));
    assert(logical_pt_index == 0);
    /* Now do the initial setup on ni_logical */
    potato_catcher.start = &potato;
    potato_catcher.length = sizeof(potato);
    potato_catcher.ac_id.uid = PTL_UID_ANY;
    potato_catcher.options = PTL_ME_OP_PUT | PTL_ME_EVENT_CT_PUT | PTL_ME_EVENT_DISABLE;
    potato_catcher.match_id.rank = PTL_RANK_ANY;
    potato_catcher.match_bits = 1;
    potato_catcher.ignore_bits = ~potato_catcher.match_bits;
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &potato_catcher.ct_handle));
    CHECK_RETURNVAL(PtlMEAppend
		    (ni_logical, logical_pt_index, potato_catcher, PTL_PRIORITY_LIST, NULL,
		     &potato_catcher_handle));
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    runtime_barrier();
    /* don't need this anymore, so free up resources */
    CHECK_RETURNVAL(PtlPTFree(ni_physical, phys_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_physical));

    /* now I can communicate between ranks with ni_logical */

    /* set up the potato launcher */
    potato_launcher.start = &potato;
    potato_launcher.length = sizeof(double);
    potato_launcher.options = PTL_MD_EVENT_DISABLE | PTL_MD_EVENT_CT_ACK | PTL_MD_EVENT_CT_SEND;
    potato_launcher.eq_handle = PTL_EQ_NONE;	// i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &potato_launcher.ct_handle));
    CHECK_RETURNVAL(PtlMDBind
		    (ni_logical, &potato_launcher, &potato_launcher_handle));

    /* rank 0 starts the potato going */
    if (myself.rank == 0) {
	ptl_process_t nextrank;
	nextrank.rank = myself.rank + 1;
	nextrank.rank *= (nextrank.rank <= maxrank);
	CHECK_RETURNVAL(PtlPut
			(potato_launcher_handle, 0, sizeof(double),
			 PTL_OC_ACK_REQ, nextrank, logical_pt_index, 1, 0,
			 NULL, 1));
	CHECK_RETURNVAL(PtlCTWait(potato_launcher.ct_handle, 1, NULL));
	{
	    ptl_ct_event_t ctc = { 0, 0 };
	    CHECK_RETURNVAL(PtlCTSet(potato_launcher.ct_handle, ctc));
	}
    }

    if (myself.rank == 0) {
	assert(gettimeofday(&start, NULL) == 0);
    }
    {				       /* the potato-passing loop */
	size_t waitfor;
	ptl_ct_event_t ctc;
	ptl_process_t nextrank;
	nextrank.rank = myself.rank + 1;
	nextrank.rank *= (nextrank.rank <= maxrank);
	for (waitfor = 1; waitfor <= LOOPS; ++waitfor) {
	    CHECK_RETURNVAL(PtlCTWait(potato_catcher.ct_handle, waitfor, &ctc));	// wait for potato
	    assert(ctc.failure == 0);
	    assert(ctc.success == waitfor);
	    /* I have the potato! */
	    ++potato;
	    if (potato < LOOPS*(maxrank+1)) { // otherwise, the recipient may have exited
		/* Bomb's away! */
		CHECK_RETURNVAL(PtlPut
			(potato_launcher_handle, 0,
			 potato_launcher.length, PTL_OC_ACK_REQ, nextrank,
			 logical_pt_index, 3, 0, NULL, 2));
	    }
	}
	// make sure that last send completed before exiting
	CHECK_RETURNVAL(PtlCTWait
			(potato_launcher.ct_handle, (LOOPS-1)*2, &ctc));
	assert(ctc.failure == 0);
	if (myself.rank == 0) {
	    // wait for the last potato
	    CHECK_RETURNVAL(PtlCTWait(potato_catcher.ct_handle, waitfor - 1, &ctc));
	    assert(ctc.failure == 0);
	    printf("Final value of potato = %i\n", potato);
	}
    }
    if (myself.rank == 0) {
	assert(gettimeofday(&stop, NULL) == 0);
	accumulate =
	    (stop.tv_sec + stop.tv_usec * 1e-6) - (start.tv_sec +
		    start.tv_usec * 1e-6);
	/* calculate the average time waiting */
	printf("Total time: %g secs\n", accumulate);
	accumulate /= LOOPS;
	printf("Average time around the loop: %g microseconds\n", accumulate*1e6);
	accumulate /= maxrank + 1;
	printf("Average catch-to-toss latency: %g microseconds\n", accumulate*1e6);
    }

    /* cleanup */
    CHECK_RETURNVAL(PtlMDRelease(potato_launcher_handle));
    CHECK_RETURNVAL(PtlCTFree(potato_launcher.ct_handle));
    CHECK_RETURNVAL(PtlMEUnlink(potato_catcher_handle));
    CHECK_RETURNVAL(PtlCTFree(potato_catcher.ct_handle));

    /* major cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    PtlFini();

    return 0;
}
