/*
 * ptl_handle.c
 */

#include "ptl_loc.h"

/* can return
PTL_OK
PTL_FAIL
*/
int PtlHandleIsEqual(ptl_handle_any_t handle1,
		     ptl_handle_any_t handle2)
{
	return (handle1 == handle2) ? PTL_OK : PTL_FAIL;
}
