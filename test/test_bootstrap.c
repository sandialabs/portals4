#include <portals4.h>
#include <support/support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "testing.h"

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t        ni_logical;
    ptl_process_t          myself;
    ptl_pt_index_t         logical_pt_index;
    ptl_process_t         *dmapping, *amapping;
    int                    my_rank, num_procs, i;
    ptl_process_t *rtprocs;
    int             my_ret;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    my_rank   = libtest_get_rank();
    num_procs = libtest_get_size();
    rtprocs = libtest_get_mapping();

    dmapping = malloc(sizeof(ptl_process_t) * num_procs);
    amapping = malloc(sizeof(ptl_process_t) * num_procs);

    /* ask for inverse of what the runtime gave us (not that we'll get it) */
    for (i = 0; i < num_procs; ++i) {
        dmapping[i] = rtprocs[num_procs - i - 1];
    }

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING |
                              PTL_NI_LOGICAL, PTL_PID_ANY, NULL, NULL,
                              //num_procs, dmapping, amapping,
                              &ni_logical));

    my_ret = PtlGetMap(ni_logical, 0, NULL, NULL);
    if (my_ret == PTL_NO_SPACE) {
        ptl_process_t *amapping;
        amapping = libtest_get_mapping();
        CHECK_RETURNVAL(PtlSetMap(ni_logical, num_procs, amapping));
        free(amapping);
    } else {
        CHECK_RETURNVAL(my_ret);
    }

    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    assert(myself.rank == my_rank);
    /*for (i = 0; i < num_procs; ++i) {
     *  printf("%3u's requested[%03i] = {%3u,%3u} actual[%03i] = {%3u,%3u}\n",
     *         (unsigned int)myself.rank, i, dmapping[i].phys.nid,
     *         dmapping[i].phys.pid, i, amapping[i].phys.nid,
     *         amapping[i].phys.pid);
     * }*/
    CHECK_RETURNVAL(PtlPTAlloc
                        (ni_logical, 0, PTL_EQ_NONE, 0, &logical_pt_index));
    assert(logical_pt_index == 0);

    libtest_barrier();
    libtest_barrier();

    /* now I can communicate between ranks with ni_logical */
    /* ... do stuff ... */

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    free(amapping);
    free(dmapping);

    return 0;
}

/* vim:set expandtab: */
