/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>

/* Internals */
#include "ptl_visibility.h"

int API_FUNC PtlMEAppend(
    ptl_handle_ni_t ni_handle,
    ptl_pt_index_t pt_index,
    ptl_me_t me,
    ptl_list_t ptl_list,
    void *user_ptr,
    ptl_handle_me_t * me_handle)
{
    return PTL_FAIL;
}

int API_FUNC PtlMEUnlink(
    ptl_handle_me_t me_handle)
{
    return PTL_FAIL;
}
