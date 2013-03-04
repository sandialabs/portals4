#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "testing.h"

# define ENTRY_T  ptl_me_t
# define HANDLE_T ptl_handle_me_t
# define NI_TYPE  PTL_NI_MATCHING
# define OPTIONS  (PTL_ME_OP_GET | PTL_ME_EVENT_CT_COMM)
# define APPEND   PtlMEAppend
# define UNLINK   PtlMEUnlink

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_logical;
    ptl_process_t   myself;
    ptl_pt_index_t  logical_pt_index;
    ptl_me_t        me;
    ptl_handle_me_t me_h;
    ptl_md_t        md;
    ptl_handle_md_t md_h;
    int             num_procs;
    int             rank;
    ptl_handle_eq_t eq_h;
    ptl_handle_ct_t ct_h;
    ptl_ct_event_t ct;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    rank = libtest_get_rank();
    num_procs = libtest_get_size();

    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_logical));

    CHECK_RETURNVAL(PtlSetMap(ni_logical, num_procs,
                              libtest_get_mapping(ni_logical)));

    CHECK_RETURNVAL(PtlGetId(ni_logical, &myself));

    if (0 == rank) {
        CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY,
                                   &logical_pt_index));

        CHECK_RETURNVAL(PtlEQAlloc(ni_logical, 1024, &eq_h));
        CHECK_RETURNVAL(PtlCTAlloc(ni_logical, &ct_h));

        me.start = NULL;
        me.length = 0;
        me.ct_handle = PTL_CT_NONE;
        me.uid = PTL_UID_ANY;
        me.options = PTL_ME_OP_PUT;
        me.match_id.rank = PTL_RANK_ANY;
        me.match_bits = 0;
        me.ignore_bits = 0;
        me.min_free = 0;
        CHECK_RETURNVAL(PtlMEAppend(ni_logical, 0, &me, PTL_PRIORITY_LIST, NULL,
                                    &me_h));

        md.start = NULL;
        md.length = 0;
        md.options = PTL_MD_EVENT_SUCCESS_DISABLE | PTL_MD_EVENT_CT_ACK;
        md.eq_handle = eq_h;
        md.ct_handle = ct_h;
        CHECK_RETURNVAL(PtlMDBind(ni_logical, &md, &md_h));

        CHECK_RETURNVAL(PtlPut(md_h, 0, 0, PTL_ACK_REQ, myself,
                               logical_pt_index, 0, 0, NULL, 0));
        CHECK_RETURNVAL(PtlCTWait(ct_h, 1, &ct));
        assert(1 == ct.success && 0 == ct.failure);

        CHECK_RETURNVAL(PtlMDRelease(md_h));
        CHECK_RETURNVAL(PtlMEUnlink(me_h));
        CHECK_RETURNVAL(PtlCTFree(ct_h));
        CHECK_RETURNVAL(PtlEQFree(eq_h));

        CHECK_RETURNVAL(PtlPTFree(ni_logical, logical_pt_index));
    }

    libtest_barrier();

    /* cleanup */
    CHECK_RETURNVAL(PtlNIFini(ni_logical));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

