#include "test_one_way.h"

void test_one_wayLE(int cache_size, int *cache_buf, ptl_handle_ni_t ni,
	int npeers, int nmsgs, int nbytes, int niters )
{
    double tmp, total = 0;

    Debug("\n");

    __PtlBarrier();
    if (rank < (world_size / 2))   {
	int i;	
	ptl_handle_md_t md_handle;
	ptl_md_t        md;

	ptl_assert( PtlEQAlloc( ni, nmsgs + 1, &md.eq_handle ), PTL_OK );

        md.start     = send_buf;
        md.length    = SEND_BUF_SIZE;
        md.options   = PTL_MD_UNORDERED | PTL_MD_REMOTE_FAILURE_DISABLE;
        md.ct_handle = PTL_CT_NONE;
            
        ptl_assert( PtlMDBind(ni, &md, &md_handle), PTL_OK );

        for (i= 0; i < niters; ++i)   {
	    int k;

            cache_invalidate(cache_size, cache_buf);

            __PtlBarrier();
	    tmp = timer();
	    for (k= 0; k < nmsgs; k++)   {
                ptl_size_t offset = nbytes * k;
                ptl_process_t dest;
		dest.rank= rank + (world_size / 2);
                ptl_assert( __PtlPut_offset(md_handle, offset, nbytes, dest,
			TestOneWayIndex, magic_tag, offset), PTL_OK );
            }

	    for (k= 0; k < nmsgs; k++)   {
		ptl_event_t event;
                ptl_assert( PtlEQWait(md.eq_handle, &event), PTL_OK );
		ptl_assert( event.type, PTL_EVENT_SEND ); 
            }

	    total += (timer() - tmp);
        }

        ptl_assert( PtlEQFree( md.eq_handle ), PTL_OK );
        ptl_assert( PtlMDRelease(md_handle) , PTL_OK );

    } else {
	int i;
	ptl_pt_index_t  index;
	ptl_handle_le_t le_handle;
	ptl_le_t        le;

	ptl_assert( PtlCTAlloc( ni, &le.ct_handle ), PTL_OK );

	ptl_assert( PtlPTAlloc( ni, 0, PTL_EQ_NONE, TestOneWayIndex, 
							&index ), PTL_OK );
	ptl_assert( index, TestOneWayIndex );

	le.start     = recv_buf;
	le.length    = RECV_BUF_SIZE;
	le.ac_id.uid = PTL_UID_ANY;
	le.options   = PTL_LE_OP_PUT | PTL_LE_ACK_DISABLE | 
						PTL_LE_EVENT_CT_COMM;
	ptl_assert( PtlLEAppend(ni, index, &le, PTL_PRIORITY_LIST,
				NULL, &le_handle), PTL_OK );

        for (i= 0; i < niters; ++i)   {
	    ptl_ct_event_t cnt_value;

            cache_invalidate(cache_size, cache_buf);

            __PtlBarrier();
	    tmp = timer();

	    ptl_assert( PtlCTWait( le.ct_handle, (i + 1) * nmsgs,
				&cnt_value ), PTL_OK );
	    ptl_assert( cnt_value.failure, 0 );

	    total += (timer() - tmp);
	}

	ptl_assert( PtlLEUnlink( le_handle), PTL_OK );
        ptl_assert( PtlPTFree( ni, index ), PTL_OK );
	ptl_assert( PtlCTFree( le.ct_handle ), PTL_OK );
    }

    tmp= __PtlAllreduceDouble(total, PTL_SUM);

#if 0 
    printf("%s %.1f ns\n",rank==0?"send":"recv",
				total/(double)(niters*nmsgs) * 1000000000.0);

    if ( rank == 0 )
        printf("avg %.1f ns\n",tmp/(double)(niters*nmsgs) * 1000000000.0/2.0);
#endif

    display_result("single direction w/EQ",
				(niters * nmsgs) / (tmp / world_size));

    __PtlBarrier();
}
