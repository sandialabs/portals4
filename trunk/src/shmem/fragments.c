#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <portals4.h>

/* System headers */
#include <stdlib.h>		       /* for size_t */
#include <assert.h>		       /* for assert() and abort() */

#include <stdio.h>

/* Internal headers */
#include "ptl_internal_fragments.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_commpad.h"
#include "ptl_visibility.h"

/* Fragment format:
 *
 * 8 bytes - Next ptr
 * 8 bytes - Size (128 or 4096 at the moment)
 * X bytes - the rest
 */

size_t SMALL_FRAG_SIZE = 128;
size_t SMALL_FRAG_PAYLOAD = 112;
size_t SMALL_FRAG_COUNT = 512;
size_t LARGE_FRAG_PAYLOAD = 4080;
size_t LARGE_FRAG_SIZE = 4096;
size_t LARGE_FRAG_COUNT = 128;

typedef struct {
    void *next;
    uint64_t size;
    char data[];
} fragment_hdr_t;

typedef struct {
    fragment_hdr_t *volatile head;
    fragment_hdr_t *volatile tail;
} NEMESIS_queue;

static fragment_hdr_t *small_free_list = NULL;
static fragment_hdr_t *large_free_list = NULL;
static NEMESIS_queue *receiveQ = NULL;
static NEMESIS_queue *ackQ = NULL;

void INTERNAL PtlInternalFragmentSetup(
    volatile char *buf)
{
    size_t i;
    fragment_hdr_t *fptr;

    /* init metadata */
    SMALL_FRAG_PAYLOAD = SMALL_FRAG_SIZE - sizeof(void *) - sizeof(uint64_t);
    LARGE_FRAG_PAYLOAD = LARGE_FRAG_SIZE - sizeof(void *) - sizeof(uint64_t);
    /* first, initialize the receive queue */
    receiveQ = (NEMESIS_queue *) buf;
    receiveQ->head = NULL;
    receiveQ->tail = NULL;
    /* next, initialize the ack queue */
    ackQ = receiveQ + 1;
    ackQ->head = NULL;
    ackQ->tail = NULL;
    /* now, initialize the small fragment free-list */
    fptr = (fragment_hdr_t *) (ackQ + 1);
    for (i = 0; i < SMALL_FRAG_COUNT; ++i) {
	fptr->next = small_free_list;
	fptr->size = SMALL_FRAG_SIZE;
	small_free_list = fptr;
	fptr = (fragment_hdr_t *) (fptr->data + SMALL_FRAG_PAYLOAD);
    }
    /* and finally, initialize the large fragment free-list */
    for (i = 0; i < LARGE_FRAG_COUNT; ++i) {
	fptr->next = large_free_list;
	fptr->size = LARGE_FRAG_SIZE;
	large_free_list = fptr;
	fptr = (fragment_hdr_t *) (fptr->data + LARGE_FRAG_PAYLOAD);
    }
}

void INTERNAL PtlInternalFragmentTeardown(
    void)
{
}

/* this pulls a fragment off the free-list(s) big enough to hold the data.
 * Potential data sizes are:
 * <= 112 bytes == a 128 byte fragment
 * > 112 bytes == a 4096 byte fragment
 *
 */
void INTERNAL *PtlInternalFragmentFetch(
    size_t payload_size)
{
#warning Need to do something intelligent if/when we have no more fragments in one of the two lists.
    fragment_hdr_t *oldv, *newv, *retv;
    if (payload_size <= SMALL_FRAG_PAYLOAD) {
	assert(small_free_list != NULL);
	retv = small_free_list;
	do {
	    oldv = retv;
	    if (retv != NULL) {
		newv = retv->next;
	    }
	    retv = PtlInternalAtomicCasPtr(&small_free_list, oldv, newv);
	    assert(retv != NULL);
	} while (retv != oldv);
    } else {
	assert(large_free_list != NULL);
	retv = large_free_list;
	do {
	    oldv = retv;
	    if (retv != NULL) {
		newv = retv->next;
	    }
	    retv = PtlInternalAtomicCasPtr(&large_free_list, oldv, newv);
	    assert(retv != NULL);
	} while (retv != oldv);
    }
    return retv->data;
}

/* Fragment queueing uses the NEMESIS lock-free queue protocol from
 * http://www.mcs.anl.gov/~buntinas/papers/ccgrid06-nemesis.pdf
 * Note: it is NOT SAFE to use with multiple de-queuers, it is ONLY safe to use
 * with multiple enqueuers and a single de-queuer. */
static void PtlInternalNEMESISEnqueue(
    NEMESIS_queue * q,
    fragment_hdr_t * f)
{
    fragment_hdr_t *prev =
	PtlInternalAtomicSwapPtr((void *volatile *)&(q->tail), f);
    if (prev == NULL) {
	q->head = f;
    } else {
	prev->next = f;
    }
}

static fragment_hdr_t *PtlInternalNEMESISDequeue(
    NEMESIS_queue * q)
{
    fragment_hdr_t *retval = q->head;
    if (retval->next != NULL) {
	q->head = retval->next;
    } else {
	fragment_hdr_t *old;
	q->head = NULL;
	old = PtlInternalAtomicCasPtr(&(q->tail), retval, NULL);
	if (old != retval) {
	    while (retval->next == NULL) ;
	    q->head = retval->next;
	}
    }
    return retval;
}

/* this enqueues a fragment in the specified receive queue */
void INTERNAL PtlInternalFragmentToss(
    void *frag,
    ptl_pid_t dest)
{
    NEMESIS_queue *destQ =
	(NEMESIS_queue *) (comm_pad + firstpagesize +
			   (per_proc_comm_buf_size * dest));
    PtlInternalNEMESISEnqueue(destQ, frag);
}

/* this enqueues a fragment in the specified ack queue */
void INTERNAL PtlInternalFragmentAck(
    void *frag,
    ptl_pid_t dest)
{
    NEMESIS_queue *destQ =
	(NEMESIS_queue *) (comm_pad + firstpagesize +
			   (per_proc_comm_buf_size * dest) +
			   sizeof(NEMESIS_queue));
    PtlInternalNEMESISEnqueue(destQ, frag);
}

/* this dequeues a fragment from my receive queue */
void INTERNAL *PtlInternalFragmentReceive(
    void)
{
    fprintf(stderr, "fragment receive unimplemented\n");
    abort();
    return NULL;
}

/* this dequeues a fragment from my ack queue */
void INTERNAL *PtlInternalFragmentAckReceive(
    void)
{
    fprintf(stderr, "fragment ack receive unimplemented\n");
    abort();
    return NULL;
}

size_t INTERNAL PtlInternalFragmentSize(
    void *frag)
{
    return *(((uint64_t *) frag) - 1);
}
