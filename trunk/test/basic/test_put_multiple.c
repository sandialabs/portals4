#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>

#include "testing.h"

#if INTERFACE == 1
# define ENTRY_T  ptl_me_t
# define HANDLE_T ptl_handle_me_t
# define NI_TYPE  PTL_NI_MATCHING
# define OPTIONS  (PTL_ME_OP_PUT | PTL_ME_EVENT_CT_COMM)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink
#else
# define ENTRY_T  ptl_le_t
# define HANDLE_T ptl_handle_le_t
# define NI_TYPE  PTL_NI_NO_MATCHING
# define OPTIONS  (PTL_LE_OP_PUT | PTL_LE_EVENT_CT_COMM)
# define APPEND   PtlLEAppend
# define UNLINK   PtlLEUnlink
#endif /* if INTERFACE == 1 */

#ifndef BUFSIZE
#error BUFSIZE must be defined
#endif
#ifndef OVERLAP
#error OVERLAP must be defined
#endif

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    unsigned char  *writeval, *value;
    ENTRY_T         value_e;
    HANDLE_T        value_e_handle;
    ptl_md_t        write_md;
    ptl_handle_md_t write_md_handle;
    int             num_procs, rank;
    ptl_process_t myself;
    ptl_pt_index_t logical_pt_index;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();

    value   = malloc(sizeof(unsigned char) * BUFSIZE * num_procs);
    assert(value);
    writeval = malloc(sizeof(unsigned char) * BUFSIZE);
    assert(writeval);

    memset(value, 42, BUFSIZE * num_procs);

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, 
                              NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY,
                              NULL, 
                              NULL, 
                              &ni_logical));
    
    CHECK_RETURNVAL(PtlSetMap(ni_logical, 
                              num_procs,
                              libtest_get_mapping(ni_logical)));

    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));
    CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 
                               0,
                               PTL_EQ_NONE,
                               PTL_PT_ANY,
                               &logical_pt_index));
    assert(logical_pt_index == 0);

    /* Now do the initial setup on ni_logical */
    value_e.start  = value;
    value_e.length = BUFSIZE * num_procs;
    value_e.uid    = PTL_UID_ANY;
#if INTERFACE == 1
    value_e.match_id.rank = PTL_RANK_ANY;
    value_e.match_bits    = 1;
    value_e.ignore_bits   = 0;
#endif
    value_e.options = OPTIONS;
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &value_e.ct_handle));
    CHECK_RETURNVAL(APPEND(ni_logical,
                           0,
                           &value_e,
                           PTL_PRIORITY_LIST,
                           NULL,
                           &value_e_handle));

    /* Now do a barrier to make sure that everyone has their logical
     * interface set up */
    libtest_barrier();

    memset(writeval, 61, BUFSIZE);
    write_md.start     = writeval;
    write_md.length    = BUFSIZE;
    write_md.options   = PTL_MD_EVENT_CT_ACK;
    write_md.eq_handle = PTL_EQ_NONE;
    CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &write_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_logical, &write_md, &write_md_handle));

    /* everyone puts to rank 0 */
    if (myself.rank !=0){
        ptl_ct_event_t ctc;
        ptl_process_t  r0 = { .rank = 0 };
        int i, iters;
        iters = 100;
        for (i=1; i <= iters; i++){
            CHECK_RETURNVAL(PtlPut(write_md_handle, 
                               0,
                               BUFSIZE, 
                               PTL_CT_ACK_REQ,
                               r0,
                               logical_pt_index,
                               1,
                               BUFSIZE * rank * OVERLAP,
                               NULL,
                               0));
            CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, i, &ctc));
        }
                assert(ctc.failure == 0);
        //assert(ctc.success == 1);
    }
    if (myself.rank == 0) {
        memset(value, 61, BUFSIZE); 
        NO_FAILURES(value_e.ct_handle, (num_procs-1)*100);
        for (unsigned idx = 0; idx < BUFSIZE * (0 == OVERLAP ? 1 : num_procs); ++idx) {
            if (value[idx] != 61) {
                fprintf(stderr,
                        "bad value at idx %u (readval[%u] = %i, should be 61)\n",
                        idx,
                        idx, value[idx]);
                abort();
            }
            //fprintf(stderr,"idx is: %i\n",idx);
        }
        fprintf(stderr,"rank 0 done listening\n");
    }
    
    CHECK_RETURNVAL(PtlMDRelease(write_md_handle));
    CHECK_RETURNVAL(PtlCTFree(write_md.ct_handle));
    CHECK_RETURNVAL(UNLINK(value_e_handle));
    CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle));

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
