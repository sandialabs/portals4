/* -*- C -*-
 *
 * Copyright 2010 Sandia Corporation. Under the terms of Contract
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

#include "support.h"

/* Return Portals error return codes as strings */
char *libtest_StrPtlError(int error_code)
{                                      /*{{{ */
    switch (error_code) {
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

        case PTL_IGNORED:
            return "PTL_IGNORED";

        case PTL_INTERRUPTED:
            return "PTL_INTERRUPTED";

        case PTL_LIST_TOO_LONG:
            return "PTL_LIST_TOO_LONG";

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

        default:
            return "Unknown Portals return code";
    }
}                                      /* end of strPTLerror() *//*}}} */

/*
** Create an MD
*/
ptl_handle_md_t libtest_CreateMD(ptl_handle_ni_t ni,
                              void           *start,
                              ptl_size_t      length)
{                                      /*{{{ */
    int             rc;
    ptl_md_t        md;
    ptl_handle_md_t md_handle;

    /* Setup the MD */
    md.start     = start;
    md.length    = length;
    md.options   = PTL_MD_UNORDERED;
    md.eq_handle = PTL_EQ_NONE;
    md.ct_handle = PTL_CT_NONE;

    rc = PtlMDBind(ni, &md, &md_handle);
    LIBTEST_CHECK(rc, "Error in libtest_CreateMD(): PtlMDBind");

    return md_handle;
}                                      /* end of libtest_CreateMD() *//*}}} */

/*
** Create an MD with a event counter attached to it
*/
void libtest_CreateMDCT(ptl_handle_ni_t  ni,
                     void            *start,
                     ptl_size_t       length,
                     ptl_handle_md_t *mh,
                     ptl_handle_ct_t *ch)
{                                      /*{{{ */
    int      rc;
    ptl_md_t md;

    /*
    ** Create a counter
    ** If a user wants to resue a CT handle, it will not be PTL_INVALID_HANDLE
    */
    if (*ch == PTL_INVALID_HANDLE) {
        rc = PtlCTAlloc(ni, ch);
        LIBTEST_CHECK(rc, "Error in libtest_CreateMDCT(): PtlCTAlloc");
    }

    /* Setup the MD */
    md.start     = start;
    md.length    = length;
    md.options   = PTL_MD_EVENT_CT_SEND | PTL_MD_UNORDERED;
    md.eq_handle = PTL_EQ_NONE;
    md.ct_handle = *ch;

    rc = PtlMDBind(ni, &md, mh);
    LIBTEST_CHECK(rc, "Error in libtest_CreateMDCT(): PtlMDBind");
}                                      /* end of libtest_CreateMDCT() *//*}}} */

/*
** Create a (persistent) LE with a counter attached to it
** Right now used for puts only...
*/
void libtest_CreateLECT(ptl_handle_ni_t  ni,
                     ptl_pt_index_t   index,
                     void            *start,
                     ptl_size_t       length,
                     ptl_handle_le_t *lh,
                     ptl_handle_ct_t *ch)
{                                      /*{{{ */
    int      rc;
    ptl_le_t le;

    /* If a user wants to reuse a CT handle, it will not be PTL_INVALID_HANDLE */
    if (*ch == PTL_INVALID_HANDLE) {
        rc = PtlCTAlloc(ni, ch);
        LIBTEST_CHECK(rc, "Error in libtest_CreateLECT(): PtlCTAlloc");
    }

    le.start     = start;
    le.length    = length;
    le.uid       = PTL_UID_ANY;
    le.options   = PTL_LE_OP_PUT | PTL_LE_ACK_DISABLE | PTL_LE_EVENT_CT_COMM | PTL_ME_EVENT_LINK_DISABLE;
    le.ct_handle = *ch;
    rc           = PtlLEAppend(ni, index, &le, PTL_PRIORITY_LIST, NULL, lh);
    LIBTEST_CHECK(rc, "Error in libtest_CreateLECT(): PtlLEAppend");
}                                      /* end of libtest_CreateLECT () *//*}}} */

/*
** Create "count" MEs with an event queue attached to it
** Each ME points to a "length" size chunk in a buffer starting at "start".
** Right now used for puts only...
*/
static int libtest_CreateME(ptl_handle_ni_t  ni,
                         ptl_pt_index_t   index,
                         void            *start,
                         ptl_size_t       length,
                         ptl_size_t       count,
                         unsigned int     options,
                         ptl_handle_me_t *mh)
{                                      /*{{{ */
    int           rc = 0;
    int           i;
    ptl_me_t      me;
    ptl_process_t src;

    src.rank = PTL_RANK_ANY;
    for (i = 0; i < count; i++) {
        me.start       = (char *)start + i * length;
        me.length      = length;
        me.ct_handle   = PTL_CT_NONE;
        me.min_free    = 0;
        me.uid         = PTL_UID_ANY;
        me.options     = options;
        me.match_id    = src;
        me.match_bits  = i;
        me.ignore_bits = 0;

        rc = PtlMEAppend(ni, index, &me, PTL_PRIORITY_LIST, NULL, &mh[i]);
        LIBTEST_CHECK(rc, "Error in libtest_CreateME(): PtlMEAppend");
    }
    return rc;
}                                      /* end of libtest_CreateME() *//*}}} */

/*
** Create "count" (persistent) MEs with an event queue attached to it
** Each ME points to a "length" size chunk in a buffer starting at "start".
** Right now used for puts only...
*/
int libtest_CreateMEPersistent(ptl_handle_ni_t  ni,
                            ptl_pt_index_t   index,
                            void            *start,
                            ptl_size_t       length,
                            ptl_size_t       count,
                            ptl_handle_me_t *mh)
{   /*{{{*/
    unsigned int options = PTL_ME_OP_PUT | PTL_ME_ACK_DISABLE;

    return libtest_CreateME(ni, index, start, length, count, options, mh);
}                                /* end of libtest_CreateMEPersistent() *//*}}}*/

/*
** Create "count" UseOnce MEs with an event queue attached to it
** Each ME points to a "length" size chunk in a buffer starting at "start".
** Right now used for puts only...
*/
int libtest_CreateMEUseOnce(ptl_handle_ni_t  ni,
                         ptl_pt_index_t   index,
                         void            *start,
                         ptl_size_t       length,
                         ptl_size_t       count,
                         ptl_handle_me_t *mh)
{
    unsigned int options = PTL_ME_OP_PUT | PTL_ME_ACK_DISABLE |
                           PTL_ME_USE_ONCE | PTL_ME_EVENT_UNLINK_DISABLE |
                           PTL_ME_EVENT_LINK_DISABLE;

    return libtest_CreateME(ni, index, start, length, count, options, mh);
}                                /* end of libtest_CreateMEUseONe() *//*}}} */

/*
** Create "count" (persistent) MEs with an event queue attached to it
** Each ME points to a "length" size chunk in a buffer starting at "start".
** Right now used for puts only...
*/
void libtest_FreeME(ptl_size_t       count,
                 ptl_handle_me_t *mh)
{                                      /*{{{ */
    int rc;
    int i;

    for (i = 0; i < count; i++) {
        rc = PtlMEUnlink(mh[i]);
        LIBTEST_CHECK(rc, "Error in libtest_FreeME(): PtlMEUnlink");
    }
}                                      /* end of libtest_FreeME() *//*}}} */

/*
** Create a Portal table entry. Use PTL_PT_ANY if you don't need
** a specific table entry.
*/
ptl_pt_index_t libtest_PTAlloc(ptl_handle_ni_t ni,
                            ptl_pt_index_t  request,
                            ptl_handle_eq_t eq)
{                                      /*{{{ */
    int            rc;
    ptl_pt_index_t index;

    rc = PtlPTAlloc(ni, 0, eq, request, &index);
    LIBTEST_CHECK(rc, "Error in libtest_PTAlloc(): PtlPTAlloc");
    if ((index != request) && (request != PTL_PT_ANY)) {
        fprintf(stderr, "Did not get the Ptl index I requested!\n");
        exit(1);
    }

    return index;
}                                      /* end of libtest_PTAlloc() *//*}}} */

/*
** Simple barrier
** Arrange processes around a ring. Send a message to the right indicating that
** this process has entered the barrier. Wait for a message from the left, indicating
** that the left neighbor has also entered. Then, send another message to the right
** indicating that this process is ready to leave the barrier, and then wait for the
** left neighbor to send the same message.
**
** libtest_Barrier_init() sets up the send-side MD, and the receive side LE and counter.
** libtest_Barrier() performs the above simple barrier on a ring.
*/
#define libtest_BarrierIndex (14)

static int             __my_rank = -1;
static ptl_rank_t      __nproc   = 1;
static ptl_handle_md_t __md_handle_barrier;
static ptl_handle_ct_t __ct_handle_barrier;
static ptl_size_t      __barrier_cnt;

void libtest_BarrierInit(ptl_handle_ni_t ni,
                      int             rank,
                      int             nproc)
{                                      /*{{{ */
    ptl_pt_index_t  index;
    ptl_handle_le_t le_handle;

    __my_rank           = rank;
    __nproc             = nproc;
    __barrier_cnt       = 1;
    __ct_handle_barrier = PTL_INVALID_HANDLE;

    /* Create the send side MD */
    __md_handle_barrier = libtest_CreateMD(ni, NULL, 0);

    /* We want a specific Portals table entry */
    index = libtest_PTAlloc(ni, libtest_BarrierIndex, PTL_EQ_NONE);
    assert(index == libtest_BarrierIndex);

    /* Create a counter and attach an LE to the Portal table */
    libtest_CreateLECT(ni, index, NULL, 0, &le_handle, &__ct_handle_barrier);
}                                      /* end of libtest_Barrier_init() *//*}}} */

void libtest_Barrier(void)
{                                      /*{{{ */
    int            rc;
    ptl_process_t  parent, leftchild, rightchild;
    ptl_size_t     test;
    ptl_ct_event_t cnt_value;

    parent.rank     = ((__my_rank + 1) >> 1) - 1;
    leftchild.rank  = ((__my_rank + 1) << 1) - 1;
    rightchild.rank = leftchild.rank + 1;

    if (leftchild.rank < __nproc) {
        /* Wait for my children to enter the barrier */
        test = __barrier_cnt++;
        if (rightchild.rank < __nproc) {
            test = __barrier_cnt++;
        }
        rc = PtlCTWait(__ct_handle_barrier, test, &cnt_value);
        LIBTEST_CHECK(rc, "1st PtlCTWait in libtest_Barrier");
    }

    if (__my_rank > 0) {
        /* Tell my parent that I have entered the barrier */
        rc = libtest_Put(__md_handle_barrier, 0, parent, libtest_BarrierIndex);
        LIBTEST_CHECK(rc, "1st PtlPut in libtest_Barrier");

        /* Wait for my parent to wake me up */
        test = __barrier_cnt++;
        rc   = PtlCTWait(__ct_handle_barrier, test, &cnt_value);
        LIBTEST_CHECK(rc, "2nd PtlCTWait in libtest_Barrier");
    }

    /* Wake my children */
    if (leftchild.rank < __nproc) {
        rc = libtest_Put(__md_handle_barrier, 0, leftchild, libtest_BarrierIndex);
        LIBTEST_CHECK(rc, "2nd PtlPut in libtest_Barrier");
        if (rightchild.rank < __nproc) {
            rc = libtest_Put(__md_handle_barrier, 0, rightchild, libtest_BarrierIndex);
            LIBTEST_CHECK(rc, "3rd PtlPut in libtest_Barrier");
        }
    }
}                                      /* end of libtest_Barrier() *//*}}} */

/*
** Allreduce on a ring
*/
ptl_pt_index_t         libtest_AllreduceIndex[2] = { 15, 16 };
static ptl_handle_md_t __md_handle_allreduce[2];
static ptl_handle_md_t __md_handle_allreduce_local;
static double          __allreduce_value[2];
static double          __allreduce_value_local;
static ptl_handle_ct_t __ct_handle_allreduce;
static ptl_size_t      __allreduce_cnt;

void libtest_AllreduceDouble_init(ptl_handle_ni_t ni)
{                                      /*{{{ */
    ptl_pt_index_t  index[2];
    ptl_handle_le_t le_handle;

    __allreduce_value[0]    = 98.0;
    __allreduce_value[1]    = 99.0;
    __allreduce_value_local = 55.0;
    __allreduce_cnt         = 1;
    __ct_handle_allreduce   = PTL_INVALID_HANDLE;

    /* We use two specific Portals table entry and alternate between them */
    index[0] = libtest_PTAlloc(ni, libtest_AllreduceIndex[0], PTL_EQ_NONE);
    index[1] = libtest_PTAlloc(ni, libtest_AllreduceIndex[1], PTL_EQ_NONE);

    /* Create a counter and attach an LE to each of the two Portal table entries */
    libtest_CreateLECT(ni, index[0], &__allreduce_value[0], sizeof(double), &le_handle, &__ct_handle_allreduce);
    libtest_CreateLECT(ni, index[1], &__allreduce_value[1], sizeof(double), &le_handle, &__ct_handle_allreduce);

    /* Create two send-side MDs */
    __md_handle_allreduce[0] = libtest_CreateMD(ni, &__allreduce_value[0], sizeof(double));
    __md_handle_allreduce[1] = libtest_CreateMD(ni, &__allreduce_value[1], sizeof(double));

    /* Create another MD to send my value */
    __md_handle_allreduce_local = libtest_CreateMD(ni, &__allreduce_value_local, sizeof(double));
}                                      /* end of libtest_AllreduceDouble_init() *//*}}} */

double libtest_AllreduceDouble(double   value,
                            ptl_op_t op)
{                                      /*{{{ */
    int            rc;
    double         ret;
    ptl_process_t  neighbor;
    ptl_size_t     test;
    ptl_ct_event_t cnt_value;
    static int     even = 0; // XXX not thread-safe

    assert(__my_rank >= 0);
    /* Send my value to my right neighbor if I am rank 0 to get things started */
    neighbor.rank = (__my_rank + 1) % __nproc;
    if (__my_rank == 0) {
        __allreduce_value_local = value;
        rc                      = libtest_Put(__md_handle_allreduce_local, sizeof(double), neighbor, libtest_AllreduceIndex[even]);
        LIBTEST_CHECK(rc, "1st PtlPut in libtest_AllreduceDouble");
    } else {
        /* Wait for the value from my left neighbor */
        test = __allreduce_cnt++;
        rc   = PtlCTWait(__ct_handle_allreduce, test, &cnt_value);
        LIBTEST_CHECK(rc, "1st PtlCTWait in libtest_AllreduceDouble");

        /* Add my value */
        __allreduce_value[even] += value;

        /* Send result to right neighbor */
        rc = libtest_Put(__md_handle_allreduce[even], sizeof(double), neighbor, libtest_AllreduceIndex[even]);
        LIBTEST_CHECK(rc, "2nd PtlPut in libtest_AllreduceDouble");
    }

    /* Wait for total */
    test = __allreduce_cnt++;
    rc   = PtlCTWait(__ct_handle_allreduce, test, &cnt_value);
    LIBTEST_CHECK(rc, "2nd PtlCTWait in libtest_AllreduceDouble");

    /* Pass it on, until we reach rank 0 again */
    if (__my_rank != (__nproc - 1)) {
        rc = libtest_Put(__md_handle_allreduce[even], sizeof(double), neighbor, libtest_AllreduceIndex[even]);
        LIBTEST_CHECK(rc, "3rd PtlPut in libtest_AllreduceDouble");
    }

    ret  = __allreduce_value[even];
    even = (even + 1) % 2;
    return ret;
}                                      /* end of libtest_AllreduceDouble() *//*}}} */

/* vim:set expandtab: */
