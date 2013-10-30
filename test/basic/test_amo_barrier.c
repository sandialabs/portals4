/*
 * Portals version of the dissemination barrier in Open SHMEM
 */

#include <portals4.h>
#include <support.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "testing.h"

const int tries = 50;
 

int
main(int argc, char *argv[])
{
    int rank, num_procs;
    ptl_handle_ni_t ni_h;
    ptl_pt_index_t  pt_index;
    ptl_le_t le;
    ptl_handle_le_t le_h;
    ptl_md_t        md;
    ptl_ct_event_t ct;
    ptl_process_t *procs;
    ptl_process_t peer;
    ptl_handle_md_t md_h;
    int8_t *psync_bytes;
    int8_t one = 1, neg_one = -1;
    int distance, to, i, j;
    size_t count = 0;
        
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

    psync_bytes = malloc(sizeof(int8_t) * sizeof(int) * 8);
    memset(psync_bytes, 0, sizeof(int8_t) * sizeof(int) * 8);

    le.start = psync_bytes;
    le.length = sizeof(int8_t) * sizeof(int) * 8;
    le.uid = PTL_UID_ANY;
    le.options = PTL_LE_OP_PUT | PTL_LE_OP_GET | PTL_LE_EVENT_CT_COMM;
    CHECK_RETURNVAL(PtlCTAlloc(ni_h, &le.ct_handle));
    CHECK_RETURNVAL(PtlLEAppend(ni_h, 0, &le, PTL_PRIORITY_LIST, NULL,
                                &le_h));

    md.start = NULL;
    md.length = PTL_SIZE_MAX;
    md.options = PTL_MD_EVENT_CT_ACK;
    md.eq_handle = PTL_EQ_NONE;
    CHECK_RETURNVAL(PtlCTAlloc(ni_h, &md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_h, &md, &md_h));

    libtest_barrier();

    for (j = 0 ; j < tries ; ++j) {
#ifdef DEBUG
        fprintf(stderr, "%d %03d start barrier\n", j, rank);
#endif
        for (i = 0, distance = 1 ; distance < num_procs ; ++i, distance <<=1) {
            to = (rank + distance) % num_procs;

            /*
             * shmem_internal_atomic_small(&pSync_bytes[i], &one, sizeof(int8_t),
             *                             to,
             *                             PTL_SUM, PTL_INT8_T);
             */
#ifdef DEBUG
            fprintf(stderr, "%d %03d iter %d send to %03d\n",
                    j, rank, i, to);
#endif
            peer.rank = to;
            CHECK_RETURNVAL(PtlAtomic(md_h,
                                      (ptl_size_t) &one,
                                      sizeof(int8_t),
                                      PTL_CT_ACK_REQ,
                                      peer,
                                      pt_index,
                                      0,
                                      i * sizeof(int8_t),
                                      NULL,
                                      0,
                                      PTL_SUM,
                                      PTL_INT8_T));

            /*
             * SHMEM_WAIT_UNTIL(&pSync_bytes[i], SHMEM_CMP_EQ, 1);
             */
#ifdef DEBUG
            fprintf(stderr, "%d %03d iter %d wait on %d\n",
                    j, rank, i, (int) psync_bytes[i]);
#endif
            while (psync_bytes[i] != 1) {
                __sync_synchronize();
                if (psync_bytes[i] > 2) abort();
            }
#ifdef DEBUG
            fprintf(stderr, "%d %03d iter %d done wait on %d\n",
                    j, rank, i, (int) psync_bytes[i]);
#endif

            /*
             * shmem_internal_atomic_small(&pSync_bytes[i], &neg_one, sizeof(int8_t),
             *                             shmem_internal_my_pe,
             *                             PTL_SUM, PTL_INT8_T);
             */
            peer.rank = rank;
            CHECK_RETURNVAL(PtlAtomic(md_h,
                                      (ptl_size_t) &neg_one,
                                      sizeof(int8_t),
                                      PTL_CT_ACK_REQ,
                                      peer,
                                      pt_index,
                                      0,
                                      i * sizeof(int8_t),
                                      NULL,
                                      0,
                                      PTL_SUM,
                                      PTL_INT8_T));
            count += 2;
        }

#ifdef DEBUG
        fprintf(stderr, "%d %03d end barrier\n",
                j, rank);
#endif

        /* shemm_quiet */
        PtlCTWait(md.ct_handle, count, &ct);

        if (0 == rank) {
            fprintf(stderr, "iteration %d complete\n", j);
            fflush(NULL);
        }
    }

    for (i = 0 ; i < sizeof(int) * 8 ; ++i) {
        if (0 != psync_bytes[i]) {
            fprintf(stderr, "%03d: psync_bytes[%d] is %d\n",
                    rank, i, psync_bytes[i]);
            abort();
        }
    }

    libtest_barrier();
    PtlLEUnlink(le_h);
    PtlMDRelease(md_h);
    if (PTL_CT_NONE != md.ct_handle) {
        PtlCTFree(md.ct_handle);
    }
    CHECK_RETURNVAL(PtlPTFree(ni_h, pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_h));

    libtest_barrier();
    if (rank == 0) {
        printf("test complete\n");
    }

    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}
