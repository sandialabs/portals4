#include "test_prepost.h"
#include <stdlib.h>

static void postME(ptl_handle_ni_t  ni,
                   ptl_pt_index_t   index,
                   void            *start,
                   ptl_size_t       length,
                   ptl_process_t    src,
                   int              tag,
                   ptl_handle_me_t *mh);

void test_prepostME(int             cache_size,
                    int            *cache_buf,
                    ptl_handle_ni_t ni,
                    int             npeers,
                    int             nmsgs,
                    int             nbytes,
                    int             niters)
{
    int    i, j, k;
    double tmp, total = 0;

    ptl_handle_md_t send_md_handle;
    ptl_md_t        send_md;
    ptl_process_t   dest;
    ptl_size_t      offset;
    ptl_pt_index_t  index;
    ptl_handle_eq_t recv_eq_handle;
    ptl_handle_me_t me_handles[npeers * nmsgs];
    ptl_event_t     event;

    ptl_assert(PtlEQAlloc(ni, nmsgs * npeers + 1,
                          &send_md.eq_handle), PTL_OK);

    send_md.start     = send_buf;
    send_md.length    = SEND_BUF_SIZE;
    send_md.options   = PTL_MD_UNORDERED;
    send_md.ct_handle = PTL_CT_NONE;

    ptl_assert(PtlMDBind(ni, &send_md, &send_md_handle), PTL_OK);

    ptl_assert(PtlEQAlloc(ni, nmsgs * npeers + 1, &recv_eq_handle), PTL_OK);

    ptl_assert(PtlPTAlloc(ni, 0, recv_eq_handle, TestSameDirectionIndex,
                          &index), PTL_OK);

    ptl_assert(TestSameDirectionIndex, index);

    tmp = timer();
    for (j = 0; j < npeers; ++j) {
        for (k = 0; k < nmsgs; ++k) {
            ptl_process_t src;
            src.rank = recv_peers[j];
            postME(ni, index, recv_buf + (nbytes * (k + j * nmsgs)),
                   nbytes, src, magic_tag, &me_handles[k + j * nmsgs]);
        }
    }
    total += (timer() - tmp);

    for (i = 0; i < niters - 1; ++i) {
        cache_invalidate(cache_size, cache_buf);

        libtest_Barrier();

        tmp = timer();
        for (j = 0; j < npeers; ++j) {
            for (k = 0; k < nmsgs; ++k) {
                offset    = (nbytes * (k + j * nmsgs));
                dest.rank = send_peers[npeers - j - 1],
                ptl_assert(libtest_Put_offset(send_md_handle, offset, nbytes,
                                           dest, index, magic_tag, offset), PTL_OK);
            }
        }

        /* wait for sends */
        for (j = 0; j < npeers * nmsgs; ++j) {
            ptl_assert(PtlEQWait(send_md.eq_handle, &event), PTL_OK);
            ptl_assert(event.type, PTL_EVENT_SEND);
        }

        /* wait for receives */
        for (j = 0; j < npeers * nmsgs; j++) {
            PtlEQWait(recv_eq_handle, &event);
        }

        for (j = 0; j < npeers; ++j) {
            for (k = 0; k < nmsgs; ++k) {
                ptl_process_t src;
                src.rank = recv_peers[j];
                postME(ni, index, recv_buf + (nbytes * (k + j * nmsgs)),
                       nbytes, src, magic_tag, &me_handles[k + j * nmsgs]);
            }
        }
        total += (timer() - tmp);
    }

    libtest_Barrier();

    tmp = timer();
    for (j = 0; j < npeers; ++j) {
        for (k = 0; k < nmsgs; ++k) {
            offset    = (nbytes * (k + j * nmsgs));
            dest.rank = send_peers[npeers - j - 1],
            ptl_assert(libtest_Put_offset(send_md_handle, offset, nbytes, dest,
                                       index, magic_tag, offset), PTL_OK);
        }
    }
    /* wait for sends */
    for (j = 0; j < npeers * nmsgs; ++j) {
        ptl_assert(PtlEQWait(send_md.eq_handle, &event), PTL_OK);
        ptl_assert(event.type, PTL_EVENT_SEND);
    }

    /* wait for receives */
    for (j = 0; j < npeers * nmsgs; j++) {
        PtlEQWait(recv_eq_handle, &event);
    }

    total += (timer() - tmp);

    ptl_assert(PtlEQFree(send_md.eq_handle), PTL_OK);

    ptl_assert(PtlMDRelease(send_md_handle), PTL_OK);

    ptl_assert(PtlEQFree(recv_eq_handle), PTL_OK);

    ptl_assert(PtlPTFree(ni, index), PTL_OK);

    tmp = libtest_AllreduceDouble(total, PTL_SUM);
    display_result("pre-post", (niters * npeers * nmsgs * 2) / (tmp / world_size));
}

static void postME(ptl_handle_ni_t  ni,
                   ptl_pt_index_t   index,
                   void            *start,
                   ptl_size_t       length,
                   ptl_process_t    src,
                   int              tag,
                   ptl_handle_me_t *mh)
{
    int      rc;
    ptl_me_t me;

    unsigned int options = PTL_ME_OP_PUT | PTL_ME_ACK_DISABLE |
                           PTL_ME_USE_ONCE | PTL_ME_EVENT_UNLINK_DISABLE;

    me.start       = (char *)start;
    me.length      = length;
    me.ct_handle   = PTL_CT_NONE;
    me.min_free    = 0;
    me.uid         = PTL_UID_ANY;
    me.options     = options;
    me.match_id    = src;
    me.match_bits  = tag;
    me.ignore_bits = 0;

    rc = PtlMEAppend(ni, index, &me, PTL_PRIORITY_LIST, NULL, mh);
    LIBTEST_CHECK(rc, "Error in libtest_CreateME(): PtlMEAppend");
}

/* vim:set expandtab: */
