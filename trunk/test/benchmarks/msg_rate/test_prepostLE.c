#include "test_prepost.h"

void
test_prepostLE(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers,
		int nmsgs, int nbytes, int niters )

{               
    int i, j, k;
    double tmp, total = 0;    
    int rc;

    ptl_handle_md_t md_handle;
    ptl_process_t dest;
    ptl_size_t offset;
    ptl_pt_index_t index;
    ptl_handle_le_t le_handle;
    ptl_handle_ct_t send_ct_handle = PTL_INVALID_HANDLE;
    ptl_handle_ct_t recv_ct_handle = PTL_INVALID_HANDLE;
    ptl_ct_event_t cnt_value;

    libtest_CreateMDCT(ni, send_buf, SEND_BUF_SIZE, &md_handle, &send_ct_handle);
    index = libtest_PTAlloc(ni, TestSameDirectionIndex, PTL_EQ_NONE);

    libtest_Barrier();
                
    tmp = timer();

    libtest_CreateLECT(ni, index, recv_buf, RECV_BUF_SIZE, &le_handle,
							&recv_ct_handle );
    total += (timer() - tmp);
                
    for (i = 0 ; i < niters - 1 ; ++i) {
        cache_invalidate( cache_size, cache_buf );
                    
        libtest_Barrier();

        tmp = timer();        
        for (j = 0 ; j < npeers ; ++j) {
            for (k = 0 ; k < nmsgs ; ++k) {

		offset = (nbytes * (k + j * nmsgs)), 
		dest.rank = send_peers[npeers - j - 1],
		rc= libtest_Put_offset(md_handle, offset, nbytes, dest,
					index, magic_tag, offset);
            }
        }
	/* wait for sends */
        rc= PtlCTWait(send_ct_handle, (i+1)*nmsgs*npeers, &cnt_value);

	/* wait for receives */ 
        rc= PtlCTWait(recv_ct_handle, (i+1)*nmsgs*npeers, &cnt_value);
        total += (timer() - tmp);
    }
    tmp = timer();

    for (j = 0 ; j < npeers ; ++j) {
        for (k = 0 ; k < nmsgs ; ++k) {
	    offset = (nbytes * (k + j * nmsgs)), 
	    dest.rank = send_peers[npeers - j - 1],
	    rc= libtest_Put_offset(md_handle, offset, nbytes, dest,
					index, magic_tag, offset);
        }
    }

    /* wait for sends */
    rc= PtlCTWait(send_ct_handle, (i+1)*nmsgs*npeers, &cnt_value);

    /* wait for receives */ 
    rc= PtlCTWait(recv_ct_handle, (i+1)*nmsgs*npeers, &cnt_value);

    total += (timer() - tmp);

    /* cleanup */
    rc= PtlCTFree(send_ct_handle);
    LIBTEST_CHECK(rc, "PtlCTFree in test_prepostLE");

    rc= PtlCTFree(recv_ct_handle);
    LIBTEST_CHECK(rc, "PtlCTFree in test_prepostLE");

    PtlMDRelease(md_handle);
    LIBTEST_CHECK(rc, "PtlMDRelease in test_prepostLE");

    rc= PtlLEUnlink(le_handle);
    LIBTEST_CHECK(rc, "PtlLEUnlink in test_prepostLE");

    rc= PtlPTFree(ni, index); 
    LIBTEST_CHECK(rc, "PtlPTFree in test_prepostLE");

    tmp= libtest_AllreduceDouble(total, PTL_SUM);
    display_result("pre-post", (niters * npeers * nmsgs * 2) / (tmp / world_size));
}
