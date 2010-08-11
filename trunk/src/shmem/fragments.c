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
#include "ptl_internal_nemesis.h"
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
    void *next;			// for NEMESIS_entry
    uint64_t size;
    char data[];
} fragment_hdr_t;

static fragment_hdr_t *small_free_list = NULL;
static fragment_hdr_t *large_free_list = NULL;
static NEMESIS_blocking_queue *receiveQ = NULL;
static NEMESIS_blocking_queue *ackQ = NULL;

void INTERNAL PtlInternalFragmentSetup(
    volatile char *buf)
{
    size_t i;
    fragment_hdr_t *fptr;

    /* init metadata */
    SMALL_FRAG_PAYLOAD = SMALL_FRAG_SIZE - sizeof(void *) - sizeof(uint64_t);
    LARGE_FRAG_PAYLOAD = LARGE_FRAG_SIZE - sizeof(void *) - sizeof(uint64_t);
    /* first, initialize the receive queue */
    receiveQ = (NEMESIS_blocking_queue *) buf;
    PtlInternalNEMESISBlockingInit(receiveQ);
    /* next, initialize the ack queue */
    ackQ = receiveQ + 1;
    PtlInternalNEMESISBlockingInit(ackQ);
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
    PtlInternalNEMESISBlockingDestroy(receiveQ);
    PtlInternalNEMESISBlockingDestroy(ackQ);
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
    retv->next = NULL;
    return retv->data;
}

/* this enqueues a fragment in the specified receive queue */
void INTERNAL PtlInternalFragmentToss(
    void *frag,
    ptl_pid_t dest)
{
    NEMESIS_blocking_queue *destQ =
	(NEMESIS_blocking_queue *) (comm_pad + firstpagesize +
			   (per_proc_comm_buf_size * dest));
    frag = ((uint64_t *) frag) - 2;
    PtlInternalNEMESISBlockingOffsetEnqueue(destQ, (NEMESIS_entry *) frag);
}

/* this enqueues a fragment in the specified ack queue */
void INTERNAL PtlInternalFragmentAck(
    void *frag,
    ptl_pid_t dest)
{
    NEMESIS_blocking_queue *destQ =
	(NEMESIS_blocking_queue *) (comm_pad + firstpagesize +
				    (per_proc_comm_buf_size * dest) +
				    sizeof(NEMESIS_queue));
    frag = ((uint64_t *) frag) - 2;
    PtlInternalNEMESISBlockingOffsetEnqueue(destQ, (NEMESIS_entry *) frag);
}

/* this dequeues a fragment from my receive queue */
void INTERNAL *PtlInternalFragmentReceive(
    void)
{
    fragment_hdr_t *frag =
	(fragment_hdr_t *) PtlInternalNEMESISBlockingOffsetDequeue(receiveQ);
    return frag->data;
}

/* this dequeues a fragment from my ack queue */
void INTERNAL *PtlInternalFragmentAckReceive(
    void)
{
    fragment_hdr_t *frag =
	(fragment_hdr_t *) PtlInternalNEMESISBlockingOffsetDequeue(ackQ);
    return frag->data;
}

uint64_t INTERNAL PtlInternalFragmentSize(
    void *frag)
{
    return *(((uint64_t *) frag) - 1);
}

void INTERNAL PtlInternalFragmentFree(
    void *data)
{
    fragment_hdr_t *frag = (fragment_hdr_t *) (((uint64_t *) data) - 2);
    assert(frag->next == NULL);
    if (frag->size == SMALL_FRAG_SIZE) {
	frag->next = small_free_list;
	small_free_list = frag;
    } else {
	frag->next = large_free_list;
	large_free_list = frag;
    }
}
