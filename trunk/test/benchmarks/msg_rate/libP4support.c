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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <portals4.h>
#include <portals4_runtime.h>
#include "libP4support.h"



/* Return Portals error return codes as strings */
char *
__StrPtlError(int error_code)
{
    switch (error_code)   {
	case PTL_OK:
	    return "PTL_OK";
	case PTL_ARG_INVALID:
	    return "PTL_ARG_INVALID";
	case PTL_CT_NONE_REACHED:
	    return "PTL_CT_NONE_REACHED";
	case PTL_EQ_DROPPED:
	    return "PTL_EQ_DROPPED";
	case PTL_EQ_EMPTY:
	    return "PTL_EQ_EMPTY";
	case PTL_FAIL:
	    return "PTL_FAIL";
	case PTL_IN_USE:
	    return "PTL_IN_USE";
	case PTL_INTERRUPTED:
	    return "PTL_INTERRUPTED";
	case PTL_LIST_TOO_LONG:
	    return "PTL_LIST_TOO_LONG";
	case PTL_NI_NOT_LOGICAL:
	    return "PTL_NI_NOT_LOGICAL";
	case PTL_NO_INIT:
	    return "PTL_NO_INIT";
	case PTL_NO_SPACE:
	    return "PTL_NO_SPACE";
	case PTL_PID_IN_USE:
	    return "PTL_PID_IN_USE";
	case PTL_PT_FULL:
	    return "PTL_PT_FULL";
	case PTL_PT_EQ_NEEDED:
	    return "PTL_PT_EQ_NEEDED";
	case PTL_PT_IN_USE:
	    return "PTL_PT_IN_USE";
	case PTL_SIZE_INVALID:
	    return "PTL_SIZE_INVALID";
	default:
	    return "Unknown Portals return code";
    }

}  /* end of strPTLerror() */



/*
** Create an MD
*/
ptl_handle_md_t
__PtlCreateMD(ptl_handle_ni_t ni, void *start, ptl_size_t length)
{

int rc;
ptl_md_t md;
ptl_handle_md_t md_handle;


    /* Setup the MD */
    md.start= start;
    md.length= length;
    md.options= PTL_MD_UNORDERED | PTL_MD_REMOTE_FAILURE_DISABLE | PTL_MD_EVENT_DISABLE;
    md.eq_handle= PTL_EQ_NONE;
    md.ct_handle= PTL_CT_NONE;

    rc= PtlMDBind(ni, &md, &md_handle);
    PTL_CHECK(rc, "Error in __PtlCreateMD(): PtlMDBind");

    return md_handle;

}  /* end of __PtlCreateMD() */



/*
** Create an MD with a event counter attached to it
*/
void
__PtlCreateMDCT(ptl_handle_ni_t ni, void *start, ptl_size_t length, ptl_handle_md_t *mh, ptl_handle_ct_t *ch)
{

int rc;
ptl_md_t md;


    /*
    ** Create a counter
    ** If a user wants to resue a CT handle, it will not be PTL_INVALID_HANDLE
    */
    if (*ch == PTL_INVALID_HANDLE)   {
	rc= PtlCTAlloc(ni, ch);
	PTL_CHECK(rc, "Error in __PtlCreateMDCT(): PtlCTAlloc");
    }

    /* Setup the MD */
    md.start= start;
    md.length= length;
    md.options= PTL_MD_EVENT_CT_SEND | PTL_MD_UNORDERED | PTL_MD_REMOTE_FAILURE_DISABLE |
		    PTL_MD_EVENT_DISABLE;
    md.eq_handle= PTL_EQ_NONE;
    md.ct_handle= *ch;

    rc= PtlMDBind(ni, &md, mh);
    PTL_CHECK(rc, "Error in __PtlCreateMDCT(): PtlMDBind");

}  /* end of __PtlCreateMDCT() */



/*
** Create a (persistent) LE with a counter attached to it
** Right now used for puts only...
*/
void
__PtlCreateLECT(ptl_handle_ni_t ni, ptl_pt_index_t index, void *start, ptl_size_t length,
	ptl_handle_le_t *lh, ptl_handle_ct_t *ch)
{

int rc;
ptl_le_t le;


    /* If a user wants to resue a CT handle, it will not be PTL_INVALID_HANDLE */
    if (*ch == PTL_INVALID_HANDLE)   {
	rc= PtlCTAlloc(ni, ch);
	PTL_CHECK(rc, "Error in __PtlCreateLECT(): PtlCTAlloc");
    }

    le.start= start;
    le.length= length;
    le.ac_id.uid= PTL_UID_ANY;
    le.options= PTL_LE_OP_PUT | PTL_LE_ACK_DISABLE | PTL_LE_EVENT_CT_COMM;
    le.ct_handle= *ch;
    rc= PtlLEAppend(ni, index, &le, PTL_PRIORITY_LIST, NULL, lh);
    PTL_CHECK(rc, "Error in __PtlCreateLECT(): PtlLEAppend");

}  /* end of __PtlCreateLECT () */



/*
** Create a Portal table entry. Use PTL_PT_ANY if you don't need
** a specific table entry.
*/
ptl_pt_index_t
__PtlPTAlloc(ptl_handle_ni_t ni, ptl_pt_index_t request)
{

int rc;
ptl_pt_index_t index;


    rc= PtlPTAlloc(ni, 0, PTL_EQ_NONE, request, &index);
    PTL_CHECK(rc, "Error in __PtlPTAlloc(): PtlPTAlloc");
    if ((index != request) && (request != PTL_PT_ANY))   {
	fprintf(stderr, "Did not get the Ptl index I requested!\n");
	exit(1);
    }

    return index;

}  /* end of __PtlPTAlloc() */



/*
** Simple barrier
** Arrange processes around a ring. Send a message to the right indicating that
** this process has entered the barrier. Wait for a message from the left, indicating
** that the left neighbor has also entered. Then, send another message to the right
** indicating that this process is ready to leave the barrier, and then wait for the
** left neighbor to send the same message.
**
** __PtlBarrier_init() sets up the send-side MD, and the receive side LE and counter.
** __PtlBarrier() performs the above simple barrier on a ring.
*/
#define __PtlBarrierIndex	(14)

static int __my_rank= -1;
static int __nproc= 1;
static ptl_handle_md_t __md_handle_barrier;
static ptl_handle_ct_t __ct_handle_barrier;
static ptl_size_t __barrier_cnt;


void
__PtlBarrierInit(ptl_handle_ni_t ni, int rank, int nproc)
{

ptl_pt_index_t index;
ptl_handle_le_t le_handle;


    __my_rank= rank;
    __nproc= nproc;
    __barrier_cnt= 1;
    __ct_handle_barrier= PTL_INVALID_HANDLE;

    /* Create the send side MD */
    __md_handle_barrier= __PtlCreateMD(ni, NULL, 0);

    /* We want a specific Portals table entry */
    index= __PtlPTAlloc(ni, __PtlBarrierIndex);

    /* Create a counter and attach an LE to the Portal table */
    __PtlCreateLECT(ni, index, NULL, 0, &le_handle, &__ct_handle_barrier);

}  /* end of __PtlBarrier_init() */



void
__PtlBarrier(void)
{

int rc;
ptl_process_t neighbor;
ptl_size_t test;
ptl_ct_event_t cnt_value;


    /* Tell my right neighbor that I have entered the barrier */
    neighbor.rank= (__my_rank + 1) % __nproc;
    rc= __PtlPut(__md_handle_barrier, 0, neighbor, __PtlBarrierIndex);
    PTL_CHECK(rc, "1st PtlPut in __PtlBarrier");

    /* Wait for my left neighbor to enter the barrier */
    test= __barrier_cnt++;
    rc= PtlCTWait(__ct_handle_barrier, test, &cnt_value);
    PTL_CHECK(rc, "1st PtlCTWait in __PtlBarrier");

    /* Tell my right neighbor that my left neighbor has entered.  */
    rc= __PtlPut(__md_handle_barrier, 0, neighbor, __PtlBarrierIndex);
    PTL_CHECK(rc, "2nd PtlPut in __PtlBarrier");

    /* Wait until my left neighbor is leaving barrier */
    test= __barrier_cnt++;
    rc= PtlCTWait(__ct_handle_barrier, test, &cnt_value);
    PTL_CHECK(rc, "2nd PtlCTWait in __PtlBarrier");

}  /* end of __PtlBarrier() */



/*
** Allreduce on a ring
*/
int __PtlAllreduceIndex[2]= {15, 16};
static ptl_handle_md_t __md_handle_allreduce[2];
static ptl_handle_md_t __md_handle_allreduce_local;
static double __allreduce_value[2];
static double __allreduce_value_local;
static ptl_handle_ct_t __ct_handle_allreduce;
static ptl_size_t __allreduce_cnt;

void
__PtlAllreduceDouble_init(ptl_handle_ni_t ni)
{

ptl_pt_index_t index[2];
ptl_handle_le_t le_handle;


    __allreduce_value[0]= 98.0;
    __allreduce_value[1]= 99.0;
    __allreduce_value_local= 55.0;
    __allreduce_cnt= 1;
    __ct_handle_allreduce= PTL_INVALID_HANDLE;

    /* We use two specific Portals table entry and alternate between them */
    index[0]= __PtlPTAlloc(ni, __PtlAllreduceIndex[0]);
    index[1]= __PtlPTAlloc(ni, __PtlAllreduceIndex[1]);

    /* Create a counter and attach an LE to each of the two Portal table entries */
    __PtlCreateLECT(ni, index[0], &__allreduce_value[0], sizeof(double), &le_handle, &__ct_handle_allreduce);
    __PtlCreateLECT(ni, index[1], &__allreduce_value[1], sizeof(double), &le_handle, &__ct_handle_allreduce);

    /* Create two send-side MDs */
    __md_handle_allreduce[0]= __PtlCreateMD(ni, &__allreduce_value[0], sizeof(double));
    __md_handle_allreduce[1]= __PtlCreateMD(ni, &__allreduce_value[1], sizeof(double));

    /* Create another MD to send my value */
    __md_handle_allreduce_local= __PtlCreateMD(ni, &__allreduce_value_local, sizeof(double));

}  /* end of __PtlAllreduceDouble_init() */



double
__PtlAllreduceDouble(double value, ptl_op_t op)
{

int rc;
double ret;
ptl_process_t neighbor;
ptl_size_t test;
ptl_ct_event_t cnt_value;
static int even= 0;


    /* Send my value to my right neighbor if I am rank 0 to get things started */
    neighbor.rank= (__my_rank + 1) % __nproc;
    if (__my_rank == 0)   {

	__allreduce_value_local= value;
	rc= __PtlPut(__md_handle_allreduce_local, sizeof(double), neighbor, __PtlAllreduceIndex[even]);
	PTL_CHECK(rc, "1st PtlPut in __PtlAllreduceDouble");

    } else   {

	/* Wait for the value from my left neighbor */
	test= __allreduce_cnt++;
	rc= PtlCTWait(__ct_handle_allreduce, test, &cnt_value);
	PTL_CHECK(rc, "1st PtlCTWait in __PtlAllreduceDouble");

	/* Add my value */
	__allreduce_value[even] += value;

	/* Send result to right neighbor */
	rc= __PtlPut(__md_handle_allreduce[even], sizeof(double), neighbor, __PtlAllreduceIndex[even]);
	PTL_CHECK(rc, "2nd PtlPut in __PtlAllreduceDouble");
    }

    /* Wait for total */
    test= __allreduce_cnt++;
    rc= PtlCTWait(__ct_handle_allreduce, test, &cnt_value);
    PTL_CHECK(rc, "2nd PtlCTWait in __PtlAllreduceDouble");

    /* Pass it on, until we reach rank 0 again */
    if (__my_rank != (__nproc - 1))   {
	rc= __PtlPut(__md_handle_allreduce[even], sizeof(double), neighbor, __PtlAllreduceIndex[even]);
	PTL_CHECK(rc, "3rd PtlPut in __PtlAllreduceDouble");
    }

    ret= __allreduce_value[even];
    even= (even + 1) % 2;
    return ret;

}  /* end of __PtlAllreduceDouble() */
