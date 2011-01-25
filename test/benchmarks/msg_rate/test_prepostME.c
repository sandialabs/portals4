#include "test_prepost.h"
#include <stdlib.h>

static void postME(
    ptl_handle_ni_t ni,
    ptl_pt_index_t index,
    void *start,
    ptl_size_t length,
    ptl_process_t src,
    int tag,
    ptl_handle_me_t *mh);

void
test_prepostME(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers,
		int nmsgs, int nbytes, int niters )
{               
    int i, j, k;
    double tmp, total = 0;    
    int rc;

    ptl_handle_md_t md_handle;
    ptl_process_t dest;
    ptl_size_t offset;
    ptl_pt_index_t index;
    ptl_handle_eq_t eq_handle;
    ptl_handle_ct_t send_ct_handle = PTL_INVALID_HANDLE;
    ptl_handle_me_t me_handles[npeers * nmsgs];
    ptl_ct_event_t cnt_value;
    ptl_event_t event;

    __PtlCreateMDCT(ni, send_buf, SEND_BUF_SIZE, &md_handle, &send_ct_handle);

    rc = PtlEQAlloc(ni, nmsgs * npeers + 1, &eq_handle);

    index= __PtlPTAlloc(ni, TestSameDirectionIndex, eq_handle);
                
    tmp = timer();
    for (j = 0 ; j < npeers ; ++j) {
        for (k = 0 ; k < nmsgs ; ++k) {
            ptl_process_t src;
            src.rank = recv_peers[j];
            postME( ni, index, recv_buf + (nbytes * (k + j * nmsgs)),
                        nbytes, src, magic_tag, &me_handles[k + j * nmsgs] );
        }
    }           
    total += (timer() - tmp);

    for (i = 0 ; i < niters - 1 ; ++i) {
        cache_invalidate( cache_size, cache_buf );
                    
        __PtlBarrier();

        tmp = timer();        
        for (j = 0 ; j < npeers ; ++j) {
            for (k = 0 ; k < nmsgs ; ++k) {

	    	offset= (nbytes * (k + j * nmsgs));
		dest.rank = send_peers[npeers - j - 1],
		__PtlPut_offset(md_handle, offset, nbytes, dest,
					index, magic_tag, offset);
            }
        }

	/* wait for sends */
        PtlCTWait(send_ct_handle, (i+1)*nmsgs*npeers, &cnt_value);

	/* wait for receives */
	for (j= 0; j < npeers * nmsgs; j++)   {
            PtlEQWait(eq_handle, &event);
	}


	for (j = 0 ; j < npeers ; ++j) {
	    for (k = 0 ; k < nmsgs ; ++k) {
                ptl_process_t src;
                src.rank = recv_peers[j];
                postME( ni, index, recv_buf + (nbytes * (k + j * nmsgs)),
                        nbytes, src, magic_tag, &me_handles[k + j * nmsgs] );
            }
        }           

        total += (timer() - tmp);
    }

    __PtlBarrier();

    tmp = timer();
    for (j = 0 ; j < npeers ; ++j) {
        for (k = 0 ; k < nmsgs ; ++k) {
	    offset= (nbytes * (k + j * nmsgs));
	    dest.rank = send_peers[npeers - j - 1],
	    __PtlPut_offset(md_handle, offset, nbytes, dest,
					index, magic_tag, offset);
        }
    }
    /* wait for sends */
    PtlCTWait(send_ct_handle, (i+1)*nmsgs*npeers, &cnt_value);

    /* wait for receives */
    for (j= 0; j < npeers * nmsgs; j++)   {
	PtlEQWait(eq_handle, &event);
    }

    total += (timer() - tmp);

    rc = PtlCTFree(send_ct_handle);
    PTL_CHECK(rc, "PtlCTFree in test_prepostME");

    rc = PtlMDRelease(md_handle);
    PTL_CHECK(rc, "PtlMDRelease in test_prepostME");

    rc = PtlEQFree(eq_handle);
    PTL_CHECK(rc, "PtlEQFree in test_prepostME");

    rc= PtlPTFree(ni, index);
    PTL_CHECK(rc, "PtlPTFree in test_prepostME");

    tmp= __PtlAllreduceDouble(total, PTL_SUM);
    display_result("pre-post", (niters * npeers * nmsgs * 2) / (tmp / world_size));
}

static void postME(
    ptl_handle_ni_t ni,
    ptl_pt_index_t index,
    void *start,
    ptl_size_t length,
    ptl_process_t src,
    int tag,
    ptl_handle_me_t *mh)
{
    int rc;
    ptl_me_t me;

    unsigned int options = PTL_ME_OP_PUT | PTL_ME_ACK_DISABLE |
                    PTL_ME_USE_ONCE | PTL_ME_EVENT_UNLINK_DISABLE;
    me.start = (char *)start;
    me.length = length;
    me.ct_handle = PTL_CT_NONE;
    me.min_free = 0;
    me.ac_id.uid = PTL_UID_ANY;
    me.options = options;
    me.match_id = src;
    me.match_bits = tag;
    me.ignore_bits = 0;

    rc = PtlMEAppend(ni, index, &me, PTL_PRIORITY_LIST, NULL, mh);
    PTL_CHECK(rc, "Error in __PtlCreateME(): PtlMEAppend");
}

