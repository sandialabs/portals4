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

/* These are prototypes for Portals library support functions that have
 * different implementatations depending on whether the library is 
 * in user/kernel/NIC space.
 */

#ifndef _PTL3_LIB_P3LIB_SUPPORT_H_
#define _PTL3_LIB_P3LIB_SUPPORT_H_

extern void p3lib_free_process(p3_process_t *p);

/* The library will call this to create a new Portals process struct for the
 * current system process.  The call is equivalent to p3lib_cur_process() if
 * a Portals process struct for the current system process already exists.
 *
 * Note that this function is only safe to call from "process" context in
 * the library; i.e., from the lib_* (dispatch table) functions.
 */
extern p3_process_t *p3lib_new_process(void);

/* The library will call this to retrieve a Portals process struct for the
 * current system process.  It returns NULL if the Portals process struct
 * for the current system process doesn't exist.
 *
 * Note that this function is only safe to call from "process" context in
 * the library; i.e., from the lib_* (dispatch table) functions.
 */
extern p3_process_t *p3lib_cur_process(void);

/* Same as above, but only safe to call when holding lib_update lock.
 */
extern p3_process_t *__p3lib_cur_process(void);

/* The library will call this to retrieve a Portals process struct for the
 * specified Portals process id.  It returns NULL if there is none.
 */
extern p3_process_t *p3lib_get_process(ptl_interface_t type, ptl_pid_t pid);
extern lib_ni_t *p3lib_get_ni_pid(ptl_interface_t type, ptl_pid_t pid);

/* Call this to register a new PID for a portals process; only safe to
 * call when holding lib_update lock.
 */
extern void __p3lib_process_add_pid(lib_ni_t *ni, ptl_pid_t pid);

/* Call this to release a PID used by a portals process; only safe to
 * call when holding lib_update lock.
 */
extern void __p3lib_process_rel_pid(lib_ni_t *ni);

/* The library will use this to get a new instance of a particular NAL type.
 * If a NAL type needs unique data to create an instance, *data and data_sz
 * can be used to supply it.  *nid  and *limits should be set to values
 * appropriate to the new instance. *status should be PTL_OK on success,
 * or give an appropriate error indication.
 */
extern lib_nal_t *lib_new_nal(ptl_interface_t type, void *data, size_t data_sz,
			      const lib_ni_t *ni, ptl_nid_t *nid,
			      ptl_ni_limits_t *limits, int *status);

/* The library will use this to cause a particular NAL type to stop
 * processing messages, prior to shutting down.  The library expects 
 * that when lib_stop_nal() returns, all references to library objects 
 * will have been released, but the NAL memory validation service is 
 * still operational.
 */
extern void lib_stop_nal(lib_nal_t *nal);

/* The library will use this to remove an existing instance of a particular 
 * NAL type.
 */
extern void lib_free_nal(lib_nal_t *nal);

/* The library will use this to do any setup or teardown for NALs.  It is
 * not an error to call these multiple times from the same context.
 */
extern void p3lib_nal_setup(void);
extern void p3lib_nal_teardown(void);


static inline
lib_ni_t *p3lib_get_ni_process(ptl_interface_t type, p3_process_t *pp)
{
	unsigned i;

	for (i=PTL_MAX_INTERFACES; i--; ) {
		lib_ni_t *ni = pp->ni[i];
		if (ni && ni->nal->nal_type->type == type)
			return ni;
	}
	return NULL;
}

/* validate/invalidate: The library uses these to allow the NAL to
 * prepare as necessary for data movement to/from an address range
 * in the API memory space.
 *
 * See p3validate.h for more details on what a NAL needs to provide and
 * do to make use of this support.
 *
 * WARNING: Once an addrkey has been passed to p3lib_invalidate() or
 * p3lib_vinvalidate(), it is no longer valid, and must not be used again.
 */
int p3lib_validate(lib_ni_t *ni,
		   api_mem_t *base, size_t extent, void **addrkey);

int p3lib_vvalidate(lib_ni_t *ni,
		    ptl_md_iovec_t *iov, size_t iovlen, void **addrkey);

void p3lib_invalidate(lib_ni_t *ni,
		      api_mem_t *base, size_t extent, void *addrkey);

void p3lib_vinvalidate(lib_ni_t *ni,
		       ptl_md_iovec_t *iov, size_t iovlen, void *addrkey);

/* Here's a companion copy_to_api function.
 */
int p3lib_copy_to_api(lib_ni_t *ni,
		      api_mem_t *dst, void *src, ptl_size_t len);


#endif /* _PTL3_LIB_P3LIB_SUPPORT_H_ */
