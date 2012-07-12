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

#include <limits.h>

/* This file is used to implement Portals library support functions that
 * are specific to a user-space library implementation.
 */

#include <sys/types.h>

/* If compiling for user-space (and maybe NIC-space), these need to be
 * the Portals3 versions of the Linux kernel include files, which
 * have been suitably modified for use in user-space code.
 */
#include <linux/list.h>

/* These are all Portals3 include files.
 */
#include "p3utils.h"

#include <p3api/types.h>

#include <p3/lock.h>
#include <p3/handle.h>
#include <p3/process.h>
//#include <p3/forward.h>
#include <p3/errno.h>
#include <p3/nal_types.h>

#include <p3lib/types.h>
#include <p3lib/p3lib.h>
#include <p3lib/nal.h>
#include <p3lib/p3lib_support.h>
#include <p3lib/p3validate.h>

//#include <p3/obj_alloc.h>	/* MUST come after p3lib/p3lib.h */

//#include <p3rt/p3rt.h>

#ifdef PTL_UTCP_NAL_SUPPORT
#include <lib-tcpnal.h>
#endif

#if 0
static
p3_process_t p3_lib_process = { .init = -1, .jid = PTL_JID_ANY, };

void p3lib_free_process(p3_process_t *pp)
{
	unsigned i;

	if (!pp || pp != &p3_lib_process)
		return;

	p3_lock(&lib_update);

	for (i=0; i<PTL_MAX_INTERFACES; i++) {
		if (p3_lib_process.ni[i])
			PTL_ROAD();
	}

#ifdef ENABLE_P3RT_SUPPORT
	p3_lib_process.next_group = NULL;
#endif
	p3_lib_process.jid = PTL_JID_ANY;
	p3_lib_process.init = -1;

	p3_unlock(&lib_update);
}

p3_process_t *p3lib_new_process(void)
{
	p3_lock(&lib_update);

	/* Since the library is in user space, there can be only one.
	 */
	if (p3_lib_process.init >= 0) {
		p3_unlock(&lib_update);
		return &p3_lib_process;
	}
#ifdef ENABLE_P3RT_SUPPORT
	p3_lib_process.next_group = NULL;
#endif
	p3_unlock(&lib_update);

	return &p3_lib_process;
}

/* Only safe to call when holding lib_update lock.
 */
p3_process_t *__p3lib_cur_process(void)
{
	return p3_lib_process.init < 0 ? NULL : &p3_lib_process;
}

p3_process_t *p3lib_cur_process(void)
{
	p3_process_t *pp;

	p3_lock(&lib_update);
	pp = __p3lib_cur_process();
	p3_unlock(&lib_update);

	return pp;
}

p3_process_t *p3lib_get_process(ptl_interface_t type, ptl_pid_t pid)
{
	p3_process_t *pp = NULL;
	unsigned i;

	p3_lock(&lib_update);
	
	if (p3_lib_process.init >= 0)
		for (i=PTL_MAX_INTERFACES; i--; ) {
			lib_ni_t *ni = p3_lib_process.ni[i];
			if (ni &&
			    ni->nal->nal_type->type == type &&
			    ni->pid == pid) {
				pp = &p3_lib_process;
				break;
			}
		}

	p3_unlock(&lib_update);
	return pp;
}

lib_ni_t *p3lib_get_ni_pid(ptl_interface_t type, ptl_pid_t pid)
{
	lib_ni_t *ni;
	unsigned i;

	p3_lock(&lib_update);
	
	if (p3_lib_process.init >= 0) {
		for (i=PTL_MAX_INTERFACES; i--; ) {
			ni = p3_lib_process.ni[i];
    
			if (ni &&
			    ni->nal->nal_type->type == type &&
			    ni->pid == pid)
				goto out;
		}
	}
	ni = NULL;
out:
	p3_unlock(&lib_update);
	return ni;
}

/* We don't need any extra registration of PIDs with a portals process,
 * beyond the PID stored in an NI instance
 */
void __p3lib_process_add_pid(lib_ni_t *ni, ptl_pid_t pid)
{
}
void __p3lib_process_rel_pid(lib_ni_t *ni)
{
}
#endif

void p3lib_nal_setup(void)
{
#ifdef PTL_UTCP_NAL_SUPPORT
	lib_register_nal(PTL_NALTYPE_UTCP, "UTCP",
			 p3tcp_create_nal, p3tcp_stop_nal, p3tcp_destroy_nal,
			 p3tcp_pid_ranges);
	lib_register_nal(PTL_NALTYPE_UTCP1, "UTCP1",
			 p3tcp_create_nal, p3tcp_stop_nal, p3tcp_destroy_nal,
			 p3tcp_pid_ranges);
	lib_register_nal(PTL_NALTYPE_UTCP2, "UTCP2",
			 p3tcp_create_nal, p3tcp_stop_nal, p3tcp_destroy_nal,
			 p3tcp_pid_ranges);
	lib_register_nal(PTL_NALTYPE_UTCP3, "UTCP3",
			 p3tcp_create_nal, p3tcp_stop_nal, p3tcp_destroy_nal,
			 p3tcp_pid_ranges);
#endif
}

void p3lib_nal_teardown(void)
{
#ifdef PTL_UTCP_NAL_SUPPORT
	lib_unregister_nal(PTL_NALTYPE_UTCP);
	lib_unregister_nal(PTL_NALTYPE_UTCP1);
	lib_unregister_nal(PTL_NALTYPE_UTCP2);
	lib_unregister_nal(PTL_NALTYPE_UTCP3);
#endif
}
