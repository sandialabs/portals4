/**
 * @file ptl_id.c
 *
 * @brief Portals ID APIs.
 */

#include "ptl_loc.h"

/**
 * @brief Get the user identifier of a process.
 *
 * @param[in] ni_handle The handle of the NI.
 * @param[out] uid_p The address of the returned user id.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * @return PTL_NO_INIT Indicates that the portals API has not been successfully initialized.
 */
int _PtlGetUid(PPEGBL ptl_handle_ni_t ni_handle, ptl_uid_t *uid_p)
{
	int err;
	ni_t *ni;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ni(MYGBL_ ni_handle, &ni);
	if (err)
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ni = fast_to_obj(ni_handle);
#endif

	*uid_p = ni->uid;

	err = PTL_OK;
	ni_put(ni);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Get the process identifier of a process.
 *
 * @param[in] ni_handle The handle of the NI.
 * @param[out] id_p The address of the returned process id.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * @return PTL_NO_INIT Indicates that the portals API has not been successfully initialized.
 */
int _PtlGetId(PPEGBL ptl_handle_ni_t ni_handle, ptl_process_t *id_p)
{
	int err;
	ni_t *ni;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ni(MYGBL_ ni_handle, &ni);
	if (err)
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ni = fast_to_obj(ni_handle);
#endif

	*id_p = ni->id;

	err = PTL_OK;
	ni_put(ni);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}

/**
 * @brief Get the process identifier of a process.
 *
 * @param[in] ni_handle The handle of the NI.
 * @param[out] id_p The address of the returned process id.
 *
 * @return PTL_OK Indicates success.
 * @return PTL_ARG_INVALID Indicates that an invalid argument was passed.
 * @return PTL_NO_INIT Indicates that the portals API has not been successfully initialized.
 */
int _PtlGetPhysId(PPEGBL ptl_handle_ni_t ni_handle, ptl_process_t *id_p)
{
	int err;
	ni_t *ni;

#ifndef NO_ARG_VALIDATION
	err = gbl_get();
	if (err)
		goto err0;

	err = to_ni(MYGBL_ ni_handle, &ni);
	if (err)
		goto err1;

	if (!ni) {
		err = PTL_ARG_INVALID;
		goto err1;
	}
#else
	ni = fast_to_obj(ni_handle);
#endif

	*id_p = ni->iface->id;

	err = PTL_OK;
	ni_put(ni);
#ifndef NO_ARG_VALIDATION
err1:
	gbl_put();
err0:
#endif
	return err;
}
