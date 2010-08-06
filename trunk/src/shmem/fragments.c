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

static fragment_hdr_t *small_free_list = NULL;
static fragment_hdr_t *large_free_list = NULL;

void INTERNAL PtlInternalFragmentSetup(
    volatile char *buf)
{
    size_t i;
    fragment_hdr_t *ptr;
    SMALL_FRAG_PAYLOAD = SMALL_FRAG_SIZE - sizeof(void *) - sizeof(uint64_t);
    LARGE_FRAG_PAYLOAD = LARGE_FRAG_SIZE - sizeof(void *) - sizeof(uint64_t);

    ptr = (fragment_hdr_t *) buf;
    for (i = 0; i < SMALL_FRAG_COUNT; ++i) {
	ptr->next = small_free_list;
	ptr->size = SMALL_FRAG_SIZE;
	small_free_list = ptr;
	ptr = (fragment_hdr_t *) (((char *)ptr) + SMALL_FRAG_SIZE);
    }
    for (i = 0; i < LARGE_FRAG_COUNT; ++i) {
	ptr->next = large_free_list;
	ptr->size = LARGE_FRAG_SIZE;
	large_free_list = ptr;
	ptr = (fragment_hdr_t *) (((char *)ptr) + LARGE_FRAG_SIZE);
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

/* this enqueues a fragment in the specified receive queue */
void INTERNAL PtlInternalFragmentToss(
    void *frag,
    ptl_pid_t dest)
{
    fprintf(stderr, "fragment toss unimplemented\n");
    abort();
}

/* this enqueues a fragment in the specified ack queue */
void INTERNAL PtlInternalFragmentAck(
    void *frag,
    ptl_pid_t dest)
{
    fprintf(stderr, "fragment ack unimplemented\n");
    abort();
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
