#include "test_one_way.h"

void test_one_wayLE(int             cache_size,
                    int            *cache_buf,
                    ptl_handle_ni_t ni,
                    int             npeers,
                    int             nmsgs,
                    int             nbytes,
                    int             niters)
{
    double tmp, total = 0;

    Debug("\n");

    libtest_Barrier();
    if (rank < (world_size / 2)) {
        int             i;
        ptl_handle_md_t md_handle;
        ptl_handle_ct_t ct_handle = PTL_INVALID_HANDLE;

        libtest_CreateMDCT(ni, send_buf, SEND_BUF_SIZE, &md_handle, &ct_handle);

        for (i = 0; i < niters; ++i) {
            int            k;
            ptl_ct_event_t cnt_value;

            cache_invalidate(cache_size, cache_buf);

            libtest_Barrier();
            tmp = timer();
            for (k = 0; k < nmsgs; k++) {
                ptl_size_t    offset = nbytes * k;
                ptl_process_t dest;
                dest.rank = rank + (world_size / 2);
                ptl_assert(libtest_Put_offset(md_handle, offset, nbytes, dest,
                                           TestOneWayIndex, magic_tag, offset), PTL_OK);
            }

            ptl_assert(PtlCTWait(ct_handle, (i + 1) * nmsgs, &cnt_value),
                       PTL_OK);

            total += (timer() - tmp);
        }

        ptl_assert(PtlCTFree(ct_handle), PTL_OK);
        ptl_assert(PtlMDRelease(md_handle), PTL_OK);
    } else {
        int             i;
        ptl_pt_index_t  index;
        ptl_handle_le_t le_handle;
        ptl_le_t        le;

        ptl_assert(PtlCTAlloc(ni, &le.ct_handle), PTL_OK);

        ptl_assert(PtlPTAlloc(ni, 0, PTL_EQ_NONE, TestOneWayIndex,
                              &index), PTL_OK);
        ptl_assert(index, TestOneWayIndex);

        le.start   = recv_buf;
        le.length  = RECV_BUF_SIZE;
        le.uid     = PTL_UID_ANY;
        le.options = PTL_LE_OP_PUT | PTL_LE_ACK_DISABLE |
                     PTL_LE_EVENT_CT_COMM;
        ptl_assert(PtlLEAppend(ni, index, &le, PTL_PRIORITY_LIST,
                               NULL, &le_handle), PTL_OK);

        for (i = 0; i < niters; ++i) {
            ptl_ct_event_t cnt_value;

            cache_invalidate(cache_size, cache_buf);

            libtest_Barrier();
            tmp = timer();

            ptl_assert(PtlCTWait(le.ct_handle, (i + 1) * nmsgs,
                                 &cnt_value), PTL_OK);
            ptl_assert(cnt_value.failure, 0);

            total += (timer() - tmp);
        }

        ptl_assert(PtlLEUnlink(le_handle), PTL_OK);
        ptl_assert(PtlPTFree(ni, index), PTL_OK);
        ptl_assert(PtlCTFree(le.ct_handle), PTL_OK);
    }

    tmp = libtest_AllreduceDouble(total, PTL_SUM);

#if 0
    printf("%s %.1f ns\n", rank == 0 ? "send" : "recv",
           total / (double)(niters * nmsgs) * 1000000000.0);

    if ( rank == 0 ) {
        printf("avg %.1f ns\n", tmp / (double)(niters * nmsgs) * 1000000000.0 / 2.0);
    }
#endif

    display_result("single direction",
                   (niters * nmsgs) / (tmp / world_size));

    libtest_Barrier();
}

/* vim:set expandtab: */
