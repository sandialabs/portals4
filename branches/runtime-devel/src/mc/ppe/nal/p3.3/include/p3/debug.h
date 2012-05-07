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

#ifndef _PTL3_P3_DEBUG_H_
#define _PTL3_P3_DEBUG_H_
  
#ifdef DEBUG_PTL_INTERNALS
#define DEBUG_P3(flags, feature)  ((flags) & (feature))
#else
#define DEBUG_P3(flags, feature)  0
#endif

/* Define some fixed-size formats to use when printing fixed-size data.
 * These make use of <inttypes.h>, so if we're doing this in kernel
 * space we'll need duplicate functionality from somewhere.
 */
#include <inttypes.h>

#if defined SIZE_T_IS_UINT
#define FMT_SZ_T      "%u"
#define FMT_SSZ_T     "%d"
#elif defined SIZE_T_IS_ULONG
#define FMT_SZ_T      "%lu"
#define FMT_SSZ_T     "%ld"
#endif

#define FMT_PSZ_T     "%"PRIu64
#define FMT_SEQ_T     "%"PRIu32
#define FMT_NID_T     "%"PRIu32
#define FMT_PID_T     "%"PRIu32
#define FMT_UID_T     "%"PRIu32
#define FMT_JID_T     "%"PRIu32
#define FMT_PTL_T     "%"PRIu32
#define FMT_HDL_T     "%"PRIx32

#define FMT_NIDPID    " nid "FMT_NID_T" pid "FMT_PID_T
#define FMT_NIDPIDPTL " nid "FMT_NID_T" pid "FMT_PID_T" ptl "FMT_PTL_T
#define FMT_MBITS     " mb %"PRIx64
#define FMT_IBITS     " ib %"PRIx64
#define FMT_RLEN      " rlen "FMT_PSZ_T
#define FMT_MLEN      " mlen "FMT_PSZ_T
#define FMT_LEN       " len "FMT_PSZ_T

#endif /* _PTL3_P3_DEBUG_H_ */
