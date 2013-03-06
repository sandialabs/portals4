#include <portals4.h>
#include <support.h>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "testing.h"

#if MATCHING == 1
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

int main(int   argc,
         char *argv[])
{
    ptl_handle_ni_t ni_h;
    ptl_process_t   myself;
    ptl_pt_index_t  logical_pt_index;
    uint64_t        value, writeval;
    ENTRY_T         value_e;
    HANDLE_T        value_e_handle;
    ptl_md_t        write_md;
    ptl_handle_md_t write_md_handle;
    int             num_procs;
    ptl_ct_event_t ctc;
    ptl_process_t *procs;

    CHECK_RETURNVAL(PtlInit());

    CHECK_RETURNVAL(libtest_init());

    num_procs = libtest_get_size();

#if PHYSICAL_ADDR == 0
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_LOGICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));
#else
    CHECK_RETURNVAL(PtlNIInit(PTL_IFACE_DEFAULT, NI_TYPE | PTL_NI_PHYSICAL,
                              PTL_PID_ANY, NULL, NULL, &ni_h));
#endif

    procs = libtest_get_mapping(ni_h);

#if PHYSICAL_ADDR == 0
    CHECK_RETURNVAL(PtlSetMap(ni_h, num_procs, procs));
#endif

    CHECK_RETURNVAL(PtlGetId(ni_h, &myself));

    CHECK_RETURNVAL(PtlPTAlloc(ni_h, 0, PTL_EQ_NONE, PTL_PT_ANY,
                               &logical_pt_index));
    assert(logical_pt_index == 0);

    value_e.start  = &value;
    value_e.length = sizeof(uint64_t);
    value_e.uid    = PTL_UID_ANY;
#if MATCHING == 1
    value_e.match_id = myself;
    value_e.match_bits    = 1;
    value_e.ignore_bits   = 0;
#endif
    value_e.options = OPTIONS;
    CHECK_RETURNVAL(PtlCTAlloc(ni_h, &value_e.ct_handle));
    CHECK_RETURNVAL(APPEND(ni_h, 0, &value_e, PTL_PRIORITY_LIST, NULL,
                           &value_e_handle));

    writeval = 12345;
    value = 0;

    libtest_barrier();

    /* set up the landing pad so that I can read others' values */
    write_md.start     = &writeval;
    write_md.length    = sizeof(uint64_t);
    write_md.options   = PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_CT_ACK;
    write_md.eq_handle = PTL_EQ_NONE;   // i.e. don't queue send events
    CHECK_RETURNVAL(PtlCTAlloc(ni_h, &write_md.ct_handle));
    CHECK_RETURNVAL(PtlMDBind(ni_h, &write_md, &write_md_handle));

    /* write to myself */
    CHECK_RETURNVAL(PtlPut(write_md_handle, 0, sizeof(uint64_t), PTL_CT_ACK_REQ, myself,
                           logical_pt_index, 1, 0, NULL, 0));
    CHECK_RETURNVAL(PtlCTWait(write_md.ct_handle, 2, &ctc));
    assert(ctc.failure == 0);

    /* wait for write to arrive */
    CHECK_RETURNVAL(PtlCTWait(value_e.ct_handle, 1, &ctc));
    assert(ctc.failure == 0);

    assert(value == writeval);

    CHECK_RETURNVAL(PtlMDRelease(write_md_handle));
    CHECK_RETURNVAL(PtlCTFree(write_md.ct_handle));
    CHECK_RETURNVAL(UNLINK(value_e_handle));
    CHECK_RETURNVAL(PtlCTFree(value_e.ct_handle));

    /* cleanup */
    CHECK_RETURNVAL(PtlPTFree(ni_h, logical_pt_index));
    CHECK_RETURNVAL(PtlNIFini(ni_h));
    CHECK_RETURNVAL(libtest_fini());
    PtlFini();

    return 0;
}

/* vim:set expandtab: */
