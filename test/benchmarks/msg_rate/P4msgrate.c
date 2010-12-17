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

/*
** This is an adaption of the message rate benchmark from
** http://www.cs.sandia.gov/smb/msgrate.html to the Portals 4
** API.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <portals4.h>
#include <portals4_runtime.h>
#include <libP4support.h>



#define TestOneWayIndex	(1)
#define TestSameDirectionIndex	(2)
#define SEND_BUF_SIZE	(npeers * nmsgs * nbytes)
#define RECV_BUF_SIZE	(SEND_BUF_SIZE)
#define LEwithCT	(0)
#define MEwithEQ	(1)


/* configuration parameters - setable by command line arguments */
int ppn;
int machine_output;



/*
** globals
*/
int *send_peers;
int *recv_peers;
char *send_buf;
char *recv_buf;

int rank;
int world_size;



/*
** Local functions
*/
static void
abort_app(const char *msg)
{

    perror(msg);
    exit(1);

}  /* end of abort_app() */


static void
cache_invalidate(int cache_size, int *cache_buf)
{
    int i;

    if (cache_size != 0) {
	cache_buf[0]= 1;
	for (i= 1 ; i < cache_size; i++)   {
	    cache_buf[i]= cache_buf[i - 1];
	}
    }
}  /* end of cache_invalidate() */


static inline double
timer(void)
{

struct timespec tm;


    clock_gettime(CLOCK_REALTIME, &tm);
    return tm.tv_sec + tm.tv_nsec / 1000000000.0;

}  /* end of timer() */


static void
display_result(const char *test, const double result)
{
    if (0 == rank)   {
        if (machine_output)   {
            printf("%.2f ", result);
        } else   {
            printf("%20s: %.2f\n", test, result);
        }
    }

}  /* end of display_result() */


static void
test_one_way(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers, int nmsgs,
	int nbytes, int niters, int test_type, int verbose)
{

int i, j, k;
int rc;
double tmp, total;
ptl_pt_index_t index;
ptl_process_t dest;
ptl_size_t offset;
ptl_handle_md_t md_handle;
ptl_handle_ct_t ct_handle;
ptl_ct_event_t cnt_value;
ptl_handle_le_t le_handle;
ptl_handle_me_t me_handles[nmsgs];
ptl_handle_eq_t eq_handle;
ptl_match_bits_t match;
ptl_event_t event;


    if ((test_type != LEwithCT) && (test_type != MEwithEQ))   {
	/* Invalid test type */
	return;
    }

    total= 0;
    match= 0;
    ct_handle= PTL_INVALID_HANDLE;
    __PtlBarrier();

    if (rank < (world_size / 2))   {
	/* The first half of the ranks are senders */
	if ((verbose > 1) && (rank == 0))   {
	    printf("Ranks 0..%d will send %d %d-byte messages %d times\n",
		world_size / 2 - 1, nmsgs, nbytes, niters);
	}

	/* Set up the MD to send from */
	__PtlCreateMDCT(ni, send_buf, SEND_BUF_SIZE, &md_handle, &ct_handle);

	/* Run the test */
	for (i= 0; i < niters; ++i)   {
	    cache_invalidate(cache_size, cache_buf);

	    __PtlBarrier();

	    tmp= timer();
	    for (k= 0; k < nmsgs; k++)   {
		if (test_type == MEwithEQ)   {
		    offset= 0;
		} else if (test_type == LEwithCT)   {
		    offset= nbytes * k;
		}

		dest.rank= rank + (world_size / 2);
		/* For the LEwithCT case, we ignore the match bits */
		match= k;
		rc= __PtlPut_offset(md_handle, offset, nbytes, dest, TestOneWayIndex, match, offset);
		PTL_CHECK(rc, "PtlPut in test_one_way");
	    }

	    rc= PtlCTWait(ct_handle, (i + 1) * nmsgs, &cnt_value);
	    total += (timer() - tmp);

	    PTL_CHECK(rc, "PtlCTWait in test_one_way");
	    if (cnt_value.failure != 0)   {
		fprintf(stderr, "test_one_way() %d PtlPut failed (%d/%d succeeded)\n",
		   (int)cnt_value.failure, (int)cnt_value.success, (i + 1) * nmsgs);
	    }

	    if ((verbose > 3) && (rank == 0))   {
		printf("test_one_way() iteration %d done\n", i);
	    }
	}

	/* Clean up the send side */
	rc= PtlCTFree(ct_handle);
	PTL_CHECK(rc, "PtlCTFree in test_one_way");
	PtlMDRelease(md_handle);
	PTL_CHECK(rc, "PtlMDRelease in test_one_way");

    } else   {

	/* The second half of the ranks are receivers */
	if ((verbose > 1) && (rank == (world_size / 2)))   {
	    printf("Ranks %d..%d will receive %d %d-byte messages %d times\n",
		world_size / 2, world_size - 1, nmsgs, nbytes, niters);
	}

	if (test_type == MEwithEQ)   {
	    /* Create an event queue */
	    rc = PtlEQAlloc(ni, 2 * nmsgs, &eq_handle);
	    PTL_CHECK(rc, "PtlEQAlloc in test_one_way");
	} else   {
	    eq_handle= PTL_EQ_NONE;
	}

	/* Allocate the Portal to send to */
	index= __PtlPTAlloc(ni, TestOneWayIndex, eq_handle);

	if (test_type == LEwithCT)   {
	    /* Create a persistent LE to receive into */
	    __PtlCreateLECT(ni, index, recv_buf, RECV_BUF_SIZE, &le_handle, &ct_handle);
	} else if (test_type == MEwithEQ)   {
	    /* Create "nmsgs" persistent MEs to receive into */
	    __PtlCreateME(ni, index, recv_buf, nbytes, nmsgs, me_handles);
	}

	/*
	** In the MPI version of this benchmark, the MPI_Irecv() are
	** posted inside a loop as the sends are going on. This can cause
	** unexpected messages.
	** For the Portals version, a large LE is posted and then all
	** we do is wait for the data to arrive.
	*/

	/* Run the test */
	for (i= 0; i < niters; i++)   {
	    cache_invalidate(cache_size, cache_buf);

	    __PtlBarrier();

	    tmp= timer();
	    if (test_type == LEwithCT)   {
		rc= PtlCTWait(ct_handle, (i + 1) * nmsgs, &cnt_value);
	    } else if (test_type == MEwithEQ)   {
		/* Pull each event off as it comes in */
		for (j= 0; j < nmsgs; j++)   {
		    rc= PtlEQWait(eq_handle, &event);
		    PTL_CHECK(rc, "PtlEQWait in test_one_way");
		    if (event.type != PTL_EVENT_PUT)   {
			fprintf(stderr, "test_prepost() Got event %d instead of PTL_EVENT_PUT with msg %d\n",
			    event.type, j);
		    }
		    if (event.ni_fail_type != PTL_NI_OK)   {
			fprintf(stderr, "test_prepost() Got event %d with fail type %d\n",
			    j, event.ni_fail_type);
		    }
		}
	    }
	    total += (timer() - tmp);

	    if (test_type == LEwithCT)   {
		PTL_CHECK(rc, "PtlCTWait in test_one_way");
		if (cnt_value.failure != 0)   {
		    fprintf(stderr, "test_one_way() %d PtlPut failed (%d/%d succeeded)\n",
		       (int)cnt_value.failure, (int)cnt_value.success, (i + 1) * nmsgs);
		}
	    }
	}

	/* Clean up the receive side */
	if (test_type == LEwithCT)   {
	    rc= PtlCTFree(ct_handle);
	    PTL_CHECK(rc, "PtlCTFree in test_one_way");
	    rc= PtlLEUnlink(le_handle);
	    PTL_CHECK(rc, "PtlLEUnlink in test_one_way");
	} else if (test_type == MEwithEQ)   {
	    __PtlFreeME(nmsgs, me_handles);
	    PtlEQFree(eq_handle);
	}
	rc= PtlPTFree(ni, index);
	PTL_CHECK(rc, "PtlPTFree in test_one_way");
    }

    tmp= __PtlAllreduceDouble(total, PTL_SUM);
    display_result("single direction", (niters * nmsgs) / (tmp / world_size));

    __PtlBarrier();

}  /* end of test_one_way() */


static void
test_same_direction(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers, int nmsgs,
	int nbytes, int niters, int verbose)
{

int i, j, k, nreqs;
double tmp, total;

    total= 0;
    __PtlBarrier();
    if (rank == 0)   {
	printf("%20s: not implemented yet\n", "pair-based");
    }
    return;

    for (i= 0; i < niters; i++)   {
        cache_invalidate(cache_size, cache_buf);

	__PtlBarrier();

        tmp= timer();
        for (j= 0; j < npeers; j++)   {
            nreqs= 0;
            for (k= 0; k < nmsgs; k++)   {
#if 0
                MPI_Irecv(recv_buf + (nbytes * (k + j * nmsgs)),
                          nbytes, MPI_CHAR, recv_peers[j], magic_tag, 
                          MPI_COMM_WORLD, &reqs[nreqs++]);
#endif
            }
            for (k= 0; k < nmsgs; k++)   {
#if 0
                MPI_Isend(send_buf + (nbytes * (k + j * nmsgs)),
                          nbytes, MPI_CHAR, send_peers[npeers - j - 1], magic_tag, 
                          MPI_COMM_WORLD, &reqs[nreqs++]);
#endif
            }
#if 0
            MPI_Waitall(nreqs, reqs, MPI_STATUSES_IGNORE);
#endif
        }
        total += (timer() - tmp);
    }

    tmp= __PtlAllreduceDouble(total, PTL_SUM);
    display_result("pair-based", (niters * npeers * nmsgs * 2) / (tmp / world_size));

}  /* end of test_same_direction() */


static void
test_prepost(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers, int nmsgs,
	int nbytes, int niters, int test_type, int verbose)
{

int i, j, k;
double tmp, total;
int rc;

ptl_pt_index_t index;
ptl_process_t dest;
ptl_size_t offset;
ptl_handle_md_t md_handle;
ptl_handle_ct_t ct_handle;
ptl_ct_event_t cnt_value;
ptl_handle_le_t le_handle;
ptl_handle_me_t me_handles[npeers * nmsgs];
ptl_match_bits_t match;
ptl_handle_eq_t eq_handle;
ptl_event_t event;


    total= 0;
    ct_handle= PTL_INVALID_HANDLE;
    if (verbose > 1)    {
	/* Some info, if desired. */
	for (i= 0; i < world_size; i++)   {
	    if (i == rank)   {
		printf("Rank %d will send %d %d-byte messages %d times to rank(s) ",
		    rank, nmsgs, nbytes, niters);
		for (j= 0; j < npeers; j++)   {
		    printf("%d, ", send_peers[npeers - j - 1]);
		}
		printf("and receive from ");
		for (j= 0; j < npeers; j++)   {
		    printf("%d", recv_peers[j]);
		    if (j < (npeers - 1))   {
			printf(", ");
		    }
		}
		printf("\n");
	    }
	    __PtlBarrier();
	}
    }

    /*
    ** Setup the send side
    */
    match= 0;

    /* Set up the MD to send from */
    __PtlCreateMDCT(ni, send_buf, SEND_BUF_SIZE, &md_handle, &ct_handle);

    /*
    ** Setup the receive side
    ** We use the same counter as for the sends, so we only
    ** have to check in one place for completion.
    */

    if (test_type == MEwithEQ)   {
	/* Create an event queue */
	rc = PtlEQAlloc(ni, 2 * nmsgs * npeers, &eq_handle);
	PTL_CHECK(rc, "PtlEQAlloc in test_prepost");
    } else   {
	eq_handle= PTL_EQ_NONE;
    }

    /* Allocate the Portal to send to */
    index= __PtlPTAlloc(ni, TestSameDirectionIndex, eq_handle);

    if (test_type == LEwithCT)   {
	/* Create a persistent LE to receive into */
	__PtlCreateLECT(ni, index, recv_buf, RECV_BUF_SIZE, &le_handle, &ct_handle);
    } else if (test_type == MEwithEQ)   {
	/* Create "npeers * nmsgs" persistent MEs to receive into */
	__PtlCreateME(ni, index, recv_buf, nbytes, npeers * nmsgs, me_handles);
    }

    /* Sync everybody */
    __PtlBarrier();

    /* Run the test */
    for (i= 0; i < niters; i++)   {
        cache_invalidate(cache_size, cache_buf);

        __PtlBarrier();

        tmp= timer();
        for (j= 0; j < npeers; ++j)   {
            for (k= 0; k < nmsgs; ++k)   {
		if (test_type == MEwithEQ)   {
		    offset= 0;
		} else if (test_type == LEwithCT)   {
		    offset= nbytes * k;
		}

		offset= (nbytes * (k + j * nmsgs));
		dest.rank= send_peers[npeers - j - 1];
		/* For the LEwithCT case, we ignore the match bits */
		match= (npeers - j - 1) * nmsgs + k;
		rc= __PtlPut_offset(md_handle, offset, nbytes, dest, index, match, offset);
		PTL_CHECK(rc, "PtlPut in test_same_direction");
            }
	}

	/* MPI_Waitall */
	if (test_type == LEwithCT)   {
	    rc= PtlCTWait(ct_handle, 2*((i+1)*nmsgs*npeers), &cnt_value);
	    PTL_CHECK(rc, "1st PtlCTWait in test_same_direction");
	    if (cnt_value.failure != 0)   {
		fprintf(stderr, "test_same_direction() PtlCTWait failed (%d failure, %d success)\n",
			(int)cnt_value.failure, (int)cnt_value.success);
	    }
	} else if (test_type == MEwithEQ)   {
	    /* We also have to wait for the send count */
	    rc= PtlCTWait(ct_handle, 1*((i+1)*nmsgs*npeers), &cnt_value);
	    PTL_CHECK(rc, "2nd PtlCTWait in test_same_direction");
	    if (cnt_value.failure != 0)   {
		fprintf(stderr, "test_same_direction() PtlPut send failed (%d/%d succeeded)\n",
			(int)cnt_value.failure, (int)cnt_value.success);
	    }

	    /* Pull each event off as it comes in */
	    for (j= 0; j < npeers * nmsgs; j++)   {
		rc= PtlEQWait(eq_handle, &event);
		PTL_CHECK(rc, "PtlEQWait in test_one_way");
		if (event.type != PTL_EVENT_PUT)   {
		    fprintf(stderr, "test_prepost() Got event %d instead of PTL_EVENT_PUT with msg %d\n",
			event.type, j);
		}
		if (event.ni_fail_type != PTL_NI_OK)   {
		    fprintf(stderr, "test_prepost() Got event %d with fail type %d\n",
			j, event.ni_fail_type);
		}
	    }
	}
        total += (timer() - tmp);
    }

    /* Clean up the send side */
    rc= PtlCTFree(ct_handle);
    PTL_CHECK(rc, "PtlCTFree in test_same_direction");
    PtlMDRelease(md_handle);
    PTL_CHECK(rc, "PtlMDRelease in test_same_direction");

    /* Clean up the receive side */
    if (test_type == LEwithCT)   {
	rc= PtlLEUnlink(le_handle);
	PTL_CHECK(rc, "PtlLEUnlink in test_same_direction");
    } else if (test_type == MEwithEQ)   {
	__PtlFreeME(npeers * nmsgs, me_handles);
	PtlEQFree(eq_handle);
    }
    rc= PtlPTFree(ni, index);
    PTL_CHECK(rc, "PtlPTFree in test_same_direction");

    tmp= __PtlAllreduceDouble(total, PTL_SUM);
    display_result("pre-post", (niters * npeers * nmsgs * 2) / (tmp / world_size));

}  /* end of test_prepost() */


static void
test_allstart(int cache_size, int *cache_buf, ptl_handle_ni_t ni, int npeers, int nmsgs,
	int nbytes, int niters, int verbose)
{

int i, j, k, nreqs;
double tmp, total;

    total= 0;
    __PtlBarrier();
    if (rank == 0)   {
	printf("%20s: not implemented yet\n", "all-start");
    }
    return;

    for (i= 0; i < niters; i++)   {
        cache_invalidate(cache_size, cache_buf);

        __PtlBarrier();

        tmp= timer();
        nreqs= 0;
        for (j= 0; j < npeers; j++)   {
            for (k= 0; k < nmsgs; k++)   {
#if 0
                MPI_Irecv(recv_buf + (nbytes * (k + j * nmsgs)),
                          nbytes, MPI_CHAR, recv_peers[j], magic_tag, 
                          MPI_COMM_WORLD, &reqs[nreqs++]);
#endif
            }
            for (k= 0; k < nmsgs; k++)   {
#if 0
                MPI_Isend(send_buf + (nbytes * (k + j * nmsgs)),
                          nbytes, MPI_CHAR, send_peers[npeers - j - 1], magic_tag, 
                          MPI_COMM_WORLD, &reqs[nreqs++]);
#endif
            }
        }
#if 0
        MPI_Waitall(nreqs, reqs, MPI_STATUSES_IGNORE);
#endif
        total += (timer() - tmp);
    }

    tmp= __PtlAllreduceDouble(total, PTL_SUM);
    display_result("all-start", (niters * npeers * nmsgs * 2) / (tmp / world_size));

}  /* end of test_allstart() */


static void
usage(void)
{
    fprintf(stderr, "Usage: msgrate -n <ppn> [OPTION]...\n\n");
    fprintf(stderr, "  -h           Display this help message and exit\n");
    fprintf(stderr, "  -p <num>     Number of peers used in communication\n");
    fprintf(stderr, "  -i <num>     Number of iterations per test\n");
    fprintf(stderr, "  -m <num>     Number of messages per peer per iteration\n");
    fprintf(stderr, "  -s <size>    Number of bytes per message\n");
    fprintf(stderr, "  -c <size>    Cache size in bytes\n");
    fprintf(stderr, "  -n <ppn>     Number of procs per node\n");
    fprintf(stderr, "  -t <test>    0 for LE and CT, 1 for ME and full events\n");
    fprintf(stderr, "  -o           Format output to be machine readable\n");
    fprintf(stderr, "  -v           Increase verbosity. Using -v -v or more may impact test results!\n");
    fprintf(stderr, "\nReport bugs to <bwbarre@sandia.gov>\n");
}


int
main(int argc, char *argv[])
{

int ch;
int start_err= 0;
int rc;
int i;
int verbose;
int npeers;
int niters;
int nmsgs;
int nbytes;
ptl_process_t *amapping;
ptl_handle_ni_t ni_logical;
ptl_handle_ni_t ni_collectives;
int cache_size;
int *cache_buf;
int test_type;


    /* Set some defaults */
    verbose= 0;
    cache_size= (8 * 1024 * 1024 / sizeof(int));
    rank= -1;
    world_size= -1;
    npeers= 6;
    niters= 4096;
    nmsgs= 128;
    nbytes= 8;
    ppn= -1;
    machine_output= 0;
    test_type= LEwithCT;


    /* Initialize Portals and get some runtime info */
    rc= PtlInit();
    PTL_CHECK(rc, "PtlInit");
    rank= runtime_get_rank();
    world_size= runtime_get_size();


    amapping= malloc(sizeof(ptl_process_t) * world_size);
    if (amapping == NULL)   {
	fprintf(stderr, "Out of memory on rank %d\n", rank);
	exit(2);
    }

    /* Handle command line arguments */
    while (start_err != 1 && 
	   (ch= getopt(argc, argv, "p:i:m:s:c:n:ohvt:")) != -1)   {
	switch (ch)   {
	    case 'p':
		npeers= strtol(optarg, (char **)NULL, 0);
		break;
	    case 'i':
		niters= strtol(optarg, (char **)NULL, 0);
		break;
	    case 'm':
		nmsgs= strtol(optarg, (char **)NULL, 0);
		break;
	    case 's':
		nbytes= strtol(optarg, (char **)NULL, 0);
		break;
	    case 't':
		if (strcmp("0", optarg) == 0)   {
		    test_type= LEwithCT;
		} else if (strcmp("1", optarg) == 0)   {
		    test_type= MEwithEQ;
		} else   {
		    if (rank == 0)   {
			fprintf(stderr, "Unknown test! Use -t 0 for LE with couting events test, and\n");
			fprintf(stderr, "                  -t 1 for ME with event queue test.\n");
		    }
		    start_err= 1;
		}
		break;
	    case 'c':
		cache_size= strtol(optarg, (char **)NULL, 0) / sizeof(int);
		break;
	    case 'n':
		ppn= strtol(optarg, (char **)NULL, 0);
		break;
	    case 'o':
		machine_output= 1;
		break;
	    case 'v':
		verbose++;
		break;
	    case 'h':
	    case '?':
	    default:
		start_err= 1;
		if (rank == 0)   {
		    usage();
		}
	}
    }

    /* sanity check */
    if (start_err != 1)   {
	if (world_size % 2 != 0)   {
	    if (rank == 0)   {
		fprintf(stderr, "Must run on an even number of ranks.\n");
	    }
	    start_err= 1;
	/*} else if (world_size < 3)   {
	    if (0 == rank)   {
		fprintf(stderr, "Error: At least three processes are required\n");
	    }
	    start_err= 1;*/
	} else if (world_size <= npeers)   {
	    if (0 == rank)   {
		fprintf(stderr, "Error: job size (%d) <= number of peers (%d)\n",
		    world_size, npeers);
	    }
	    start_err= 1;
	} else if (ppn < 1)   {
	    if (0 == rank)   {
		fprintf(stderr, "Error: must specify process per node (-n #)\n");
	    }
	    start_err= 1;
	} else if (world_size / ppn <= npeers)   {
	    if (0 == rank)   {
		fprintf(stderr, "Error: node count <= number of peers\n");
	    }
	    start_err= 1;
	}
    }

    if (0 != start_err)   {
	PtlFini();
        exit(1);
    }

    /*
    ** We need an ni to do barriers and allreduces on.
    ** It needs to be a non-matching ni, so we can't share it
    ** in the MEwithEQ case.
    */
    ptl_ni_limits_t actual;

    rc= PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_LOGICAL, PTL_PID_ANY, NULL,
	    NULL, world_size, NULL, amapping, &ni_collectives);

    if (test_type == LEwithCT)   {
	rc= PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_LOGICAL, PTL_PID_ANY, NULL,
		&actual, world_size, NULL, amapping, &ni_logical);
    } else if (test_type == MEwithEQ)   {
	rc= PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_LOGICAL, PTL_PID_ANY, NULL,
		&actual, world_size, NULL, amapping, &ni_logical);
    } else   {
	rc= PTL_NO_INIT;
    }
    PTL_CHECK(rc, "PtlNIInit");


    if (0 == rank)   {
        if (!machine_output)   {
	    if (verbose > 0)   {
		printf("NI actual limits\n");
		printf("  max_entries:          %d\n", actual.max_entries);
		printf("  max_overflow_entries: %d\n", actual.max_overflow_entries);
		printf("  max_mds:              %d\n", actual.max_mds);
		printf("  max_cts:              %d\n", actual.max_cts);
		printf("  max_eqs:              %d\n", actual.max_eqs);
		printf("  max_pt_index:         %d\n", actual.max_pt_index);
		printf("  max_iovecs:           %d\n", actual.max_iovecs);
		printf("  max_list_size:        %d\n", actual.max_list_size);
		printf("  max_msg_size:         %d\n", (int)actual.max_msg_size);
		printf("  max_atomic_size:      %d\n", (int)actual.max_atomic_size);
		printf("  max_ordered_size:     %d\n", (int)actual.max_ordered_size);
	    }

            printf("job size:   %d\n", world_size);
            printf("npeers:     %d\n", npeers);
            printf("niters:     %d\n", niters);
            printf("nmsgs:      %d\n", nmsgs);
            printf("nbytes:     %d\n", nbytes);
            printf("cache size: %d\n", cache_size * (int)sizeof(int));
            printf("ppn:        %d\n", ppn);
	    if (test_type == LEwithCT)   {
		printf("test:       LE with counting events\n");
	    } else if (test_type == MEwithEQ)   {
		printf("test:       ME with event queue\n");
	    } else   {
		printf("test:       Invalid\n");
	    }
        } else   {
            printf("%d %d %d %d %d %d %d ", 
                   world_size, npeers, niters, nmsgs, nbytes,
                   cache_size * (int)sizeof(int), ppn);
        }
    }

    if (nmsgs * npeers > actual.max_list_size)   {
	if (rank == 0)   {
	    fprintf(stderr, "Not enough max match list entries. Need %d.\n",
		nmsgs * npeers);
	}
	exit(-1);
    }

    /* allocate buffers */
    send_peers= malloc(sizeof(int) * npeers);
    if (NULL == send_peers)   {
	abort_app("malloc send_peers");
    }

    recv_peers= malloc(sizeof(int) * npeers);
    if (NULL == recv_peers)   {
	abort_app("malloc recv_peers");
    }

    cache_buf= malloc(sizeof(int) * cache_size);
    if (NULL == cache_buf)   {
	abort_app("malloc cache_buf");
    }

    send_buf= malloc(SEND_BUF_SIZE);
    if (NULL == send_buf)   {
	abort_app("malloc send_buf");
    }

    recv_buf= malloc(RECV_BUF_SIZE);
    if (NULL == recv_buf)   {
	abort_app("malloc recv_buf");
    }


    /* calculate peers */
    for (i= 0; i < npeers; i++)    {
        if (i < npeers / 2)    {
            send_peers[i]= (rank + world_size + ((i - npeers / 2) * ppn)) % world_size;
        } else    {
            send_peers[i]= (rank + world_size + ((i - npeers / 2 + 1) * ppn)) % world_size;
        }
    }
    if ((npeers % 2) == 0)    {
        /* even */
        for (i= 0; i < npeers; i++)    {
            if (i < (npeers / 2))    {
                recv_peers[i]= (rank + world_size + ((i - npeers / 2) *ppn)) % world_size;
            } else    {
                recv_peers[i]= (rank + world_size + ((i - npeers / 2 + 1) * ppn)) % world_size;
            }
        } 
    } else    {
        /* odd */
        for (i= 0; i < npeers; i++)    {
            if (i < (npeers / 2 + 1))    {
                recv_peers[i]= (rank + world_size + ((i - npeers / 2 - 1) * ppn)) % world_size;
            } else    {
                recv_peers[i]= (rank + world_size + ((i - npeers / 2) * ppn)) % world_size;
            }
        }
    }

    /* BWB: FIX ME: trash the free lists / malloc here */


    /*
    ** Initialize the barrier and allreduce for doubles in the P4support library.
    ** The sync everybody before testing and calls to the PTL barrier and allreduce
    ** begin.
    */
    __PtlBarrierInit(ni_collectives, rank, world_size);
    __PtlAllreduceDouble_init(ni_collectives);
    runtime_barrier();
    free(amapping);  /* Not needed anymore */

    /* run tests */
    if (verbose > 0)   {
	printf("Rank %3d: Starting test_one_way(nmsgs %d, nbytes %d, niters %d)\n", rank,
	    nmsgs, nbytes, niters);
    }
    test_one_way(cache_size, cache_buf, ni_logical, npeers, nmsgs, nbytes, niters,
	    test_type, verbose);

    if (verbose > 0)   {
	printf("Rank %3d: Starting test_same_direction(nmsgs %d, nbytes %d, niters %d)\n", rank,
	    nmsgs, nbytes, niters);
    }
    test_same_direction(cache_size, cache_buf, ni_logical, npeers, nmsgs, nbytes, niters, verbose);

    if (verbose > 0)   {
	printf("Rank %3d: Starting test_prepost(nmsgs %d, nbytes %d, niters %d)\n", rank,
	    nmsgs, nbytes, niters);
    }
    test_prepost(cache_size, cache_buf, ni_logical, npeers, nmsgs, nbytes, niters,
	    test_type, verbose);

    if (verbose > 0)   {
	printf("Rank %3d: Starting test_allstart(nmsgs %d, nbytes %d, niters %d)\n", rank,
	    nmsgs, nbytes, niters);
    }
    test_allstart(cache_size, cache_buf, ni_logical, npeers, nmsgs, nbytes, niters, verbose);

    if ((rank == 0) && machine_output)   {
	printf("\n");
    }

    /* done */
    PtlNIFini(ni_logical);
    PtlNIFini(ni_collectives);
    PtlFini();
    return 0;

}  /* end of main() */
