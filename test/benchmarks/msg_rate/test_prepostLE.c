#include "test_prepost.h"

void
test_prepostLE(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers,
		int nmsgs, int nbytes, int niters )
{               
    int i, j, k;
    double tmp, total = 0;    

    ptl_handle_md_t send_md_handle;
    ptl_md_t        send_md;
    ptl_process_t   dest;
    ptl_size_t      offset;
    ptl_pt_index_t  index;
    ptl_handle_le_t le_handle;
    ptl_handle_ct_t recv_ct_handle = PTL_INVALID_HANDLE;
    ptl_ct_event_t  cnt_value;
    ptl_event_t     event;

    ptl_assert( PtlEQAlloc( ni, nmsgs * npeers + 1, 
				&send_md.eq_handle ), PTL_OK );

    send_md.start     = send_buf;
    send_md.length    = SEND_BUF_SIZE;
    send_md.options   = PTL_MD_UNORDERED | PTL_MD_REMOTE_FAILURE_DISABLE;
    send_md.ct_handle = PTL_CT_NONE;

    ptl_assert( PtlMDBind(ni, &send_md, &send_md_handle), PTL_OK );

    ptl_assert( PtlPTAlloc( ni, 0, PTL_EQ_NONE, TestSameDirectionIndex,
                                                        &index ), PTL_OK );
    ptl_assert( TestSameDirectionIndex, index );

    __PtlBarrier();
                
    tmp = timer();

    __PtlCreateLECT(ni, index, recv_buf, RECV_BUF_SIZE, &le_handle,
							&recv_ct_handle );
    total += (timer() - tmp);
                
    for (i = 0 ; i < niters - 1 ; ++i) {
        cache_invalidate( cache_size, cache_buf );
                    
        __PtlBarrier();

        tmp = timer();        
        for (j = 0 ; j < npeers ; ++j) {
            for (k = 0 ; k < nmsgs ; ++k) {

		offset = (nbytes * (k + j * nmsgs)), 
		dest.rank = send_peers[npeers - j - 1],
		ptl_assert( __PtlPut_offset(send_md_handle, offset, nbytes,
			dest, index, magic_tag, offset), PTL_OK) ;
            }
        }
	/* wait for sends */
        for (j = 0 ; j < npeers * nmsgs; ++j) {
            ptl_assert( PtlEQWait(send_md.eq_handle, &event), PTL_OK );
            ptl_assert( event.type, PTL_EVENT_SEND );
        }

	/* wait for receives */ 
        ptl_assert( PtlCTWait(recv_ct_handle, (i+1)*nmsgs*npeers, 
				&cnt_value), PTL_OK );
        total += (timer() - tmp);
    }
    tmp = timer();

    for (j = 0 ; j < npeers ; ++j) {
        for (k = 0 ; k < nmsgs ; ++k) {
	    offset = (nbytes * (k + j * nmsgs)), 
	    dest.rank = send_peers[npeers - j - 1],
	    ptl_assert( __PtlPut_offset(send_md_handle, offset, nbytes, dest,
			index, magic_tag, offset), PTL_OK );
        }
    }

    /* wait for sends */
    for (j = 0 ; j < npeers * nmsgs; ++j) {
        ptl_assert( PtlEQWait(send_md.eq_handle, &event), PTL_OK );
        ptl_assert( event.type, PTL_EVENT_SEND );
    }

    /* wait for receives */ 
    ptl_assert( PtlCTWait(recv_ct_handle, (i+1)*nmsgs*npeers, &cnt_value), 
								PTL_OK );

    total += (timer() - tmp);

    /* cleanup */
    ptl_assert( PtlEQFree( send_md.eq_handle ), PTL_OK );

    ptl_assert( PtlCTFree(recv_ct_handle), PTL_OK );

    ptl_assert( PtlMDRelease(send_md_handle), PTL_OK );

    ptl_assert( PtlLEUnlink(le_handle), PTL_OK );

    ptl_assert( PtlPTFree(ni, index), PTL_OK ); 

    tmp= __PtlAllreduceDouble(total, PTL_SUM);
    display_result("pre-post", (niters * npeers * nmsgs * 2) / (tmp / world_size));
}
