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
#include "ptl_visibility.h"

static ptl_process_t COLLECTOR;
static volatile int runtime_inited = 0;
static int num_procs;
static int my_rank = -1;
static struct runtime_proc_t* ranks;
static ptl_handle_ni_t ni_physical;
static ptl_pt_index_t phys_pt_index;

#define NI_PHYS_NOMATCH 3

static void noFailures(
    ptl_handle_ct_t ct,
    ptl_size_t threshold,
    size_t line)
{
    ptl_ct_event_t ct_data;
    if (PTL_OK != PtlCTWait(ct, threshold, &ct_data)) abort();
    if (ct_data.failure != 0) {
	fprintf(stderr, "ct_data reports failure! {%u, %u} line %u\n",
		(unsigned int)ct_data.success, (unsigned int)ct_data.failure,
		(unsigned int)line);
	abort();
    }
}


void
runtime_init(void)
{
    int ret, i;
    ptl_process_t myself;
    uint64_t rank, maxrank;
    ptl_process_t *dmapping, *amapping;
    ptl_le_t le;
    ptl_handle_le_t le_handle;
    ptl_md_t md;
    ptl_handle_md_t md_handle;

    if (0 != PtlInternalAtomicInc(&runtime_inited, 1)) return;

    assert(getenv("PORTALS4_COLLECTOR_NID") != NULL);
    assert(getenv("PORTALS4_COLLECTOR_PID") != NULL);
    COLLECTOR.phys.nid = atoi(getenv("PORTALS4_COLLECTOR_NID"));
    COLLECTOR.phys.pid = atoi(getenv("PORTALS4_COLLECTOR_PID"));
    assert(getenv("PORTALS4_RANK") != NULL);
    rank = my_rank = atoi(getenv("PORTALS4_RANK"));
    assert(getenv("PORTALS4_NUM_PROCS") != NULL);
    num_procs = atoi(getenv("PORTALS4_NUM_PROCS"));
    maxrank = num_procs - 1;

    if (COLLECTOR.phys.pid == rank) return;
    
    ret = PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
		     PTL_PID_ANY, NULL, NULL, 0, NULL, NULL, &ni_physical);
    if (ret != PTL_OK) abort();

    ret = PtlGetId(ni_physical, &myself);
    if (ret != PTL_OK) abort();

    ret = PtlPTAlloc(ni_physical, 0, PTL_EQ_NONE, 0, &phys_pt_index);
    if (ret != PTL_OK) abort();
    assert(phys_pt_index == 0);

    dmapping = calloc(maxrank + 1, sizeof(ptl_process_t));
    assert(dmapping != NULL);
    amapping = calloc(maxrank + 1, sizeof(ptl_process_t));
    assert(amapping != NULL);

    /* for distributing my ID */
    md.start = &myself;
    md.length = sizeof(ptl_process_t);
    md.options = PTL_MD_EVENT_DISABLE | PTL_MD_EVENT_CT_SEND;	// count sends, but don't trigger events
    md.eq_handle = PTL_EQ_NONE;	       // i.e. don't queue send events
    ret = PtlCTAlloc(ni_physical, &md.ct_handle);
    if (ret != PTL_OK) abort();

    /* for receiving the mapping */
    le.start = dmapping;
    le.length = (maxrank + 1) * sizeof(ptl_process_t);
    le.ac_id.uid = PTL_UID_ANY;
    le.options = PTL_LE_OP_PUT | PTL_LE_USE_ONCE | PTL_LE_EVENT_DISABLE | PTL_LE_EVENT_CT_PUT;
    ret = PtlCTAlloc(ni_physical, &le.ct_handle);
    if (ret != PTL_OK) abort();

    /* post this now to avoid a race condition later */
    ret = PtlLEAppend(ni_physical, 0, le, PTL_PRIORITY_LIST, NULL,
                      &le_handle);
    if (ret != PTL_OK) abort();

    /* now send my ID to the collector */
    ret = PtlMDBind(ni_physical, &md, &md_handle);
    if (ret != PTL_OK) abort();

    ret = PtlPut(md_handle, 0, sizeof(ptl_process_t), PTL_OC_ACK_REQ,
                 COLLECTOR, phys_pt_index, 0,
                 rank * sizeof(ptl_process_t), NULL, 0);
    if (ret != PTL_OK) abort();

    /* wait for the send to finish */
    noFailures(md.ct_handle, 1, __LINE__);

    /* cleanup */
    ret = PtlMDRelease(md_handle);
    if (ret != PTL_OK) abort();

    ret = PtlCTFree(md.ct_handle);
    if (ret != PTL_OK) abort();

    /* wait to receive the mapping from the COLLECTOR */
    noFailures(le.ct_handle, 1, __LINE__);
    /* cleanup the counter */
    ret = PtlCTFree(le.ct_handle);
    if (ret != PTL_OK) abort();

    ranks = malloc(sizeof(struct runtime_proc_t) * num_procs);
    if (NULL == ranks) abort();
    for (i = 0 ; i < num_procs ; ++i) {
        ranks[i].nid = 0;
        ranks[i].pid = i;
    }
}

void
runtime_finalize(void)
{
    if (runtime_inited == 0 || COLLECTOR.phys.pid == my_rank) return;
    runtime_inited = 0;
    PtlPTFree(ni_physical, phys_pt_index);
    PtlNIFini(ni_physical);
}

int API_FUNC
runtime_get_rank(void)
{
    if (runtime_inited == 0) runtime_init();
    return my_rank;
}


int API_FUNC
runtime_get_size(void)
{
    if (runtime_inited == 0) runtime_init();
    return num_procs;
}


int API_FUNC
runtime_get_nidpid_map(struct runtime_proc_t**map)
{
    if (runtime_inited == 0) runtime_init();
    *map = ranks;
    return num_procs;
}


void API_FUNC runtime_barrier(
    void)
{
    ptl_handle_le_t leh;
    ptl_le_t le;
    ptl_handle_md_t mdh;
    ptl_md_t md;
    ptl_ct_event_t ctc;
    const ptl_internal_handle_converter_t ni = {.s =
	    {HANDLE_NI_CODE, NI_PHYS_NOMATCH, 0}
    };

    if (runtime_inited == 0) runtime_init();

    le.start = md.start = NULL;
    le.length = md.length = 0;
    le.ac_id.uid = PTL_UID_ANY;
    le.options = PTL_LE_OP_PUT | PTL_LE_USE_ONCE | PTL_LE_EVENT_CT_PUT;
    md.options = PTL_MD_EVENT_DISABLE;
    le.ct_handle = md.ct_handle = PTL_CT_NONE;
    md.eq_handle = PTL_EQ_NONE;
    ptl_assert(PtlCTAlloc(ni.a.ni, &le.ct_handle), PTL_OK);
    /* post my sensor */
    ptl_assert(PtlLEAppend(ni.a.ni, 0, le, PTL_PRIORITY_LIST, NULL, &leh),
	   PTL_OK);
    /* prepare my messenger */
    ptl_assert(PtlMDBind(ni.a.ni, &md, &mdh), PTL_OK);
    /* alert COLLECTOR of my presence */
    ptl_assert(PtlPut(mdh, 0, 0, PTL_CT_ACK_REQ, COLLECTOR, 0, 0, 0, NULL, 0),
	   PTL_OK);
    /* wait for COLLECTOR to respond */
    ptl_assert(PtlCTWait(le.ct_handle, 1, &ctc), PTL_OK);
    assert(ctc.failure == 0);
    ptl_assert(PtlMDRelease(mdh), PTL_OK);
    ptl_assert(PtlCTFree(le.ct_handle), PTL_OK);
}
