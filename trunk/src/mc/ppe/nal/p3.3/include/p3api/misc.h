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

/*
 * This file defines miscellaneous implementation-specific types and
 * function prototypes that an application needs to know about to use
 * this implementation of Portals 3.
 */

#ifndef _PTL3_API_MISC_H_
#define _PTL3_API_MISC_H_

/* Use these to get the status register for the maximum and current
 * number of match entries in a particular match list.
 */
#define PTL_SR_MLIST_LEN(ptl_index) \
	(ptl_index << 16 | PTL_SR_MES_CUR | 1U << 31)

#define PTL_SR_MLIST_MAX(ptl_index) \
	(ptl_index << 16 | PTL_SR_MESMAX | 1U << 31)

/* These are the values used by this implementation for
 * ptl_event_t:ni_fail_type:
 */
#define PTL_NI_FAIL 1

#ifdef __cplusplus
extern "C" {
#endif

/* These functions to return an error string for a Portals return code
 * aren't part of the spec, but they should be.
 */
const char *PtlErrorStr(unsigned ptl_errno);
//const char *PtlNIFailStr(ptl_handle_ni_t ni, ptl_ni_fail_t nal_errno);

/* This function to return an event type string for a Portals event
 * type isn't part of the spec, but it should be.
 */
//const char *PtlEventKindStr(ptl_event_kind_t ev_kind);

/* This function isn't part of the spec, but it should be.  An implementation
 * that has no progress thread anywhere (i.e. there is no NIC, kernel, or
 * user-space thread that can independently make progress) needs this
 * function to force progress to be made on outstanding messages.
 *
 * Anyway, we use it internally, but a library user needs to know it is
 * here to access it.  Also, a library user would only need it if not
 * using events at all, with a NAL that had no progress thread.
 */
//int PtlProgress(ptl_handle_any_t handle);

#ifdef __cplusplus
}
#endif

#endif /* _PTL3_API_MISC_H_ */
