/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <assert.h>
#include <limits.h>		       /* for UINT_MAX */

const ptl_uid_t PTL_UID_ANY = UINT_MAX;

int PtlGetId(ptl_handle_ni_t	ni_handle,
	     ptl_process_id_t*	id)
{
    return PTL_FAIL;
}
