#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <portals4.h>

/* System headers */
#include <stdlib.h>                    /* for size_t */
#include <string.h>                    /* for memset() */

#ifdef PARANOID
# include <stdio.h>
#endif

#include <sys/mman.h>  /* for mprotect */
#include <unistd.h>  /* for getpagesize */

/* Internal headers */
#include "ptl_internal_assert.h"
#include "ptl_internal_fragments.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nemesis.h"
#include "ptl_visibility.h"

/* Fragment format:
 *
 * 8 bytes - Next ptr
 * 8 bytes - Size (256 or 4096 at the moment)
 * 8 optional bytes - owner (for debugging)
 * X bytes - the rest
 */

typedef struct fragment_hdr_s fragment_hdr_t;

struct fragment_hdr_s {
    fragment_hdr_t *    next;             // for NEMESIS_entry
    uint64_t            size;
#ifdef PARANOID
    uint64_t owner_rank;
#endif
    char data[];
};

size_t SMALL_FRAG_SIZE    = 256;
size_t SMALL_FRAG_PAYLOAD = 0;
size_t SMALL_FRAG_COUNT   = 512;
size_t LARGE_FRAG_PAYLOAD = 0;
size_t LARGE_FRAG_SIZE    = 4096;
size_t LARGE_FRAG_COUNT   = 128;

static fragment_hdr_t *small_free_list  = NULL;
static fragment_hdr_t *large_free_list  = NULL;
static NEMESIS_blocking_queue *receiveQ = NULL;

#define C_VALIDPTR(x) assert(((uintptr_t)(x)) >= (uintptr_t)comm_pad && \
                             ((uintptr_t)(x)) < \
                             ((uintptr_t)comm_pad + per_proc_comm_buf_size * \
                              (num_siblings + 1)))
#ifdef PARANOID
static uintptr_t small_bufstart, small_bufend;
static uintptr_t large_bufstart, large_bufend;
# define PARANOID_STEP(x)       x
# define VALIDPTR(x, t)         assert(((uintptr_t)(x)) >= t ## _bufstart && \
                                       ((uintptr_t)(x)) < t ## _bufend)
static void PtlInternalValidateFragmentLists(
                                             void)
{                                      /*{{{ */
    unsigned long count = 0;
    fragment_hdr_t *cursor = small_free_list;
    fragment_hdr_t *prev = NULL;

    while (cursor != NULL) {
        count++;
        VALIDPTR(cursor, small);
        if (cursor->size != SMALL_FRAG_PAYLOAD) {
            fprintf(
                    stderr,
                    "problem in small free list: item %lu size is %lu, rather than %lu, prev=%p\n",
                    count, (unsigned long)cursor->size, SMALL_FRAG_PAYLOAD,
                    prev);
        }
        assert(cursor->size == SMALL_FRAG_PAYLOAD);
        prev = cursor;
        cursor = cursor->next;
    }
    cursor = large_free_list;
    count = 0;
    prev = NULL;
    while (cursor != NULL) {
        count++;
        VALIDPTR(cursor, large);
        if (cursor->size != LARGE_FRAG_PAYLOAD) {
            fprintf(
                    stderr,
                    "problem in large free list: item %lu size is %lu, rather than %lu, prev=%p\n",
                    count, (unsigned long)cursor->size, LARGE_FRAG_PAYLOAD,
                    prev);
        }
        assert(cursor->size == LARGE_FRAG_PAYLOAD);
        prev = cursor;
        cursor = cursor->next;
    }
}                                      /*}}} */

#else /* ifdef PARANOID */
# define PARANOID_STEP(x)
# define PtlInternalValidateFragmentLists()
#endif /* ifdef PARANOID */

void INTERNAL PtlInternalFragmentSetup(volatile char *buf)
{                                      /*{{{ */
    size_t i;
    fragment_hdr_t *fptr;
    char *bptr;

    /* init metadata */
    SMALL_FRAG_PAYLOAD = SMALL_FRAG_SIZE - sizeof(fragment_hdr_t);
    LARGE_FRAG_PAYLOAD = LARGE_FRAG_SIZE - sizeof(fragment_hdr_t);
    /* first, initialize the receive queue */
    receiveQ = (NEMESIS_blocking_queue *)buf;
    PtlInternalNEMESISBlockingInit(receiveQ);
    // printf("%i(%u)==========> receiveQ (%p) initialized\n", (int)getpid(), (unsigned)proc_number, receiveQ);
    /* now, initialize the small fragment free-list */
    fptr = (fragment_hdr_t *)(buf + sizeof(NEMESIS_blocking_queue));
    bptr = (char*)fptr;
    PARANOID_STEP(small_bufstart = (uintptr_t)fptr);
    for (i = 0; i < SMALL_FRAG_COUNT; ++i) {
        fptr->next = small_free_list;
        fptr->size = SMALL_FRAG_PAYLOAD;
        PARANOID_STEP(fptr->owner_rank = proc_number);
        small_free_list = fptr;
        fptr = (fragment_hdr_t *)(bptr + (SMALL_FRAG_SIZE * (i + 1)));
        // fptr = (fragment_hdr_t *) (fptr->data + SMALL_FRAG_PAYLOAD);
    }
    /* and finally, initialize the large fragment free-list */
    PARANOID_STEP(large_bufstart = small_bufend = (uintptr_t)fptr);
    for (i = 0; i < LARGE_FRAG_COUNT; ++i) {
        fptr->next = large_free_list;
        fptr->size = LARGE_FRAG_PAYLOAD;
        PARANOID_STEP(fptr->owner_rank = proc_number);
        large_free_list = fptr;
        fptr = (fragment_hdr_t *)(fptr->data + LARGE_FRAG_PAYLOAD);
    }
    PARANOID_STEP(large_bufend = (uintptr_t)fptr);
    PtlInternalValidateFragmentLists();
}                                      /*}}} */

/* this pulls a fragment off the free-list(s) big enough to hold the data. */
void INTERNAL *PtlInternalFragmentFetch(size_t payload_size)
{                                      /*{{{ */
    fragment_hdr_t *oldv, *newv, *retv;

    PtlInternalValidateFragmentLists();
    if (payload_size <= SMALL_FRAG_PAYLOAD) {
        retv = small_free_list;
        do {
            oldv = retv;
            if (retv != NULL) {
                newv = retv->next;
            } else {
                newv = NULL;
            }
            retv = PtlInternalAtomicCasPtr(&small_free_list, oldv, newv);
        } while (retv != oldv || retv == NULL /* perhaps should yield? */ );
        PARANOID_STEP(memset(retv->data, 0x77, SMALL_FRAG_PAYLOAD));
    } else {
        retv = large_free_list;
        do {
            oldv = retv;
            if (retv != NULL) {
                newv = retv->next;
            } else {
                newv = NULL;
            }
            retv = PtlInternalAtomicCasPtr(&large_free_list, oldv, newv);
        } while (retv != oldv || retv == NULL /* perhaps should yield? */ );
        PARANOID_STEP(memset(retv->data, 0x77, LARGE_FRAG_PAYLOAD));
    }
    retv->next = NULL;
    PtlInternalValidateFragmentLists();
    return retv->data;
}                                      /*}}} */

/* this enqueues a fragment in the specified receive queue */
void INTERNAL PtlInternalFragmentToss(void *frag,
                                      ptl_pid_t dest)
{                                      /*{{{ */
    NEMESIS_blocking_queue *destQ =
        (NEMESIS_blocking_queue *)(comm_pad + firstpagesize +
                                   (per_proc_comm_buf_size * dest));

    PtlInternalValidateFragmentLists();
    frag = ((fragment_hdr_t *)frag) - 1;
    C_VALIDPTR(frag);
    PtlInternalNEMESISBlockingOffsetEnqueue(destQ, (NEMESIS_entry *)frag);
}                                      /*}}} */

/* this dequeues a fragment from my receive queue */
void INTERNAL *PtlInternalFragmentReceive(void)
{                                      /*{{{ */
    fragment_hdr_t *frag =
        (fragment_hdr_t *)PtlInternalNEMESISBlockingOffsetDequeue(receiveQ);

    PtlInternalValidateFragmentLists();
    assert(frag->next == NULL);
    C_VALIDPTR(frag);
    return frag->data;
}                                      /*}}} */

uint64_t INTERNAL PtlInternalFragmentSize(void *frag)
{                                      /*{{{ */
    PtlInternalValidateFragmentLists();
    return (((fragment_hdr_t *)frag) - 1)->size;
}                                      /*}}} */

void INTERNAL PtlInternalFragmentFree(void *data)
{                                      /*{{{ */
    fragment_hdr_t *frag = (((fragment_hdr_t*)data) - 1);

    assert(frag->next == NULL);
    assert((uintptr_t)frag > (uintptr_t)comm_pad);
    PtlInternalValidateFragmentLists();
    if (frag->size == SMALL_FRAG_PAYLOAD) {
        void *oldv, *newv, *tmpv;
        PARANOID_STEP(VALIDPTR(frag, small));
        tmpv = small_free_list;
        do {
            oldv = frag->next = tmpv;
            newv = frag;
            tmpv = PtlInternalAtomicCasPtr(&small_free_list, oldv, newv);
        } while (tmpv != oldv);
    } else if (frag->size == LARGE_FRAG_PAYLOAD) {
        void *oldv, *newv, *tmpv;
        PARANOID_STEP(VALIDPTR(frag, large));
        tmpv = large_free_list;
        do {
            oldv = frag->next = tmpv;
            newv = frag;
            tmpv = PtlInternalAtomicCasPtr(&large_free_list, oldv, newv);
        } while (tmpv != oldv);
    } else {
        abort();
        *(int *)0 = 0;
    }
    PtlInternalValidateFragmentLists();
}                                      /*}}} */

/* vim:set expandtab: */
