/*
 * Portals version of the adjacent_32bit_amo test in Open SHMEM
 */

#include <portals4.h>
#include <support.h>

#include <stdio.h>
#include <stdlib.h>

#include "testing.h"

const int tries = 1000000;
typedef int32_t locktype;
 

int
main(int argc, char *argv[])
{
    int tpe, other;
    long i;
    struct {
	locktype a;
	locktype b;
    } *twovars;  
    int numfail = 0;
    int rank, num_procs;
    ptl_handle_ni_t ni_h;
    ptl_pt_index_t  pt_index;
    ptl_le_t le;
    ptl_handle_le_t le_h;
    ptl_md_t        write_md, read_md;
    ptl_ct_event_t ctc;
    ptl_process_t *procs;
    ptl_process_t peer;
    ptl_handle_md_t write_md_h, read_md_h;
        
    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();

    /* This test only succeeds if we have more than one rank */
    if (num_procs < 2) return 77;

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));
    procs = libtest_get_mapping(ni_h);
    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));

    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, PTL_EQ_NONE, PTL_PT_ANY,
                               &pt_index));

    tpe = 0;
    peer.rank = tpe;
    other = num_procs - 1;

    twovars = malloc(sizeof(*twovars));
    if (rank == 0) {
	printf("Element size: %ld bytes\n", sizeof(locktype));
	printf("Addresses: 1st element %p\n", &twovars->a);
	printf("           2nd element %p\n", &twovars->b);
	printf("Iterations: %d   target PE: %d   other active PE: %d\n",
		tries, tpe, other);
    }
    twovars->a = 0;
    twovars->b = 0;

    le.start = twovars;
    le.length = sizeof(*twovars);
    le.uid = PTL_UID_ANY;
    le.options = PTL_LE_OP_PUT | PTL_LE_OP_GET | PTL_LE_EVENT_CT_COMM;
    CHECK_RETURNVAL(PtlCTAlloc(ni_h, &le.ct_handle));
    CHECK_RETURNVAL(PtlLEAppend(ni_h, 0, &le, PTL_PRIORITY_LIST, NULL,
                                &le_h));

    libtest_barrier();

    if (rank == 0) {
	// put two values alternately to the 1st 32 bit word
        locktype expect, check;

        write_md.start = &expect;
        write_md.length = sizeof(expect);
        write_md.options = PTL_MD_EVENT_CT_ACK;
        write_md.eq_handle = PTL_EQ_NONE;
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &write_md.ct_handle));
        CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_h));

        read_md.start = &check;
        read_md.length = sizeof(check);
        read_md.options = PTL_MD_EVENT_CT_REPLY;
        read_md.eq_handle = PTL_EQ_NONE;
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &read_md.ct_handle));
        CHECK_RETURNVAL(PtlMDBind(ni_h, &read_md, &read_md_h));

	for (i=0; i<tries; i++) {
	    expect =  2 + i%2;

            CHECK_RETURNVAL(PtlPut(write_md_h,
                                   0,
                                   sizeof(expect),
                                   PTL_CT_ACK_REQ,
                                   peer,
                                   pt_index,
                                   0,
                                   0,
                                   NULL,
                                   0));
            CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, i + 1, &ctc));

            CHECK_RETURNVAL(PtlGet(read_md_h,
                                   0,
                                   sizeof(expect),
                                   peer,
                                   pt_index,
                                   0,
                                   0,
                                   NULL));
            CHECK_RETURNVAL(PtlCTWait(read_md.ct_handle, i + 1, &ctc));

	    if (check != expect) {
		printf("error: iter %ld get returned %d expected %d\n", i, check, expect);
		numfail++;
		if (numfail > 10) {
		    printf("FAIL\n");
		    abort();
		}
	    }
	}
	printf("PE %d done doing puts and gets\n",rank);

    } else if (rank == other) {
	// keep on atomically incrementing the 2nd 32 bit word
	locktype oldval, one;

        one = 1;
        oldval = -10;

        write_md.start = &one;
        write_md.length = sizeof(one);
        write_md.options = 0;
        write_md.eq_handle = PTL_EQ_NONE;
        write_md.ct_handle = PTL_CT_NONE;
        CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_h));

        read_md.start = &oldval;
        read_md.length = sizeof(oldval);
        read_md.options = PTL_MD_EVENT_CT_REPLY;
        read_md.eq_handle = PTL_EQ_NONE;
        CHECK_RETURNVAL(PtlCTAlloc(ni_h, &read_md.ct_handle));
        CHECK_RETURNVAL(PtlMDBind(ni_h, &read_md, &read_md_h));


	for (i=0; i<tries; i++) {
            CHECK_RETURNVAL(PtlFetchAtomic(read_md_h,
                                           0,
                                           write_md_h,
                                           0,
                                           sizeof(one),
                                           peer,
                                           pt_index,
                                           0,
                                           sizeof(locktype),
                                           NULL,
                                           0,
                                           PTL_SUM,
                                           PTL_INT32_T));
            CHECK_RETURNVAL(PtlCTWait(read_md.ct_handle, i + 1, &ctc));

	    if (oldval != i) {
		printf("error: iter %ld finc got %d expect %ld\n", i, oldval, i);
		numfail++;
		if (numfail > 10) {
		    printf("FAIL\n");
		    abort();
		}
	    }
	}
	printf("PE %d done doing fincs\n",rank);
    }

    libtest_barrier();
    PtlLEUnlink(le_h);
    PtlMDRelease(write_md_h);
    if (PTL_CT_NONE != write_md.ct_handle) {
        PtlCTFree(write_md.ct_handle);
    }
    PtlMDRelease(read_md_h);
    PtlCTFree(read_md.ct_handle);
    CHECK_RETURNVAL(PtlPTFree(ni_h, pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_h));

    if (numfail) {
        printf("FAIL\n");
    }
    libtest_barrier();
    if (rank == 0) {
        printf("test complete\n");
    }

    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}
