#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <portals4.h>
#include <portals4_runtime.h>

/* System headers */
#include <stdlib.h>
#include <stdio.h>

/* Internal headers */
#include "ptl_internal_assert.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_CT.h"
#include "ptl_visibility.h"

static ptl_process_t          COLLECTOR;
static volatile int           runtime_inited = 0;
static int                    num_procs;
static int                    my_rank = -1;
static struct runtime_proc_t *ranks;
static ptl_handle_ni_t        ni_physical;
static ptl_pt_index_t         phys_pt_index;

static long            barrier_count = 0;
static ptl_handle_le_t barrier_le_h;
static ptl_handle_ct_t barrier_ct_h;
static ptl_handle_ct_t barrier_ct_h2;
static ptl_handle_md_t barrier_md_h;

#define NI_PHYS_NOMATCH   3
#define __PtlBarrierIndex (14)

static void noFailures(ptl_handle_ct_t ct,
                       ptl_size_t      threshold,
                       size_t          line)
{
    ptl_ct_event_t ct_data;

    if (PTL_OK != PtlCTWait(ct, threshold, &ct_data)) {
        abort();
    }
    if (ct_data.failure != 0) {
        fprintf(stderr, "ct_data reports failure! {%u, %u} line %u\n",
                (unsigned int)ct_data.success, (unsigned int)ct_data.failure,
                (unsigned int)line);
        abort();
    }
}

void runtime_init(void)
{
    int             ret, i;
    ptl_process_t   myself;
    uint64_t        rank, maxrank;
    ptl_process_t  *dmapping, *amapping;
    ptl_le_t        le;
    ptl_handle_le_t le_handle;
    ptl_md_t        md;
    ptl_handle_md_t md_handle;

    if (0 != PtlInternalAtomicInc(&runtime_inited, 1)) {
        return;
    }

    assert(getenv("PORTALS4_COLLECTOR_NID") != NULL);
    assert(getenv("PORTALS4_COLLECTOR_PID") != NULL);
    COLLECTOR.phys.nid = atoi(getenv("PORTALS4_COLLECTOR_NID"));
    COLLECTOR.phys.pid = atoi(getenv("PORTALS4_COLLECTOR_PID"));
    assert(getenv("PORTALS4_RANK") != NULL);
    rank = my_rank = atoi(getenv("PORTALS4_RANK"));
    assert(getenv("PORTALS4_NUM_PROCS") != NULL);
    num_procs = atoi(getenv("PORTALS4_NUM_PROCS"));
    maxrank   = num_procs - 1;

    if (COLLECTOR.phys.pid == rank) {
        return;
    }

    ret =
        PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
                  PTL_PID_ANY, NULL, NULL, 0, NULL, NULL, &ni_physical);
    if (ret != PTL_OK) {
        abort();
    }

    ret = PtlGetId(ni_physical, &myself);
    if (ret != PTL_OK) {
        abort();
    }

    ret = PtlPTAlloc(ni_physical, 0, PTL_EQ_NONE, 0, &phys_pt_index);
    if (ret != PTL_OK) {
        abort();
    }
    assert(phys_pt_index == 0);

    dmapping = calloc(maxrank + 1, sizeof(ptl_process_t));
    assert(dmapping != NULL);
    amapping = calloc(maxrank + 1, sizeof(ptl_process_t));
    assert(amapping != NULL);

    /* for the runtime_barrier() */
    {
        ptl_pt_index_t index;
        ptl_md_t       md = {
            .start     = NULL, .length = 0, .options = PTL_MD_UNORDERED | PTL_MD_REMOTE_FAILURE_DISABLE,
            .eq_handle = PTL_EQ_NONE, .ct_handle = PTL_CT_NONE
        };
        ptl_assert(PtlMDBind(ni_physical, &md, &barrier_md_h), PTL_OK);
        /* We want a specific Portals table entry */
        ptl_assert(PtlPTAlloc(ni_physical, 0, PTL_EQ_NONE, __PtlBarrierIndex, &index), PTL_OK);
        assert(index == __PtlBarrierIndex);
    }
    {
        ptl_le_t le = {
            .start     = NULL,
            .length    = 0,
            .ac_id.uid = PTL_UID_ANY,
            .options   = PTL_LE_OP_PUT | PTL_LE_ACK_DISABLE | PTL_LE_EVENT_CT_COMM
        };
        ptl_assert(PtlCTAlloc(ni_physical, &barrier_ct_h2), PTL_OK);
        le.ct_handle = barrier_ct_h2;
        ptl_assert(PtlLEAppend(ni_physical, __PtlBarrierIndex, &le, PTL_PRIORITY_LIST, NULL, &barrier_le_h), PTL_OK);
    }

    /* for distributing my ID */
    md.start     = &myself;
    md.length    = sizeof(ptl_process_t);
    md.options   = PTL_MD_EVENT_CT_SEND; // count sends
    md.eq_handle = PTL_EQ_NONE;        // i.e. don't queue send events
    ret          = PtlCTAlloc(ni_physical, &md.ct_handle);
    if (ret != PTL_OK) {
        abort();
    }

    /* for receiving the mapping */
    le.start     = dmapping;
    le.length    = (maxrank + 1) * sizeof(ptl_process_t);
    le.ac_id.uid = PTL_UID_ANY;
    le.options   =
        PTL_LE_OP_PUT | PTL_LE_USE_ONCE | PTL_LE_EVENT_COMM_DISABLE |
        PTL_LE_EVENT_CT_COMM;
    ret = PtlCTAlloc(ni_physical, &le.ct_handle);
    if (ret != PTL_OK) {
        abort();
    }

    /* post this now to avoid a race condition later */
    ret =
        PtlLEAppend(ni_physical, 0, &le, PTL_PRIORITY_LIST, NULL, &le_handle);
    if (ret != PTL_OK) {
        abort();
    }

    /* now send my ID to the collector */
    ret = PtlMDBind(ni_physical, &md, &md_handle);
    if (ret != PTL_OK) {
        abort();
    }

    ret =
        PtlPut(md_handle, 0, sizeof(ptl_process_t), PTL_OC_ACK_REQ, COLLECTOR,
               phys_pt_index, 0, rank * sizeof(ptl_process_t), NULL, 0);
    if (ret != PTL_OK) {
        abort();
    }

    /* wait for the send to finish */
    noFailures(md.ct_handle, 1, __LINE__);

    /* cleanup */
    ret = PtlMDRelease(md_handle);
    if (ret != PTL_OK) {
        abort();
    }

    ret = PtlCTFree(md.ct_handle);
    if (ret != PTL_OK) {
        abort();
    }

    /* wait to receive the mapping from the COLLECTOR */
    noFailures(le.ct_handle, 1, __LINE__);
    /* cleanup the counter */
    ret = PtlCTFree(le.ct_handle);
    if (ret != PTL_OK) {
        abort();
    }

    ranks = malloc(sizeof(struct runtime_proc_t) * num_procs);
    if (NULL == ranks) {
        abort();
    }
    for (i = 0; i < num_procs; ++i) {
        ranks[i].nid = 0;
        ranks[i].pid = i;
    }
}

void runtime_finalize(void)
{
    if ((runtime_inited == 0) || (COLLECTOR.phys.pid == my_rank)) {
        return;
    }
    runtime_inited = 0;

    if (barrier_count > 0) {
        ptl_assert(PtlCTFree(barrier_ct_h), PTL_OK);
        ptl_assert(PtlCTFree(barrier_ct_h2), PTL_OK);
        ptl_assert(PtlLEUnlink(barrier_le_h), PTL_OK);
    }

    PtlPTFree(ni_physical, phys_pt_index);
    PtlNIFini(ni_physical);
}

int API_FUNC runtime_get_rank(void)
{
    if (runtime_inited == 0) {
        runtime_init();
    }
    return my_rank;
}

int API_FUNC runtime_get_size(void)
{
    if (runtime_inited == 0) {
        runtime_init();
    }
    return num_procs;
}

int API_FUNC runtime_get_nidpid_map(struct runtime_proc_t **map)
{
    if (runtime_inited == 0) {
        runtime_init();
    }
    *map = ranks;
    return num_procs;
}

void API_FUNC runtime_barrier(void)
{
    const ptl_internal_handle_converter_t ni = { .s = { HANDLE_NI_CODE, NI_PHYS_NOMATCH, 0 } };

    if (runtime_inited == 0) {
        runtime_init();
    }

    if (0 == barrier_count) {
        /* first barrier calls to yod to make sure everyone is present */
        ptl_le_t        le;
        ptl_handle_md_t mdh;
        ptl_md_t        md;
        ptl_ct_event_t  ctc;

        barrier_count = 1;
        le.start      = md.start = NULL;
        le.length     = md.length = 0;
        le.ac_id.uid  = PTL_UID_ANY;
        le.options    = PTL_LE_OP_PUT | PTL_LE_EVENT_CT_COMM;
        md.options    = 0;
        md.eq_handle  = PTL_EQ_NONE;
        md.ct_handle  = PTL_CT_NONE;
        ptl_assert(PtlCTAlloc(ni_physical, &barrier_ct_h), PTL_OK);
        le.ct_handle = barrier_ct_h;
        /* post my receive */
        ptl_assert(PtlLEAppend(ni.a, 0, &le, PTL_PRIORITY_LIST, NULL, &barrier_le_h),
                   PTL_OK);
        /* prepare my messenger */
        ptl_assert(PtlMDBind(ni.a, &md, &mdh), PTL_OK);
        /* alert COLLECTOR of my presence */
        ptl_assert(PtlPut(mdh, 0, 0, PTL_NO_ACK_REQ, COLLECTOR, 0, 0, 0, NULL, 0),
                   PTL_OK);
        /* wait for COLLECTOR to respond */
        ptl_assert(PtlCTWait(barrier_ct_h, 1, &ctc), PTL_OK);
        assert(ctc.failure == 0);
        ptl_assert(PtlMDRelease(mdh), PTL_OK);

        if (0 == my_rank) {
            /* to make counting easier */
            PtlInternalCTSuccessInc(barrier_ct_h, num_procs - 2);
        }
    } else {
        /* follow-on barrier calls only within user space */
        static ptl_size_t __barrier_cnt = 1;
        ptl_process_t     parent, leftchild, rightchild;
        ptl_size_t        test;
        ptl_ct_event_t    cnt_value;

        parent.phys.pid     = ((my_rank + 1) >> 1) - 1;
        parent.phys.nid     = 0;
        leftchild.phys.pid  = ((my_rank + 1) << 1) - 1;
        leftchild.phys.nid  = 0;
        rightchild.phys.pid = leftchild.phys.pid + 1;
        rightchild.phys.nid = 0;

        if (leftchild.phys.pid < num_procs) {
            /* Wait for my children to enter the barrier */
            test = __barrier_cnt++;
            if (rightchild.phys.pid < num_procs) {
                test = __barrier_cnt++;
            }
            ptl_assert(PtlCTWait(barrier_ct_h2, test, &cnt_value), PTL_OK);
        }

        if (my_rank > 0) {
            /* Tell my parent that I have entered the barrier */
            ptl_assert(PtlPut(barrier_md_h, 0, 0, PTL_NO_ACK_REQ, parent, __PtlBarrierIndex, 0, 0, NULL, 0), PTL_OK);

            /* Wait for my parent to wake me up */
            test = __barrier_cnt++;
            ptl_assert(PtlCTWait(barrier_ct_h2, test, &cnt_value), PTL_OK);
        }

        /* Wake my children */
        if (leftchild.phys.pid < num_procs) {
            ptl_assert(PtlPut(barrier_md_h, 0, 0, PTL_NO_ACK_REQ, leftchild, __PtlBarrierIndex, 0, 0, NULL, 0), PTL_OK);
            if (rightchild.phys.pid < num_procs) {
                ptl_assert(PtlPut(barrier_md_h, 0, 0, PTL_NO_ACK_REQ, rightchild, __PtlBarrierIndex, 0, 0, NULL, 0), PTL_OK);
            }
        }
    }
}

/* vim:set expandtab: */
