/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <limits.h>		       /* for UINT_MAX */

/* Internals */
#include "ptl_visibility.h"

const ptl_handle_eq_t PTL_EQ_NONE = UINT_MAX;

int API_FUNC PtlEQAlloc(
    ptl_handle_ni_t ni_handle,
    ptl_size_t count,
    ptl_handle_eq_t * eq_handle)
{
    return PTL_FAIL;
}

int API_FUNC PtlEQFree(
    ptl_handle_eq_t eq_handle)
{
    return PTL_FAIL;
}

int API_FUNC PtlEQGet(
    ptl_handle_eq_t eq_handle,
    ptl_event_t * event)
{
    return PTL_FAIL;
}

int API_FUNC PtlEQWait(
    ptl_handle_eq_t eq_handle,
    ptl_event_t * event)
{
    return PTL_FAIL;
}

int API_FUNC PtlEQPoll(
    ptl_handle_eq_t * eq_handles,
    int size,
    ptl_time_t timeout,
    ptl_event_t * event,
    int *which)
{
    return PTL_FAIL;
}
