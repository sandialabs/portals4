/* -*- C -*-
 *
 * Copyright 2011 Sandia Corporation. Under the terms of Contract
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

/*
** This is a Portals 4 module for the NetPIPE benchmark.
** It has been developed for and tested with NetPIPE
** version 3.7.1
** This module uses non-matching list entries (LE) with
** counting events (CT).
** NetPIPE uses two ranks to run the benchmark. We use rank 0
** and rank _nprocs - 1 for that. A persistent LE at Portal
** table index PTL_XMIT_INDEX is used by both ranks to receive
** benchmark data. We use a counter on each to check for
** completion.
** NetPIPE also sends single integer and double values between
** those ranks to coordinate and measure the benchmark progress.
** We set up an LE over a single integer and single double each
** and again use a counter to check for completion.
*/


/*
** We assume Portals 4 is properly built and installed.
**
** Get NetPIPE from http://www.scl.ameslab.gov/netpipe/ and unpack it
** in the current directory. Then compile using this command:
**     cc -DMEMCPY NetPIPE-3.7.1/src/netpipe.c P4LEwithCT.c -I NetPIPE-3.7.1/src -lportals -lP4support -o NPptlLECT
**
** We define MEMCPY on the command line for netpipe.h. It seems harmless enough,
** and we have to define something for netpipe.h to work, but we
** don't really want to modify the NetPIPE distribution.
**
** Run it using
**     yod -c 2 -- ./NPptlLECT
**
** IB build example:
** gcc -I NetPIPE-3.7.1/src -I portals4/include -I portals4/test -DMEMCPY NetPIPE-3.7.1/src/netpipe.c P4LEwithCT.c -Lportals4_install/lib -Lportals4/test/.libs -L /cluster_tools/lw_linux/src/slurm-2.1.15-install/lib -lportals -ltestsupport -lpmi -o NPptlLECT
** srun -n 2 ./NPptlLECT
*/
#include <netpipe.h>
#include <portals4.h>
#include <support/support.h>


/* Some globals */
static int _my_rank;
static int _nprocs;
static ptl_handle_ni_t ni_logical;

/* MD and counters for benchmark data sends and receives */
static ptl_handle_md_t md_handle;
static int md_size;
static void *md_buf;
static ptl_handle_ct_t send_ct_handle;
static ptl_handle_ct_t recv_ct_handle;

/* MD and counters for single ints and double sends */
static ptl_handle_md_t send_int_md_handle;
static ptl_handle_md_t send_double_md_handle;
static ptl_handle_ct_t send_int_ct_handle;
static ptl_handle_ct_t send_double_ct_handle;

/* LE and counters for single ints and double receives */
static ptl_handle_le_t le_handle;
static int le_size;
static void *le_buf;
static ptl_handle_le_t recv_int_le_handle;
static ptl_handle_le_t recv_double_le_handle;
static ptl_handle_ct_t recv_int_ct_handle;
static ptl_handle_ct_t recv_double_ct_handle;

/* Keep track of total number of sends and receives */
static ptl_size_t total_sends= 1;
static ptl_size_t total_int_sends= 1;
static ptl_size_t total_double_sends= 1;

static ptl_size_t total_recvs= 1;
static ptl_size_t total_int_recvs= 1;
static ptl_size_t total_double_recvs= 1;

/* Temporary locations to send from */
static int send_int;
static double send_double;
static int recv_int;
static double recv_double;

#define PTL_XMIT_INDEX		(3)
#define PTL_SEND_DOUBLE_INDEX	(4)
#define PTL_SEND_INT_INDEX	(5)



void
Init(ArgStruct *p, int* pargc, char*** pargv)
{

int rc;
ptl_pt_index_t pt_handle;

    /* Initialize Portals and get some runtime info */
    rc= PtlInit();
    LIBTEST_CHECK(rc, "PtlInit");

    libtest_init();
    _my_rank= libtest_get_rank();
    _nprocs= libtest_get_size();

    if (_nprocs < 2)   {
	if (_my_rank == 0)   {
	    fprintf(stderr, "Need at least two processes!\n", _my_rank);
	}
        exit(-2);
    }

    /*
    ** We need an ni to do barriers and allreduces on.
    ** It needs to be a non-matching ni.
    */
    rc= PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_LOGICAL, PTL_PID_ANY, NULL, NULL, &ni_logical);
    LIBTEST_CHECK(rc, "PtlNIInit");

    rc= PtlSetMap(ni_logical, _nprocs, libtest_get_mapping(ni_logical));
    LIBTEST_CHECK(rc, "PtlSetMap");

    /* Initialize the barrier in the P4support library.  */
    libtest_BarrierInit(ni_logical, _my_rank, _nprocs);

    /* Allocate a Portal Table Index entry for data transmission */
    PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_XMIT_INDEX, &pt_handle);

    /* Allocate a Portal Table Index entry to receive an int */
    PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_SEND_INT_INDEX, &pt_handle);

    /* Allocate a Portal Table Index entry to receive a double */
    PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_SEND_DOUBLE_INDEX, &pt_handle);

    /* Set up the MD to send a single int */
    send_int_ct_handle= PTL_INVALID_HANDLE;
    libtest_CreateMDCT(ni_logical, &send_int, sizeof(int), &send_int_md_handle, &send_int_ct_handle);

    /* Set up the MD to send a single double */
    send_double_ct_handle= PTL_INVALID_HANDLE;
    libtest_CreateMDCT(ni_logical, &send_double, sizeof(double), &send_double_md_handle, &send_double_ct_handle);

    /* Create a persistent LE to receive a single int */
    recv_int_ct_handle= PTL_INVALID_HANDLE;
    libtest_CreateLECT(ni_logical, PTL_SEND_INT_INDEX, &recv_int, sizeof(int), &recv_int_le_handle, &recv_int_ct_handle);

    /* Create a persistent LE to receive a single double */
    recv_double_ct_handle= PTL_INVALID_HANDLE;
    libtest_CreateLECT(ni_logical, PTL_SEND_DOUBLE_INDEX, &recv_double, sizeof(double), &recv_double_le_handle, &recv_double_ct_handle);

    /*
    ** Initialize the benchmark data ct handles. Once allocated we'll
    ** reuse them, instead of reallocating them each time in
    ** AfterAlignmentInit()
    */
    send_ct_handle= PTL_INVALID_HANDLE;
    recv_ct_handle= PTL_INVALID_HANDLE;
    md_handle= PTL_INVALID_HANDLE;
    md_size= -1;
    md_buf= NULL;

    le_handle= PTL_INVALID_HANDLE;
    le_size= -1;
    le_buf= NULL;

    libtest_barrier();

}  /* end of Init() */



void
Setup(ArgStruct *p)
{

    p->tr= 0;
    p->rcv= 0;
    p->source_node= 0;

    if (_my_rank == 0)   {
        p->tr= 1;
	p->source_node= _nprocs - 1;
    } else if (_my_rank == (_nprocs - 1))   {
        p->rcv= 1;
	p->source_node= 0;
    }

}  /* end of Setup() */



void
Sync(ArgStruct *p)
{

    libtest_barrier();

}  /* end of Sync() */



/*
** No need for Portals to do anything here.
*/
void
PrepareToReceive(ArgStruct *p)
{
}  /* end of PrepareToReceive() */



/*
** Send a buffer's worth of data
*/
void
SendData(ArgStruct *p)
{

int rc;
int index;
ptl_process_t dest;
ptl_ct_event_t cnt_value;


    index= PTL_XMIT_INDEX;
    dest.rank= p->source_node;
    rc= PtlPut(md_handle, 0, p->bufflen, PTL_NO_ACK_REQ, dest, index, 0, 0, NULL, 0);
    LIBTEST_CHECK(rc, "PtlPut in SendData()");

    rc= PtlCTWait(send_ct_handle, total_sends, &cnt_value);
    LIBTEST_CHECK(rc, "PtlCTWait in SendData()");
    if (cnt_value.failure != 0)   {
	fprintf(stderr, "SendData() PtlPut failed %d (%d succeeded)\n",
	   (int)cnt_value.failure, (int)cnt_value.success);
    }

    total_sends++;

}  /* end of SendData() */



/*
** Wait for a buffer's worth of data to arrive
*/
void
RecvData(ArgStruct *p)
{

int rc;
ptl_ct_event_t cnt_value;


    rc= PtlCTWait(recv_ct_handle, total_recvs, &cnt_value);
    LIBTEST_CHECK(rc, "PtlCTWait in RecvData");
    if (cnt_value.failure != 0)   {
	fprintf(stderr, "RecvData() PtlPut failed %d (%d succeeded)\n",
	    (int)cnt_value.failure, (int)cnt_value.success);
   }

   total_recvs++;

}  /* end of RecvData() */



/*
** This is a function used to send the double value t to the other side
*/
void
SendTime(ArgStruct *p, double *t)
{

int rc;
int index;
ptl_process_t dest;
ptl_ct_event_t cnt_value;


    send_double= *t;
    index= PTL_SEND_DOUBLE_INDEX;
    dest.rank= p->source_node;
    rc= PtlPut(send_double_md_handle, 0, sizeof(double), PTL_NO_ACK_REQ, dest, index, 0, 0, NULL, 0);
    LIBTEST_CHECK(rc, "PtlPut in SendTime()");

    rc= PtlCTWait(send_double_ct_handle, total_double_sends, &cnt_value);
    LIBTEST_CHECK(rc, "PtlCTWait in SendTime()");
    if (cnt_value.failure != 0)   {
	fprintf(stderr, "SendTime() PtlPut failed %d (%d succeeded)\n",
	   (int)cnt_value.failure, (int)cnt_value.success);
    }

    total_double_sends++;

}  /* end of SendTime() */



/*
** This is a function used to receive the double value t from the other side
*/
void
RecvTime(ArgStruct *p, double *t)
{

int rc;
ptl_ct_event_t cnt_value;


    rc= PtlCTWait(recv_double_ct_handle, total_double_recvs, &cnt_value);
    LIBTEST_CHECK(rc, "PtlCTWait in RecvTime");
    if (cnt_value.failure != 0)   {
	fprintf(stderr, "RecvTime() PtlPut failed %d (%d succeeded)\n",
	    (int)cnt_value.failure, (int)cnt_value.success);
   }

   *t= recv_double;
   total_double_recvs++;

}  /* end of RecvTime() */



/*
** This is a function used to send the int value rpt to the other side
*/
void
SendRepeat(ArgStruct *p, int rpt)
{

int rc;
int index;
ptl_process_t dest;
ptl_ct_event_t cnt_value;


    send_int= rpt;
    index= PTL_SEND_INT_INDEX;
    dest.rank= p->source_node;
    rc= PtlPut(send_int_md_handle, 0, sizeof(int), PTL_NO_ACK_REQ, dest, index, 0, 0, NULL, 0);
    LIBTEST_CHECK(rc, "PtlPut in SendRepeat()");

    rc= PtlCTWait(send_int_ct_handle, total_int_sends, &cnt_value);
    LIBTEST_CHECK(rc, "PtlCTWait in SendRepeat()");
    if (cnt_value.failure != 0)   {
	fprintf(stderr, "SendRepeat() PtlPut failed %d (%d succeeded)\n",
	   (int)cnt_value.failure, (int)cnt_value.success);
    }

    total_int_sends++;

}  /* end of SendRepeat() */



/*
** This is a function used to receive the int value rpt from the other side
*/
void
RecvRepeat(ArgStruct *p, int *rpt)
{

int rc;
ptl_ct_event_t cnt_value;


    rc= PtlCTWait(recv_int_ct_handle, total_int_recvs, &cnt_value);
    LIBTEST_CHECK(rc, "PtlCTWait in RecvRepeat");
    if (cnt_value.failure != 0)   {
	fprintf(stderr, "RecvRepeat() PtlPut failed %d (%d succeeded)\n",
	    (int)cnt_value.failure, (int)cnt_value.success);
   }

   *rpt= recv_int;
   total_int_recvs++;

}  /* end of RecvRepeat() */



void
CleanUp(ArgStruct *p)
{

int rc;


    /* Free all CTs */
    rc= PtlCTFree(send_ct_handle);
    LIBTEST_CHECK(rc, "PtlCTFree(send_ct_handle) in CleanUp");

    rc= PtlCTFree(send_int_ct_handle);
    LIBTEST_CHECK(rc, "PtlCTFree(send_int_ct_handle) in CleanUp");

    rc= PtlCTFree(send_double_ct_handle);
    LIBTEST_CHECK(rc, "PtlCTFree(send_double_ct_handle) in CleanUp");

    rc= PtlCTFree(recv_ct_handle);
    LIBTEST_CHECK(rc, "PtlCTFree(recv_ct_handle) in CleanUp");

    rc= PtlCTFree(recv_int_ct_handle);
    LIBTEST_CHECK(rc, "PtlCTFree(recv_int_ct_handle) in CleanUp");

    rc= PtlCTFree(recv_double_ct_handle);
    LIBTEST_CHECK(rc, "PtlCTFree(recv_double_ct_handle) in CleanUp");

    /* Free all MDs */
    rc= PtlMDRelease(md_handle);
    LIBTEST_CHECK(rc, "PtlMDRelease(md_handle) in CleanUp");

    rc= PtlMDRelease(send_int_md_handle);
    LIBTEST_CHECK(rc, "PtlMDRelease (send_int_md_handle) in CleanUp");

    rc= PtlMDRelease(send_double_md_handle);
    LIBTEST_CHECK(rc, "PtlMDRelease (send_double_md_handle) in CleanUp");

    /* Free all LEs */
    rc= PtlLEUnlink(le_handle);
    LIBTEST_CHECK(rc, "PtlLEUnlink(le_handle) in CleanUp");

    rc= PtlLEUnlink(recv_int_le_handle);
    LIBTEST_CHECK(rc, "PtlLEUnlink(recv_int_le_handle) in CleanUp");

    rc= PtlLEUnlink(recv_double_le_handle);
    LIBTEST_CHECK(rc, "PtlLEUnlink(recv_double_le_handle) in CleanUp");

    /* Free the Portal table entries we used */
    rc= PtlPTFree(ni_logical, PTL_XMIT_INDEX);
    LIBTEST_CHECK(rc, "PtlPTFree(PTL_XMIT_INDEX) in CleanUp");

    rc= PtlPTFree(ni_logical, PTL_SEND_INT_INDEX);
    LIBTEST_CHECK(rc, "PtlPTFree(PTL_SEND_INT_INDEX) in CleanUp");

    rc= PtlPTFree(ni_logical, PTL_SEND_DOUBLE_INDEX);
    LIBTEST_CHECK(rc, "PtlPTFree(PTL_SEND_DOUBLE_INDEX) in CleanUp");

    /* Almost done */
    PtlNIFini(ni_logical);
    PtlFini();

}  /* end of CleanUp() */



/*
** because NetPIPE calls to Reset() and AfterAlignmentInit() are not symetrical,
** we just don't do anything here and let AfterAlignmentInit() handle it all.
*/
void
Reset(ArgStruct *p)
{
}  /* end of Reset() */



/*
** Buffers in args.r_buff and args.s_buff have been allocated and aligned.
** We setup an MD over s_buff and an LE over r_buff. This gets called
** outside the timing loop, but we still try to be a little efficient here.
** We only free and re-allocate an MD/LE if the buffer address or
** length has changed.
*/
void
AfterAlignmentInit(ArgStruct *p)
{

int rc;


    /* Create a persistent ME to send from */
    if (PtlHandleIsEqual(md_handle, PTL_INVALID_HANDLE))   {
	/* First time here, setup an MD to send benchmark data */
	libtest_CreateMDCT(ni_logical, p->s_buff, p->bufflen, &md_handle, &send_ct_handle);
	md_size= p->bufflen;
	md_buf= p->s_buff;
    } else if ((md_size != p->bufflen) || (md_buf != p->s_buff))   {
	/* Release the existing MD and create a new one */
	rc= PtlMDRelease(md_handle);
	LIBTEST_CHECK(rc, "PtlMDRelease(md_handle) in AfterAlignmentInit");
	libtest_CreateMDCT(ni_logical, p->s_buff, p->bufflen, &md_handle, &send_ct_handle);
	md_size= p->bufflen;
	md_buf= p->s_buff;
    } else   {
	/* Just keep the MD we already have */
    }

    /* Create a persistent LE to receive into */
    if (PtlHandleIsEqual(le_handle, PTL_INVALID_HANDLE))   {
	libtest_CreateLECT(ni_logical, PTL_XMIT_INDEX, p->r_buff, p->bufflen,
		&le_handle, &recv_ct_handle);
	le_size= p->bufflen;
	le_buf= p->r_buff;
    } else if ((le_size != p->bufflen) || (le_buf != p->r_buff))   {
	rc= PtlLEUnlink(le_handle);
	LIBTEST_CHECK(rc, "PtlLEUnlink(le_handle) in CleanUp");
	libtest_CreateLECT(ni_logical, PTL_XMIT_INDEX, p->r_buff, p->bufflen,
		&le_handle, &recv_ct_handle);
	le_size= p->bufflen;
	le_buf= p->r_buff;
    } else   {
	/* Just keep the LE we already have */
    }

}  /* end of AfterAlignmentInit() */
