/* -*- C -*-
 *
 * Copyright 2006 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
 * retains certain rights in this software.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301, USA.
 */

#ifndef _LIBP4SUPPORT_H
#define _LIBP4SUPPORT_H


/*
** Some convenience functions
*/
#define PTL_CHECK(rc, fun) \
    if (rc != PTL_OK)   { \
	fprintf(stderr, "%s() failed (%s)\n", fun, __StrPtlError(rc)); \
	exit(1); \
    }

/* Use this __PtlPut() if no ACKs, user data, etc. */
#define __PtlPut(handle, size, dest, index) \
	PtlPut(handle, 0, size, PTL_NO_ACK_REQ, dest, index, 0, 0, NULL, 0)

/* Same as above, but with local and remote offsets */
#define __PtlPut_offset(handle, local_offset, size, dest, index, match, remote_offset) \
	PtlPut(handle, local_offset, size, PTL_NO_ACK_REQ, dest, index, match, remote_offset, NULL, 0)


/* Use these to setup MDs and LEs, with and without an attached counter */
ptl_handle_md_t __PtlCreateMD(ptl_handle_ni_t ni, void *start, ptl_size_t length);
void __PtlCreateMDCT(ptl_handle_ni_t ni, void *start, ptl_size_t length, ptl_handle_md_t *mh,
	ptl_handle_ct_t *ch);
void __PtlCreateLECT(ptl_handle_ni_t ni, ptl_pt_index_t index, void *start, ptl_size_t length,
	ptl_handle_le_t *lh, ptl_handle_ct_t *ch);

void __PtlCreateMEPersistent(ptl_handle_ni_t ni, ptl_pt_index_t index, void *start, ptl_size_t length,
	ptl_size_t count, ptl_handle_me_t *mh);
int __PtlCreateMEUseOnce(ptl_handle_ni_t ni, ptl_pt_index_t index, void *start, ptl_size_t length,
	ptl_size_t count, ptl_handle_me_t *mh);
void __PtlFreeME(ptl_size_t count, ptl_handle_me_t *mh);

char *__StrPtlError(int error_code);

/* Use this to get a Portal table entry */
ptl_pt_index_t __PtlPTAlloc(ptl_handle_ni_t ni, ptl_pt_index_t request, ptl_handle_eq_t eq);

void __PtlBarrierInit(ptl_handle_ni_t ni, int rank, int nproc);
void __PtlBarrier(void);

void __PtlAllreduceDouble_init(ptl_handle_ni_t ni);
double __PtlAllreduceDouble(double value, ptl_op_t op);

#endif /* _LIBP4SUPPORT_H */
