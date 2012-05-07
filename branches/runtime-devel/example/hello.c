#include <portals4.h>
#include <portals4_runtime.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

int main(
    int argc,
    char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t myself;
    ptl_pt_index_t logical_pt_index;
    uint64_t value, readval;
    ptl_me_t value_e;
    ptl_handle_me_t value_e_handle;
    ptl_md_t read_md;
    ptl_handle_md_t read_md_handle;
    int my_rank, num_procs;

    PtlInit();

    my_rank = runtime_get_rank();
    num_procs = runtime_get_size();

    PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_LOGICAL,
	      PTL_PID_ANY, NULL, NULL, 0, NULL, NULL, &ni_logical);
    PtlGetId(ni_logical, &myself);
    assert(my_rank == myself.rank);
    printf("I am rank %u\n", (unsigned)my_rank);

    PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY, &logical_pt_index);
    assert(logical_pt_index == 0);
    printf
	("Rank %u allocated PT index %u on the logical matching interface for my communication\n",
	 (unsigned)my_rank, (unsigned)logical_pt_index);

    /* Now do the initial setup on ni_logical */
    value = myself.rank + 0xdeadbeefc0d1f1ed;
    value_e.start = &value;
    value_e.length = sizeof(uint64_t);
    value_e.ac_id.uid = PTL_UID_ANY;
    value_e.match_id.rank = PTL_RANK_ANY;
    value_e.match_bits = 1;
    value_e.ignore_bits = 0;
    value_e.options = (PTL_ME_OP_GET | PTL_ME_EVENT_CT_COMM);
    PtlCTAlloc(ni_logical, &value_e.ct_handle);
    PtlMEAppend(ni_logical, 0, &value_e, PTL_PRIORITY_LIST, NULL,
		&value_e_handle);
    printf("Rank %u posted an ME to bootstrap; now calling the runtime_barrier()\n", (unsigned)my_rank);
    /* Now do a barrier (on ni_physical) to make sure that everyone has their
     * logical interface set up */
    runtime_barrier();

    printf("Rank %u: now I can communicate between ranks with ni_logical\n", (unsigned)my_rank);

    /* set up the landing pad so that I can read others' values */
    read_md.start = &readval;
    read_md.length = sizeof(uint64_t);
    read_md.options = PTL_MD_EVENT_CT_REPLY;
    read_md.eq_handle = PTL_EQ_NONE;
    PtlCTAlloc(ni_logical, &read_md.ct_handle);
    PtlMDBind(ni_logical, &read_md, &read_md_handle);

    /* read rank 0's value (an arbitrary action) */
    {
	ptl_ct_event_t ctc;
	ptl_process_t r0 = {.rank = 0 };
	PtlGet(read_md_handle, 0, sizeof(uint64_t), r0, logical_pt_index, 1,
	       0, NULL);
	PtlCTWait(read_md.ct_handle, 1, &ctc);
	assert(ctc.failure == 0);
    }
    assert(readval == 0xdeadbeefc0d1f1ed);
    if (myself.rank == 0) {
	ptl_ct_event_t ct_data;
	PtlCTWait(value_e.ct_handle, num_procs, &ct_data);
	if (ct_data.failure != 0) {
	    fprintf(stderr, "ct_data reports failure!!!!!!! {%u, %u}\n",
		    (unsigned int)ct_data.success,
		    (unsigned int)ct_data.failure);
	    abort();
	}
    }
    PtlMDRelease(read_md_handle);
    PtlCTFree(read_md.ct_handle);
    PtlMEUnlink(value_e_handle);
    PtlCTFree(value_e.ct_handle);

    /* cleanup */
    PtlPTFree(ni_logical, logical_pt_index);
    PtlNIFini(ni_logical);
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
