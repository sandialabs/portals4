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
 * libtest_init(void)
 * 
 * Returns:
 *  0  - success
 *  1  - failure
 *
 * Notes:
 *  Must be called before any other libtest function.
 */
int libtest_init(void);


/*
 * libtest_fini(void)
 * 
 * Returns:
 *  0  - success
 *  1  - failure
 *
 * Notes:
 *  No libtest function may be called after calling fini.
 */
int libtest_fini(void);


/*
 * libtest_get_mapping(void)
 * 
 * Returns:
 *  non-NULL - physical address for every rank in job
 *  NULL     - Error
 *
 * Notes:
 *  Mapping will be a static buffer; should not be freed by
 *  the caller.
 */
ptl_process_t* libtest_get_mapping(void);


/*
 * libtest_get_rank(void)
 * 
 * Returns:
 *  -1           - Error
 *  Non-negative - Rank in job
 *
 * Notes:
 */
int libtest_get_rank(void);


/*
 * libtest_get_size(void)
 * 
 * Returns:
 *  -1           - Error
 *  Non-negative - Job size
 *
 * Notes:
 */
int libtest_get_size(void);


/*
 * libtest_barrier(void)
 * 
 * Returns:
 *
 * Notes:
 */
void libtest_barrier(void);


/*
** Some convenience functions
*/
#define LIBTEST_CHECK(rc, fun)                                         \
    if (rc != PTL_OK && rc != PTL_IGNORED)   {							\
	fprintf(stderr, "%s() failed (%s)\n", fun, libtest_StrPtlError(rc)); \
	exit(1);                                                       \
    }

/* Use this libtest_Put() if no ACKs, user data, etc. */
#define libtest_Put(handle, size, dest, index)                          \
    PtlPut(handle, 0, size, PTL_NO_ACK_REQ, dest, index, 0, 0, NULL, 0)

/* Same as above, but with local and remote offsets */
#define libtest_Put_offset(handle, local_offset, size, dest, index, match, remote_offset) \
    PtlPut(handle, local_offset, size, PTL_NO_ACK_REQ, dest, index, match, remote_offset, NULL, 0)

/* Use these to setup MDs and LEs, with and without an attached counter */
ptl_handle_md_t libtest_CreateMD(ptl_handle_ni_t ni, 
                                 void *start, 
                                 ptl_size_t length);

void libtest_CreateMDCT(ptl_handle_ni_t ni, 
                        void *start,
                        ptl_size_t length,
                        ptl_handle_md_t *mh,
                        ptl_handle_ct_t *ch);

void libtest_CreateLECT(ptl_handle_ni_t ni, 
                        ptl_pt_index_t index,
                        void *start,
                        ptl_size_t length,
                        ptl_handle_le_t *lh,
                        ptl_handle_ct_t *ch);

int libtest_CreateMEPersistent(ptl_handle_ni_t ni,
                               ptl_pt_index_t index,
                               void *start,
                               ptl_size_t length,
                               ptl_size_t count, 
                               ptl_handle_me_t *mh);

int libtest_CreateMEUseOnce(ptl_handle_ni_t ni,
                            ptl_pt_index_t index,
                            void *start,
                            ptl_size_t length,
                            ptl_size_t count,
                            ptl_handle_me_t *mh);

void libtest_FreeME(ptl_size_t count,
                    ptl_handle_me_t *mh);

char * libtest_StrPtlError(int error_code);

/* Use this to get a Portal table entry */
ptl_pt_index_t libtest_PTAlloc(ptl_handle_ni_t ni,
                               ptl_pt_index_t request,
                               ptl_handle_eq_t eq);

void libtest_BarrierInit(ptl_handle_ni_t ni,
                         int rank,
                         int nproc);
void libtest_Barrier(void);

void libtest_AllreduceDouble_init(ptl_handle_ni_t ni);
double libtest_AllreduceDouble(double value, ptl_op_t op);

#endif /* _LIBP4SUPPORT_H */
