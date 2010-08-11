#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <portals4.h>

/* System headers */
#include <assert.h>

/* Internal headers */
#include "ptl_internal_PT.h"
#include "ptl_internal_EQ.h"
#include "ptl_visibility.h"

#define PT_FREE     0
#define PT_ENABLED  1
#define PT_DISABLED 2

int API_FUNC PtlPTAlloc(
    ptl_handle_ni_t ni_handle,
    unsigned int options,
    ptl_handle_eq_t eq_handle,
    ptl_pt_index_t pt_index_req,
    ptl_pt_index_t * pt_index)
{
    return PTL_FAIL;
}

int API_FUNC PtlPTFree(
    ptl_handle_ni_t ni_handle,
    ptl_pt_index_t pt_index)
{
    return PTL_FAIL;
}

int API_FUNC PtlPTDisable(
    ptl_handle_ni_t ni_handle,
    ptl_pt_index_t pt_index)
{
    return PTL_FAIL;
}

int API_FUNC PtlPTEnable(
    ptl_handle_ni_t ni_handle,
    ptl_pt_index_t pt_index)
{
    return PTL_FAIL;
}

void INTERNAL PtlInternalPTInit(
    ptl_table_entry_t * t)
{
    assert(pthread_mutex_init(&t->lock, NULL) == 0);
    t->priority.head = NULL;
    t->priority.tail = NULL;
    t->overflow.head = NULL;
    t->overflow.tail = NULL;
    t->EQ = PTL_EQ_NONE;
    t->status = PT_FREE;
}

int INTERNAL PtlInternalPTValidate(
    ptl_table_entry_t * t)
{
    if (PtlInternalEQHandleValidator(t->EQ, 1)) {
        return 3;
    }
    switch (t->status) {
        case PT_FREE:
            return 1;
        case PT_DISABLED:
            return 2;
        case PT_ENABLED:
            return 0;
        default:
            // should never happen
            *(int*)0 = 0;
            return 3;
    }
}
