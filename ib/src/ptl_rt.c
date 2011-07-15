/*
 * ptl_rt.c
 */

#include "ptl_loc.h"

int PtlSetJid(ptl_handle_ni_t ni_handle, ptl_jid_t jid)
{
	int err;
	ni_t *ni;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (err) {
		WARN();
		return err;
	}

	err = to_ni(ni_handle, &ni);
	if (unlikely(err)) {
		WARN();
		goto err1;
	}

	if (!ni) {
		WARN();
		err = PTL_ARG_INVALID;
		goto err1;
	}

	ni->rt.jid = jid;

	if (debug)
		printf("setting jid = %d\n", jid);

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}
