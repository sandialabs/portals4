/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <limits.h>		       /* for UINT_MAX */

/* Internals */
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"

const ptl_uid_t PTL_UID_ANY = UINT_MAX;

int PtlGetId(ptl_handle_ni_t	ni_handle,
	     ptl_process_id_t*	id)
{
    if (comm_pad == NULL) {
	return PTL_NO_INIT;
    }
    if (ni_handle > 3 || nit.refcount[ni_handle] == 0) {
	return PTL_ARG_INVALID;
    }
    switch (ni_handle) {
	case 0: case 1:
	    id->rank = proc_number; // heh
	    break;
	case 2: case 3:
	    id->phys.pid = proc_number;
	    id->phys.nid = 0;
	    break;
	default:
	    *(int *)0 = 0; // should never happen
    }
    return PTL_OK;
}
