/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System libraries */
#include <assert.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_atomic.h"

static unsigned int Init_Ref_Count = 0;

int API_FUNC PtlInit(void)
{
    unsigned int race = PtlInternalAtomicInc(&Init_Ref_Count,1);
    if (race == 1) {
	/* do stuff */
	return PTL_OK;
    } else {
	return PTL_OK;
    }
}

void API_FUNC PtlFini(void)
{
    assert(Init_Ref_Count > 0);
    if (Init_Ref_Count == 0) return;
    PtlInternalAtomicInc(&Init_Ref_Count, -1);
}
