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

#ifndef _PTL3_API_DEBUG_H_
#define _PTL3_API_DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* PtlNIDebug:
 *
 * These are not official Portals 3 API calls.  They are provided
 * by the reference implementation to allow the maintainers an
 * easy way to debugg information in the library.  Do not use them
 * in code that is not intended for use with any version other than
 * the portable reference library.
 *
 * PtlNIDebug() sets the network interface debug value to <mask>, and
 * returns the previous value.  It can be called before PtlNIInit if
 * called with an NI handle value of PTL_INVALID_HANDLE; in this case
 * the debug value will be available during NAL startup, and will
 * become the default debug value for all NI instances.
 */
//unsigned int PtlNIDebug(ptl_handle_ni_t ni, unsigned int mask);

//int PtlTblDump(ptl_handle_ni_t ni, int pt_index);
//int PtlMEDump(ptl_handle_me_t me_handle);

#ifdef __cplusplus
}
#endif

/* Debugging flags reserved for the Portals reference library.
 * These are not part of the official Portals 3 API, but are for
 * the use of the maintainers of the reference implementation.
 *
 * It is not expected that the real implementations will export
 * this functionality.
 */
#define PTL_DBG_NONE		0U
#define PTL_DBG_ALL		(0x0000ffff)	/* Only the Portals flags */

#define __bit(x)		(1U<<(x))
#define PTL_DBG_API		__bit(0)
#define PTL_DBG_PARSE		__bit(1)
#define PTL_DBG_MOVE		__bit(2)
#define PTL_DBG_DROP		__bit(3)
#define PTL_DBG_REQUEST		__bit(4)
#define PTL_DBG_DELIVERY	__bit(5)
#define PTL_DBG_MD		__bit(6)
#define PTL_DBG_ME		__bit(7)
#define PTL_DBG_UNLINK		__bit(8)
#define PTL_DBG_EQ		__bit(9)
#define PTL_DBG_EVENT		__bit(10)
#define PTL_DBG_MEMORY		__bit(11)
#define PTL_DBG_SETUP		__bit(12)
#define PTL_DBG_SHUTDOWN	__bit(13)

/* These eight are reserved for the NAL to define
 * It should probably give them better names...
 */
#define PTL_DBG_NI_ALL		(0xffff0000)	/* Only the NAL flags */
#define PTL_DBG_NI_00		__bit(16)
#define PTL_DBG_NI_01		__bit(17)
#define PTL_DBG_NI_02		__bit(18)
#define PTL_DBG_NI_03		__bit(19)
#define PTL_DBG_NI_04		__bit(20)
#define PTL_DBG_NI_05		__bit(21)
#define PTL_DBG_NI_06		__bit(22)
#define PTL_DBG_NI_07		__bit(23)
#define PTL_DBG_NI_08		__bit(24)
#define PTL_DBG_NI_09		__bit(25)
#define PTL_DBG_NI_10		__bit(26)
#define PTL_DBG_NI_11		__bit(27)
#define PTL_DBG_NI_12		__bit(28)
#define PTL_DBG_NI_13		__bit(29)
#define PTL_DBG_NI_14		__bit(30)
#define PTL_DBG_NI_15		__bit(31)

/* A application that needs to change where the user-space debug output
 * goes can set this to a file pointer.  This will also affect the library
 * and NAL debug output if they are compiled for user-space.
 */
#ifndef PTL_KERNEL_BLD
#include <stdio.h>
extern FILE *p3_out;
#endif

#endif /* _PTL3_API_DEBUG_H_ */
