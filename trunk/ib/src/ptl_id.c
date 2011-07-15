/*
 * ptl_id.c
 */

#include "ptl_loc.h"

/* can return
PTL OK
PTL NI INVALID
PTL NO INIT
PTL ARG_INVALID
*/
int PtlGetUid(ptl_handle_ni_t ni_handle, ptl_uid_t *uid)
{
	int err;
	ni_t *ni;
	gbl_t *gbl;

	err = get_gbl(&gbl);
	if (err)
		return err;

	err = to_ni(ni_handle, &ni);
	if (unlikely(err))
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}

	*uid = ni->uid;

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

/* can return
PTL OK
PTL NI INVALID
PTL NO INIT
PTL ARG_INVALID
*/
int PtlGetId(ptl_handle_ni_t ni_handle, ptl_process_t *id)
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

	*id = ni->id;

	if (debug) {
		printf("PtlGetId returned id = %x, %x\n", ni->id.phys.nid, ni->id.phys.pid);
	}

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}

/* can return
PTL OK
PTL NI INVALID
PTL NO INIT
PTL ARG_INVALID
*/
int PtlGetJid(ptl_handle_ni_t ni_handle, ptl_jid_t *jid)
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

	*jid = ni->rt.jid;

	ni_put(ni);
	gbl_put(gbl);
	return PTL_OK;

err1:
	gbl_put(gbl);
	return err;
}
