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

#ifndef _PTL3_P3_NAL_TYPES_H_
#define _PTL3_P3_NAL_TYPES_H_

#include <stdint.h>

/* Since the library handles forwarding from API space to library space
 * now, we need some way for the API-side NAL code to tell the library-side
 * code what NAL is being requested.
 *
 * Hence, we need this file to assign NAL type identifiers to all the
 * NAL types this implementation supports.
 *
 * Furthermore, since the spec doesn't admit a simple method for allowing
 * a given NAL type to handle more than one interface, we'll need to have
 * multiple type identifiers. E.g., PTL_NALTYPE_TCP0 might use eth0,
 * while PTL_NALTYPE_TCP1 might use myri0.  See the applicable NAL READMEs
 * for details.
 */

#define PTL_NALTYPE_NONE UINT32_C(0)

/* User-space TCP NAL
 */
#define PTL_NALTYPE_UTCP  PTL_NALTYPE_UTCP0

#define PTL_NALTYPE_UTCP0 UINT32_C(0xf0f0f000)
#define PTL_NALTYPE_UTCP1 UINT32_C(0xf0f0f001)
#define PTL_NALTYPE_UTCP2 UINT32_C(0xf0f0f002)
#define PTL_NALTYPE_UTCP3 UINT32_C(0xf0f0f003)

/* User-space UDP NAL
 */
#define PTL_NALTYPE_UUDP  PTL_NALTYPE_UUDP0

#define PTL_NALTYPE_UUDP0 UINT32_C(0xf0f0f010)
#define PTL_NALTYPE_UUDP1 UINT32_C(0xf0f0f011)
#define PTL_NALTYPE_UUDP2 UINT32_C(0xf0f0f012)
#define PTL_NALTYPE_UUDP3 UINT32_C(0xf0f0f013)

/* Kernel-space TCP
 */
#define PTL_NALTYPE_TCP   PTL_NALTYPE_TCP0

#define PTL_NALTYPE_TCP0  UINT32_C(0xf0f0f020)
#define PTL_NALTYPE_TCP1  UINT32_C(0xf0f0f021)
#define PTL_NALTYPE_TCP2  UINT32_C(0xf0f0f022)
#define PTL_NALTYPE_TCP3  UINT32_C(0xf0f0f023)

/* Kernel-space UDP
 */
#define PTL_NALTYPE_UDP   PTL_NALTYPE_UDP0

#define PTL_NALTYPE_UDP0  UINT32_C(0xf0f0f030)
#define PTL_NALTYPE_UDP1  UINT32_C(0xf0f0f031)
#define PTL_NALTYPE_UDP2  UINT32_C(0xf0f0f032)
#define PTL_NALTYPE_UDP3  UINT32_C(0xf0f0f033)

#endif /* _PTL3_P3_NAL_TYPES_H_ */
