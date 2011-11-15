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

/* Include this file to make the UTCP NAL available to an application.
 * An application can access multiple NALs by including more than one
 * p3nal_<nal>.h file.  In that case, the first file included defines
 * the default NAL, and any other NAL must be explicitly referenced.
 */

#ifndef _PTL3_P3NAL_UTCP_H_
#define _PTL3_P3NAL_UTCP_H_

#define PTL_IFACE_UTCP  PTL_NALTYPE_UTCP

#define PTL_IFACE_UTCP0 PTL_NALTYPE_UTCP0
#define PTL_IFACE_UTCP1 PTL_NALTYPE_UTCP1
#define PTL_IFACE_UTCP2 PTL_NALTYPE_UTCP2
#define PTL_IFACE_UTCP3 PTL_NALTYPE_UTCP3

#if 0
#ifdef PTL_IFACE_DEFAULT
#warn  Default Portals3 interface already defined
#else
#define PTL_IFACE_DEFAULT PTL_IFACE_UTCP
#endif
#endif


#endif /* _PTL3_P3NAL_UTCP_H_ */
