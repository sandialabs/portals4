/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <string.h>                    /* for memcpy() */

#include <stdio.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_LE.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_PT.h"
#include "ptl_internal_error.h"
#include "ptl_internal_performatomic.h"
#include "ptl_internal_papi.h"
#include "ptl_internal_fragments.h"

#define LE_FREE         0
#define LE_ALLOCATED    1
#define LE_IN_USE       2

typedef struct {
    void *next;
    void *user_ptr;
    ptl_internal_handle_converter_t le_handle;
} ptl_internal_appendLE_t;

typedef struct {
    ptl_internal_appendLE_t Qentry;
    ptl_le_t visible;
    volatile uint32_t status;   // 0=free, 1=allocated, 2=in-use
    ptl_pt_index_t pt_index;
    ptl_list_t ptl_list;
} ptl_internal_le_t;

static ptl_internal_le_t *les[4] = { NULL, NULL, NULL, NULL };

/* Static functions */
static void PtlInternalPerformDelivery(
    const unsigned char type,
    void *const restrict src,
    void *const restrict dest,
    size_t nbytes,
    ptl_internal_header_t * hdr);
static void PtlInternalAnnounceLEDelivery(
    const ptl_handle_eq_t eq_handle,
    const ptl_handle_ct_t ct_handle,
    const unsigned char type,
    const unsigned int options,
    const uint64_t mlength,
    const uintptr_t start,
    const int overflow,
    void *const user_ptr,
    ptl_internal_header_t * const restrict hdr);

void INTERNAL PtlInternalLENISetup(
    unsigned int ni,
    ptl_size_t limit)
{                                      /*{{{ */
    ptl_internal_le_t *tmp;
    while ((tmp =
            PtlInternalAtomicCasPtr(&(les[ni]), NULL,
                                    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
        tmp = calloc(limit, sizeof(ptl_internal_le_t));
        assert(tmp != NULL);
        __sync_synchronize();
        les[ni] = tmp;
    }
}                                      /*}}} */

void INTERNAL PtlInternalLENITeardown(
    unsigned int ni)
{                                      /*{{{ */
    ptl_internal_le_t *tmp;
    tmp = les[ni];
    les[ni] = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    free(tmp);
}                                      /*}}} */

#define PTL_INTERNAL_INIT_TEVENT(e,hdr,uptr) do { \
    e.pt_index = hdr->pt_index; \
    e.uid = 0; \
    e.jid = PTL_JID_NONE; \
    e.match_bits = 0; \
    e.rlength = hdr->length; \
    e.mlength = 0; \
    e.remote_offset = hdr->dest_offset; \
    e.user_ptr = uptr; \
    e.ni_fail_type = PTL_NI_OK; \
    if (hdr->ni <= 1) { /* Logical */ \
        e.initiator.rank = hdr->src; \
    } else { /* Physical */ \
        e.initiator.phys.pid = hdr->src; \
        e.initiator.phys.nid = 0; \
    } \
    switch (hdr->type) { \
        case HDR_TYPE_PUT: e.type = PTL_EVENT_PUT; \
            e.hdr_data = hdr->info.put.hdr_data; \
            break; \
        case HDR_TYPE_ATOMIC: e.type = PTL_EVENT_ATOMIC; \
            e.hdr_data = hdr->info.atomic.hdr_data; \
            break; \
        case HDR_TYPE_FETCHATOMIC: e.type = PTL_EVENT_ATOMIC; \
            e.hdr_data = hdr->info.fetchatomic.hdr_data; \
            break; \
        case HDR_TYPE_SWAP: e.type = PTL_EVENT_ATOMIC; \
            e.hdr_data = hdr->info.swap.hdr_data; \
            break; \
        case HDR_TYPE_GET: e.type = PTL_EVENT_GET; \
            e.hdr_data = 0; \
            break; \
    } \
} while (0)

int API_FUNC PtlLEAppend(
    ptl_handle_ni_t ni_handle,
    ptl_pt_index_t pt_index,
    ptl_le_t * le,
    ptl_list_t ptl_list,
    void *user_ptr,
    ptl_handle_le_t * le_handle)
{                                      /*{{{ */
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t leh = {.s.selector = HANDLE_LE_CODE };
    ptl_internal_appendLE_t *Qentry = NULL;
    ptl_table_entry_t *t;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (ni.s.ni >= 4 || ni.s.code != 0 || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if (ni.s.ni == 0 || ni.s.ni == 2) { // must be a non-matching NI
        VERBOSE_ERROR("must be a non-matching NI\n");
        return PTL_ARG_INVALID;
    }
    if (nit.tables[ni.s.ni] == NULL) { // this should never happen
        assert(nit.tables[ni.s.ni] != NULL);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[ni.s.ni].max_pt_index) {
        VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
                      nit_limits[ni.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    {
        int ptv = PtlInternalPTValidate(&nit.tables[ni.s.ni][pt_index]);
        if (ptv == 1 || ptv == 3) {    // Unallocated or bad EQ (enabled/disabled both allowed)
            VERBOSE_ERROR("LEAppend sees an invalid PT\n");
            return PTL_ARG_INVALID;
        }
    }
#endif
    assert(les[ni.s.ni] != NULL);
    leh.s.ni = ni.s.ni;
    /* find an LE handle */
    for (uint32_t offset = 0; offset < nit_limits[ni.s.ni].max_entries; ++offset) {
        if (les[ni.s.ni][offset].status == 0) {
            if (PtlInternalAtomicCas32
                (&(les[ni.s.ni][offset].status), LE_FREE,
                 LE_ALLOCATED) == LE_FREE) {
                leh.s.code = offset;
                les[ni.s.ni][offset].visible = *le;
                les[ni.s.ni][offset].pt_index = pt_index;
                les[ni.s.ni][offset].ptl_list = ptl_list;
                Qentry = &(les[ni.s.ni][offset].Qentry);
                break;
            }
        }
    }
    if (Qentry == NULL) {
        return PTL_FAIL;
    }
    Qentry->user_ptr = user_ptr;
    Qentry->le_handle = leh;
    *le_handle = leh.a;
    /* append to associated list */
    assert(nit.tables[ni.s.ni] != NULL);
    t = &(nit.tables[ni.s.ni][pt_index]);
    ptl_assert(pthread_mutex_lock(&t->lock), 0);
    switch (ptl_list) {
        case PTL_PRIORITY_LIST:
            if (t->buffered_headers.head != NULL) {     // implies that overflow.head != NULL
                /* If there are buffered headers, then they get first priority on matching this priority append. */
                ptl_internal_buffered_header_t *cur =
                    (ptl_internal_buffered_header_t *) (t->
                                                        buffered_headers.head);
                ptl_internal_buffered_header_t *prev = NULL;
                for (; cur != NULL; prev = cur, cur = cur->hdr.next) {
                    /* act like there was a delivery;
                     * 1. Dequeue header
                     * 2. Check permissions
                     * 3. Iff LE is persistent...
                     * 4a. Queue buffered header to ME buffer
                     * 5a. When done processing entire unexpected header list, send retransmit request
                     * ... else: deliver and return */
                    // dequeue header
                    if (prev != NULL) {
                        prev->hdr.next = cur->hdr.next;
                    } else {
                        t->buffered_headers.head = cur->hdr.next;
                    }
                    // (1) check permissions
                    if (le->options & PTL_LE_AUTH_USE_JID) {
                        if (le->ac_id.jid != PTL_JID_ANY) {
                            goto permission_violation;
                        }
                    } else {
                        if (le->ac_id.uid != PTL_UID_ANY) {
                            goto permission_violation;
                        }
                    }
                    switch (cur->hdr.type) {
                        case HDR_TYPE_PUT:
                        case HDR_TYPE_ATOMIC:
                        case HDR_TYPE_FETCHATOMIC:
                        case HDR_TYPE_SWAP:
                            if ((le->options & PTL_LE_OP_PUT) == 0) {
                                goto permission_violation;
                            }
                    }
                    switch (cur->hdr.type) {
                        case HDR_TYPE_GET:
                        case HDR_TYPE_FETCHATOMIC:
                        case HDR_TYPE_SWAP:
                            if ((le->options & PTL_LE_OP_GET) == 0) {
                                goto permission_violation;
                            }
                    }
                    if (0) {
                        ptl_internal_buffered_header_t *tmp;
                      permission_violation:
                        (void)PtlInternalAtomicInc(&nit.regs[cur->hdr.ni]
                                                   [PTL_SR_PERMISSIONS_VIOLATIONS], 1);
                        tmp = cur;
                        prev->hdr.next = cur->hdr.next;
                        cur = prev;
                        PtlInternalDeallocUnexpectedHeader(tmp);
                        continue;
                    }
                    // (2) iff LE is persistent
                    if ((le->options & PTL_LE_USE_ONCE) == 0) {
                        fprintf(stderr,
                                "PORTALS4-> PtlLEAppend() does not work with persistent LEs and buffered headers (implementation needs to be fleshed out)\n");
                        /* suggested plan: put an LE-specific buffered header
                         * list on each LE, and when the LE is persistent, it
                         * gets the buffered headers that it matched, in order.
                         * Then, this list can be used to start reworking (e.g.
                         * retransmitting/restarting) the original order of
                         * deliveries. While this list exists on the LE, new
                         * packets get added to that list. Once the list is
                         * empty, the LE becomes a normal persistent LE. */
                        abort();
                        // Queue buffered header to LE buffer
                        // etc.
                    } else {
                        size_t mlength;
                        // deliver
                        if (le->length == 0) {
                            mlength = 0;
                        } else if (cur->hdr.length + cur->hdr.dest_offset >
                                   le->length) {
                            if (le->length > cur->hdr.dest_offset) {
                                mlength = le->length - cur->hdr.dest_offset;
                            } else {
                                mlength = 0;
                            }
                        } else {
                            mlength = cur->hdr.length;
                        }
#ifndef ALWAYS_TRIGGER_OVERFLOW_EVENTS
                        if (cur->buffered_data != NULL) {
                            PtlInternalPerformDelivery(cur->hdr.type,
                                                       (char *)le->start +
                                                       cur->hdr.dest_offset,
                                                       cur->buffered_data,
                                                       mlength, &(cur->hdr));
                            // notify
                            if (t->EQ != PTL_EQ_NONE ||
                                le->ct_handle != PTL_CT_NONE) {
                                PtlInternalAnnounceLEDelivery(t->EQ,
                                                              le->ct_handle,
                                                              cur->hdr.type,
                                                              le->options,
                                                              mlength,
                                                              (uintptr_t)
                                                              le->start +
                                                              cur->
                                                              hdr.dest_offset,
                                                              0, user_ptr,
                                                              &(cur->hdr));
                            }
                        } else {
                            /* Cannot deliver buffered messages without local data; so just emit the OVERFLOW event */
                            if (t->EQ != PTL_EQ_NONE ||
                                le->ct_handle != PTL_CT_NONE) {
                                PtlInternalAnnounceLEDelivery(t->EQ,
                                                              le->ct_handle,
                                                              cur->hdr.type,
                                                              le->options,
                                                              mlength,
                                                              (uintptr_t) 0,
                                                              1, user_ptr,
                                                              &(cur->hdr));
                            }
                        }
#else
                        if (t->EQ != PTL_EQ_NONE ||
                            le->ct_handle != PTL_CT_NONE) {
                            PtlInternalAnnounceLEDelivery(t->EQ,
                                                          le->ct_handle,
                                                          cur->hdr.type,
                                                          le->options,
                                                          mlength, (uintptr_t)
                                                          cur->buffered_data,
                                                          1, user_ptr,
                                                          &(cur->hdr));
                        }
#endif
                        // return
                        PtlInternalDeallocUnexpectedHeader(cur);
                        goto done_appending;
                    }
                }
                /* either nothing matched in the buffered_headers, or something
                 * did but we're appending a persistent LE, so go on and append
                 * to the priority list. */
            }
            if (t->priority.tail == NULL) {
                t->priority.head = Qentry;
            } else {
                ((ptl_internal_appendLE_t *) (t->priority.tail))->next =
                    Qentry;
            }
            t->priority.tail = Qentry;
            break;
        case PTL_OVERFLOW:
            if (t->overflow.tail == NULL) {
                t->overflow.head = Qentry;
            } else {
                ((ptl_internal_appendLE_t *) (t->overflow.tail))->next =
                    Qentry;
            }
            t->overflow.tail = Qentry;
            break;
        case PTL_PROBE_ONLY:
            if (t->buffered_headers.head != NULL) {
                ptl_internal_buffered_header_t *cur =
                    (ptl_internal_buffered_header_t *) (t->
                                                        buffered_headers.head);
                for (; cur != NULL; cur = cur->hdr.next) {
                    /* act like there was a delivery;
                     * 1. Check permissions
                     * 2. Iff LE is persistent...
                     * 3a. Queue buffered header to LE buffer
                     * 4a. When done processing entire unexpected header list, send retransmit request
                     * ... else: deliver and return */
                    // (1) check permissions
                    if (le->options & PTL_LE_AUTH_USE_JID) {
                        if (le->ac_id.jid != PTL_JID_ANY) {
                            goto permission_violationPO;
                        }
                    } else {
                        if (le->ac_id.uid != PTL_UID_ANY) {
                            goto permission_violationPO;
                        }
                    }
                    switch (cur->hdr.type) {
                        case HDR_TYPE_PUT:
                        case HDR_TYPE_ATOMIC:
                        case HDR_TYPE_FETCHATOMIC:
                        case HDR_TYPE_SWAP:
                            if ((le->options & PTL_LE_OP_PUT) == 0) {
                                goto permission_violationPO;
                            }
                    }
                    switch (cur->hdr.type) {
                        case HDR_TYPE_GET:
                        case HDR_TYPE_FETCHATOMIC:
                        case HDR_TYPE_SWAP:
                            if ((le->options & PTL_LE_OP_GET) == 0) {
                                goto permission_violationPO;
                            }
                    }
                    if (0) {
                      permission_violationPO:
                        (void)PtlInternalAtomicInc(&nit.regs[cur->hdr.ni]
                                                   [PTL_SR_PERMISSIONS_VIOLATIONS], 1);
                        continue;
                    }
                    {
                        size_t mlength;
                        // deliver
                        if (le->length == 0) {
                            mlength = 0;
                        } else if (cur->hdr.length + cur->hdr.dest_offset >
                                   le->length) {
                            if (le->length > cur->hdr.dest_offset) {
                                mlength = le->length - cur->hdr.dest_offset;
                            } else {
                                mlength = 0;
                            }
                        } else {
                            mlength = cur->hdr.length;
                        }
                        // notify
                        if (t->EQ != PTL_EQ_NONE) {
                            ptl_event_t e;
                            PTL_INTERNAL_INIT_TEVENT(e, (&(cur->hdr)),
                                                     user_ptr);
                            e.type = PTL_EVENT_PROBE;
                            e.mlength = mlength;
                            e.start = cur->buffered_data;
                            PtlInternalEQPush(t->EQ, &e);
                        }
                    }
                    // (2) iff LE is NOT persistent
                    if (le->options & PTL_LE_USE_ONCE) {
                        goto done_appending;
                    }
                }
            }
            break;
    }
  done_appending:
    ptl_assert(pthread_mutex_unlock(&t->lock), 0);
    return PTL_OK;
}                                      /*}}} */

int API_FUNC PtlLEUnlink(
    ptl_handle_le_t le_handle)
{                                      /*{{{ */
    const ptl_internal_handle_converter_t le = { le_handle };
    ptl_table_entry_t *restrict const t = &(nit.tables[le.s.ni][les[le.s.ni][le.s.code].pt_index]);
    const ptl_internal_appendLE_t *restrict const dq_target = &(les[le.s.ni][le.s.code].Qentry);
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if (le.s.ni > 3 || le.s.code > nit_limits[le.s.ni].max_entries ||
        nit.refcount[le.s.ni] == 0) {
        VERBOSE_ERROR
            ("LE Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n",
             le.s.ni, le.s.code, nit_limits[le.s.ni].max_entries);
        return PTL_ARG_INVALID;
    }
    if (les[le.s.ni] == NULL) {
        VERBOSE_ERROR("LE array uninitialized\n");
        return PTL_ARG_INVALID;
    }
    if (les[le.s.ni][le.s.code].status == LE_FREE) {
        VERBOSE_ERROR("LE appears to be free already\n");
        return PTL_ARG_INVALID;
    }
#endif
    ptl_assert(pthread_mutex_lock(&t->lock), 0);
    switch (les[le.s.ni][le.s.code].ptl_list) {
        case PTL_PRIORITY_LIST:
        {
            ptl_internal_appendLE_t *dq =
                (ptl_internal_appendLE_t *) (t->priority.head);
            if (dq == dq_target) {
                if (dq->next != NULL) {
                    t->priority.head = dq->next;
                } else {
                    t->priority.head = t->priority.tail = NULL;
                }
                dq->next = NULL;
            } else {
                ptl_internal_appendLE_t *prev = NULL;
                while (dq != dq_target && dq != NULL) {
                    prev = dq;
                    dq = dq->next;
                }
                if (dq == NULL) {
                    fprintf(stderr, "PORTALS4-> attempted to unlink an un-queued LE\n");
                    return PTL_FAIL;
                }
                prev->next = dq->next;
                if (dq->next == NULL) {
                    assert(t->priority.tail == dq);
                    t->priority.tail = prev;
                } else {
                    dq->next = NULL;
                }
            }
        }
            break;
        case PTL_OVERFLOW:
        {
            ptl_internal_appendLE_t *dq =
                (ptl_internal_appendLE_t *) (t->overflow.head);
            if (dq == &(les[le.s.ni][le.s.code].Qentry)) {
                if (dq->next != NULL) {
                    t->overflow.head = dq->next;
                } else {
                    t->overflow.head = t->overflow.tail = NULL;
                }
                dq->next = NULL;
            } else {
                ptl_internal_appendLE_t *prev = NULL;
                while (dq != &(les[le.s.ni][le.s.code].Qentry) && dq != NULL) {
                    prev = dq;
                    dq = dq->next;
                }
                if (dq == NULL) {
                    fprintf(stderr, "PORTALS4-> attempted to unlink an un-queued LE\n");
                    return PTL_FAIL;
                }
                prev->next = dq->next;
                if (dq->next == NULL) {
                    assert(t->overflow.tail == dq);
                    t->overflow.tail = prev;
                } else {
                    dq->next = NULL;
                }
            }
        }
            break;
        case PTL_PROBE_ONLY:
            fprintf(stderr, "PORTALS4-> how on earth did this happen?\n");
            abort();
            break;
    }
    ptl_assert(pthread_mutex_unlock(&t->lock), 0);
    switch (PtlInternalAtomicCas32
            (&(les[le.s.ni][le.s.code].status), LE_ALLOCATED, LE_FREE)) {
        case LE_IN_USE:
            return PTL_IN_USE;
        case LE_ALLOCATED:
            return PTL_OK;
#ifndef NO_ARG_VALIDATION
        case LE_FREE:
            VERBOSE_ERROR("LE unexpectedly became free");
            return PTL_ARG_INVALID;
#endif
    }
    return PTL_OK;
}                                      /*}}} */

ptl_pid_t INTERNAL PtlInternalLEDeliver(
    ptl_table_entry_t * restrict t,
    ptl_internal_header_t * restrict hdr)
{                                      /*{{{ */
    enum { PRIORITY, OVERFLOW } foundin = PRIORITY;
    ptl_internal_appendLE_t *entry = NULL;
    ptl_handle_eq_t tEQ = t->EQ;
    ptl_le_t le;
    ptl_size_t msg_mlength = 0, fragment_mlength = 0;
    char need_more_data = 0;
    char need_to_unlock = 1;    // to decide whether to unlock the table upon return or whether it was unlocked earlier

    PtlInternalPAPIStartC();
    assert(t);
    assert(hdr);
    if (hdr->src_data.entry == NULL) {
        /* Find an entry */
        if (t->priority.head) {
            entry = t->priority.head;
        } else if (t->overflow.head) {
            entry = t->overflow.head;
            foundin = OVERFLOW;
        }
        hdr->src_data.entry = entry;
    } else {
        entry = hdr->src_data.entry;
        le = *(ptl_le_t *) (((char *)entry) +
                           offsetof(ptl_internal_le_t, visible));
        goto check_lengths;
    }
    if (entry != NULL) {
        /*********************************************************
         * There is an LE present, and 'entry' points to it *
         *********************************************************/
        le = *(ptl_le_t *) (((char *)entry) +
                           offsetof(ptl_internal_le_t, visible));
        assert(les[hdr->ni][entry->le_handle.s.code].status != LE_FREE);
        // check the permissions on the LE
        if (le.options & PTL_LE_AUTH_USE_JID) {
            if (le.ac_id.jid != PTL_JID_ANY) {
                goto permission_violation;
            }
        } else {
            if (le.ac_id.uid != PTL_UID_ANY) {
                goto permission_violation;
            }
        }
        switch (hdr->type) {
            case HDR_TYPE_PUT:
            case HDR_TYPE_ATOMIC:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                if ((le.options & PTL_LE_OP_PUT) == 0) {
                    goto permission_violation;
                }
        }
        switch (hdr->type) {
            case HDR_TYPE_GET:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                if ((le.options & (PTL_LE_ACK_DISABLE | PTL_LE_OP_GET)) == 0) {
                    goto permission_violation;
                }
        }
        if (0) {
          permission_violation:
            (void)PtlInternalAtomicInc(&nit.regs[hdr->ni]
                                       [PTL_SR_PERMISSIONS_VIOLATIONS], 1);
            //PtlInternalPAPIDoneC(PTL_LE_PROCESS, 0);
            ptl_assert(pthread_mutex_unlock(&t->lock), 0);
            return (ptl_pid_t) 3;
        }
        /*******************************************************************
         * We have permissions on this LE, now check if it's a use-once LE *
         *******************************************************************/
        if (le.options & PTL_LE_USE_ONCE) {
            // unlink LE
            if (foundin == PRIORITY) {
                t->priority.head = entry->next;
                if (entry->next == NULL)
                    t->priority.tail = NULL;
            } else {
                t->overflow.head = entry->next;
                if (entry->next == NULL)
                    t->overflow.tail = NULL;
            }
            entry->next = NULL;
            /* now that the LE has been unlinked, we can unlock the portal
             * table, thus allowing appends on the PT while we do this delivery
             */
            need_to_unlock = 0;
            ptl_assert(pthread_mutex_unlock(&t->lock), 0);
            if (tEQ != PTL_EQ_NONE &&
                (le.options & (PTL_LE_EVENT_UNLINK_DISABLE)) == 0) {
                ptl_event_t e;
                PTL_INTERNAL_INIT_TEVENT(e, hdr, entry->user_ptr);
                e.type = PTL_EVENT_AUTO_UNLINK;
                e.start = (char *)le.start + hdr->dest_offset;
                PtlInternalEQPush(tEQ, &e);
            }
        }
        /* check lengths */
check_lengths:
        {
            const size_t max_payload = PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t);
            /* msg_mlength is the total number of bytes that will be modified by this message */
            /* fragment_mlength is the total number of bytes that will by modified by this fragment */
            if (hdr->length + hdr->dest_offset > le.length) {
                if (le.length > hdr->dest_offset) {
                    msg_mlength = le.length - hdr->dest_offset;
                } else {
                    msg_mlength = 0;
                }
            } else {
                msg_mlength = hdr->length;
            }
            if (max_payload >= msg_mlength) {
                /* the entire operation fits into a single fragment */
                fragment_mlength = msg_mlength;
                need_more_data = 0;
            } else {
                /* the operation requires multiple fragments */
                if (hdr->src_data.remaining > max_payload) {
                    /* this is NOT the last fragment */
                    fragment_mlength = max_payload;
                    need_more_data = 1;
                } else {
                    /* this IS the last fragment */
                    fragment_mlength = hdr->src_data.remaining;
                    need_more_data = 0;
                }
            }
        }
        /*************************
         * Perform the Operation *
         *************************/
        /*
         * msg_mlength is the total bytecount of the message
         * fragment_mlength is the total bytecount of this packet
         * src_data.remaining is the total bytecount that has not been transmitted yet
         * Thus, the offset from the beginning of the message that this fragment refers to is...
         * me.start + dest_offset + (msg_mlength - fragment_mlength - src_data.remaining)
         * >_____+--------####====+____<
         * |     |        |   |   |    `--> le.start + le.length
         * |     |        |   |   `-------> le.start + hdr->dest_offset + ( msg_mlength )
         * |     |        |   `-----------> le.start + hdr->dest_offset + ( msg_mlength - ( src_data.remaining - fragment_mlength ) )
         * |     |        `---------------> le.start + hdr->dest_offset + ( msg_mlength - src_data.remaining )
         * |     `------------------------> le.start + hdr->dest_offset
         * `------------------------------> le.start
         */
        void *report_this_start = (char *)le.start + hdr->dest_offset;
        void *effective_start;
        if (hdr->src_data.moredata) {
            effective_start = (char *)le.start + hdr->dest_offset + (msg_mlength - hdr->src_data.remaining);
        } else {
            effective_start = report_this_start;
        }
        if (foundin == PRIORITY) {
            if (fragment_mlength > 0) {
                PtlInternalPerformDelivery(hdr->type, effective_start,
                                           hdr->data, fragment_mlength, hdr);
            }
            if (need_more_data == 0) {
                PtlInternalAnnounceLEDelivery(tEQ, le.ct_handle, hdr->type,
                                          le.options, msg_mlength,
                                          (uintptr_t) report_this_start, 0,
                                          entry->user_ptr, hdr);
            }
        } else {
            if (fragment_mlength != msg_mlength) {
                fprintf(stderr, "multi-fragment (oversize) messages into the overflow list doesn't work\n");
                abort();
            }
            assert(hdr->length + hdr->dest_offset <= fragment_mlength);
            if (fragment_mlength > 0) {
                memcpy(report_this_start, hdr->data, fragment_mlength);
            } else {
                report_this_start = NULL;
            }
            PtlInternalPTBufferUnexpectedHeader(t, hdr, (uintptr_t)
                                                report_this_start);
        }
        switch (hdr->type) {
            case HDR_TYPE_PUT:
            case HDR_TYPE_ATOMIC:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                PtlInternalPAPIDoneC(PTL_LE_PROCESS, 0);
                if (need_to_unlock)
                    ptl_assert(pthread_mutex_unlock(&t->lock), 0);
                return (ptl_pid_t) ((le.options & PTL_LE_ACK_DISABLE) ? 0 :
                                    1);
            default:
                PtlInternalPAPIDoneC(PTL_ME_PROCESS, 0);
                if (need_to_unlock)
                    ptl_assert(pthread_mutex_unlock(&t->lock), 0);
                return (ptl_pid_t) 1;
        }
    }
#ifdef LOUD_DROPS
    fprintf(stderr,
            "PORTALS4-> Rank %u dropped a message from rank %u, no LEs posted on PT %u on NI %u\n",
            (unsigned)proc_number, (unsigned)hdr->src,
            (unsigned)hdr->pt_index, (unsigned)hdr->ni);
    fflush(stderr);
#endif
    // post dropped message event
    if (tEQ != PTL_EQ_NONE) {
        ptl_event_t e;
        PTL_INTERNAL_INIT_TEVENT(e, hdr, NULL);
        e.type = PTL_EVENT_DROPPED;
        e.start = NULL;
        PtlInternalEQPush(tEQ, &e);
    }
    (void)PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_DROP_COUNT], 1);
    PtlInternalPAPIDoneC(PTL_LE_PROCESS, 0);
    if (need_to_unlock)
        ptl_assert(pthread_mutex_unlock(&t->lock), 0);
    return 0;                          // silent ACK
}                                      /*}}} */

static void PtlInternalPerformDelivery(
    const unsigned char type,
    void *const restrict src,
    void *const restrict dest,
    size_t nbytes,
    ptl_internal_header_t * hdr)
{                                      /*{{{ */
    switch (type) {
        case HDR_TYPE_PUT:
            memcpy(src, dest, nbytes);
            break;
        case HDR_TYPE_ATOMIC:
            PtlInternalPerformAtomic(src, dest, nbytes,
                                     hdr->info.atomic.operation,
                                     hdr->info.atomic.datatype);
            break;
        case HDR_TYPE_FETCHATOMIC:
            PtlInternalPerformAtomic(src, dest, nbytes,
                                     hdr->info.fetchatomic.operation,
                                     hdr->info.fetchatomic.datatype);
            break;
        case HDR_TYPE_GET:
            memcpy(dest, src, nbytes);
            break;
        case HDR_TYPE_SWAP:
            PtlInternalPerformAtomicArg(src, ((char *)dest) + 8,
                                        *(uint64_t *) hdr->data, nbytes,
                                        hdr->info.swap.operation,
                                        hdr->info.swap.datatype);
            break;
        default:
            UNREACHABLE;
            *(int *)0 = 0;
    }
}                                      /*}}} */

static void PtlInternalAnnounceLEDelivery(
    const ptl_handle_eq_t eq_handle,
    const ptl_handle_ct_t ct_handle,
    const unsigned char type,
    const unsigned int options,
    const uint64_t mlength,
    const uintptr_t start,
    const int overflow,
    void *const user_ptr,
    ptl_internal_header_t * const restrict hdr)
{                                      /*{{{ */
    int ct_announce = ct_handle != PTL_CT_NONE;
    if (ct_announce != 0) {
        if (overflow) {
            ct_announce = options & PTL_LE_EVENT_CT_OVERFLOW;
        } else {
            ct_announce = options & PTL_LE_EVENT_CT_COMM;
        }
    }
    if (ct_announce != 0) {
        if ((options & PTL_LE_EVENT_CT_BYTES) == 0) {
            const ptl_ct_event_t cte = { 1, 0 };
            PtlCTInc(ct_handle, cte);
        } else {
            const ptl_ct_event_t cte = { mlength, 0 };
            PtlCTInc(ct_handle, cte);
        }
    }
    if (eq_handle != PTL_EQ_NONE &&
        (options & (PTL_LE_EVENT_COMM_DISABLE | PTL_LE_EVENT_SUCCESS_DISABLE))
        == 0) {
        ptl_event_t e;
        PTL_INTERNAL_INIT_TEVENT(e, hdr, user_ptr);
        if (overflow) {
            switch (type) {
                case HDR_TYPE_PUT:
                    e.type = PTL_EVENT_PUT_OVERFLOW;
                    break;
                case HDR_TYPE_ATOMIC:
                    e.type = PTL_EVENT_ATOMIC_OVERFLOW;
                    break;
                default:
                    UNREACHABLE;
                    *(int *)0 = 0;
            }
        }
        e.mlength = mlength;
        e.start = (void *)start;
        PtlInternalEQPush(eq_handle, &e);
    }
}                                      /*}}} */

/* vim:set expandtab: */
