/*
 * This Cplant(TM) source code is part of the Portals3 Reference
 * Implementation.
 *
 * This Cplant(TM) source code is the property of Sandia National
 * Laboratories.
 *
 * This Cplant(TM) source code is copyrighted by Sandia National
 * Laboratories.
 *
 * The redistribution of this Cplant(TM) source code is subject to the
 * terms of version 2 of the GNU General Public License.
 * (See COPYING, or http://www.gnu.org/licenses/lgpl.html.)
 *
 * Cplant(TM) Copyright 1998-2006 Sandia Corporation. 
 *
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the US Government.
 * Export of this program may require a license from the United States
 * Government.
 */


/* Portals3 is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License,
 * as published by the Free Software Foundation.
 *
 * Portals3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Portals3; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Questions or comments about this library should be sent to:
 *
 * Jim Schutt
 * Sandia National Laboratories, New Mexico
 * P.O. Box 5800
 * Albuquerque, NM 87185-0806
 *
 * jaschut@sandia.gov
 *
 */
#include <p3-config.h>

#ifndef PTL_KERNEL_BLD
#include <sys/types.h>
#endif
#include <errno.h>
#include <limits.h>

/* If compiling for user-space (and maybe NIC-space), these need to be
 * the Portals3 versions of the Linux kernel include files, which
 * have been suitably modified for use in user-space code.
 */
#include <linux/list.h>

/* These are all Portals3 include files.
 */
#include <p3utils.h>

#include <p3api/types.h>
#include <p3api/debug.h>

#include <p3/lock.h>
#include <p3/handle.h>
#include <p3/process.h>
//#include <p3/forward.h>
#include <p3/errno.h>
#include <p3/debug.h>

#include <p3lib/types.h>
#include <p3lib/p3lib.h>
#include <p3lib/nal.h>
#include <p3lib/p3lib_support.h>

/* On the theory we shouldn't need to allow more NAL types than
 * we allow interfaces (arguable, I know), we'll have a static
 * NAL table with PTL_MAX_INTERFACES entries.
 *
 * If this ever proves to be a problem, we'll change it.
 */
nal_type_t p3nals[PTL_MAX_INTERFACES];

/* Helper function to acquire NAL pid ranges for lib_register_nal.
 */
static
int acquire_pid_ranges(lib_pids_inuse_t **pids, pid_ranges_t pid_limits)
{
	int rc = 0;
	unsigned i, wkp_longs;
	lib_pids_inuse_t *p;

	p = p3_malloc(sizeof(*p));
	if (!p) {
		rc = -ENOMEM;
		goto fail;
	}
	memset(p, 0, sizeof(*p));

	/* Available PID values for a given NAL type are shared across
	 * all instances of that NAL type.
	 */
	rc = pid_limits(&p->first_epid, &p->last_epid,
			&p->well_known_pids, &p->num_wkpids);
	if (rc)
		goto fail;

	if (p->first_epid >= p->last_epid) {
		rc = -EINVAL;
		goto fail;
	}
	p->next_epid = p->first_epid;

	/* Make sure we meet the well-known PID ordering requirement, that
	 * we don't use the reserved value PTL_PID_ANY, and that the
	 * ephemeral PIDs range doesn't overlap the well-known PID range.
	 */
	for (i=1; i<p->num_wkpids; i++)
		if (p->well_known_pids[i-1] >= 
		    p->well_known_pids[i]) {
			rc = -EINVAL;
			goto fail;
		}
	if (p->num_wkpids) {
		if (p->well_known_pids[p->num_wkpids-1] == PTL_PID_ANY)
			p->num_wkpids--;

		p->first_wkpid = &p->well_known_pids[0];
		p->last_wkpid = &p->well_known_pids[p->num_wkpids-1];

		if ((*p->first_wkpid >= p->first_epid && 
		     *p->first_wkpid <= p->last_epid) ||
		    (p->first_epid >= *p->first_wkpid && 
		     p->first_epid <= *p->last_wkpid)) {
			rc = -EINVAL;
			goto fail;
		}
		wkp_longs = (p->num_wkpids + BITS_IN_LONG - 1)/BITS_IN_LONG;
		p->wkpid = p3_malloc(wkp_longs * sizeof(long));
		if (!p->wkpid) {
			rc = -ENOMEM;
			goto fail;
		}
		memset(p->wkpid, 0, wkp_longs*sizeof(long));
	}
out:
	*pids = p;
	return rc;
fail:
	if (p) {
		if (p->well_known_pids)
			p3_free(p->well_known_pids);
		p3_free(p);
		p = NULL;
	}
	goto out;
}

int lib_register_nal(ptl_interface_t type, const char *name,
		     nal_create_t add, nal_stop_t stop, nal_destroy_t drop,
		     pid_ranges_t pid_limits)
{
	int rc;
	unsigned i;
	char *nm = p3_strdup(name);
	lib_pids_inuse_t *pids = NULL;

	if (!nm) {
		rc = -ENOMEM;
		goto err;
	}
	if ((rc = acquire_pid_ranges(&pids, pid_limits)))
		goto err;

	p3_lock(&lib_update);

	for (i=0; i<PTL_MAX_INTERFACES; i++) {
		if (!p3nals[i].type)
			break;
		if (p3nals[i].type == type) {
			rc = -EEXIST;
			goto err_unlock;
		}
	}
	if (i == PTL_MAX_INTERFACES) {
		p3_unlock(&lib_update);
		return -ENOMEM;
	}
	p3nals[i].name = nm;
	p3nals[i].type = type;
	p3nals[i].create = add;
	p3nals[i].stop = stop;
	p3nals[i].destroy = drop;
	p3nals[i].pids_inuse = pids;

	if (DEBUG_P3(p3lib_debug, PTL_DBG_SETUP))
		p3_print("lib_register_nal: NAL type %#x at index %d\n",
			 type, i);

	p3_unlock(&lib_update);

	return 0;

err_unlock:
	p3_unlock(&lib_update);
err:
	if (nm)
		p3_free(nm);
	if (pids) {
		if (pids->wkpid)
			p3_free(pids->wkpid);
		if (pids->well_known_pids)
			p3_free(pids->well_known_pids);
		p3_free(pids);
	}
	return rc;
}

int lib_unregister_nal(ptl_interface_t type)
{
	nal_type_t *nt;
	int rc = -1;

	p3_lock(&lib_update);

	if ((nt = __get_nal_type(type))) {

		if (nt->refcnt != 1) {	/* ours is only reference? */
			rc = -EBUSY;
			goto out;
		}
		nt->type = 0;
		nt->create = NULL;
		nt->stop = NULL;
		nt->destroy = NULL;

		p3_free(nt->pids_inuse->wkpid);
		p3_free(nt->pids_inuse->well_known_pids);
		p3_free(nt->pids_inuse);

		if (nt->name) {
			p3_free(nt->name);
			nt->name = NULL;
		}
		__put_nal_type(nt);
		if (nt->refcnt)
			PTL_ROAD();

		rc = 0;
	}
out:
	p3_unlock(&lib_update);
	return rc;
}

lib_nal_t *lib_new_nal(ptl_interface_t type, void *data, size_t data_sz,
		       const lib_ni_t *ni, ptl_nid_t *nid,
		       ptl_ni_limits_t *limits, int *status)
{
	lib_nal_t *nal;
	nal_type_t *nt = get_nal_type(type);

	if (!nt) {
		*status = PTL_IFACE_INVALID;
		return NULL;
	}
	if ((nal = nt->create(type, ni, nid, limits, data, data_sz))) {
		nal->nal_type = nt;
		*status = PTL_OK;
	}
	else {
		put_nal_type(nt);
		*status = PTL_NO_SPACE;
	}
	return nal;
}

void lib_stop_nal(lib_nal_t *nal)
{
	nal_type_t *nt = nal->nal_type;

	nt->stop(nal);
}

void lib_free_nal(lib_nal_t *nal)
{
	nal_type_t *nt = nal->nal_type;

	nt->destroy(nal);
	put_nal_type(nt);
}
