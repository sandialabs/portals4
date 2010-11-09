/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <string.h>                    /* for memcpy() */

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_ME.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_error.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_performatomic.h"
#include "ptl_internal_papi.h"

#define ME_FREE         0
#define ME_ALLOCATED    1
#define ME_IN_USE       2

typedef struct {
    void *next;                 // for nemesis
    void *user_ptr;
    ptl_internal_handle_converter_t me_handle;
    size_t local_offset;
    ptl_match_bits_t dont_ignore_bits;
} ptl_internal_appendME_t;

typedef struct {
    ptl_internal_appendME_t Qentry;
    ptl_me_t visible;
    volatile uint32_t status;   // 0=free, 1=allocated, 2=in-use
    ptl_pt_index_t pt_index;
    ptl_list_t ptl_list;
} ptl_internal_me_t;

static ptl_internal_me_t *mes[4] = { NULL, NULL, NULL, NULL };

void INTERNAL PtlInternalMENISetup(
    unsigned int ni,
    ptl_size_t limit)
{
    ptl_internal_me_t *tmp;
    while ((tmp =
            PtlInternalAtomicCasPtr(&(mes[ni]), NULL,
                                    (void *)1)) == (void *)1) ;
    if (tmp == NULL) {
        tmp = calloc(limit, sizeof(ptl_internal_me_t));
        assert(tmp != NULL);
        __sync_synchronize();
        mes[ni] = tmp;
    }
}

void INTERNAL PtlInternalMENITeardown(
    unsigned int ni)
{
    ptl_internal_me_t *tmp;
    tmp = mes[ni];
    mes[ni] = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    free(tmp);
}

static void PtlInternalPerformDelivery(
    const unsigned char type,
    void *const restrict src,
    void *const restrict dest,
    size_t nbytes,
    ptl_internal_header_t * hdr)
{
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
}

static void *PtlInternalPerformOverflowDelivery(
    ptl_internal_appendME_t * const restrict Qentry,
    char *const restrict lstart,
    const ptl_size_t llength,
    const unsigned int loptions,
    const ptl_size_t mlength,
    ptl_internal_header_t * const restrict hdr)
{
    void *retval = NULL;
    if (loptions & PTL_ME_MANAGE_LOCAL) {
        assert(hdr->length + Qentry->local_offset <= llength);
        if (mlength > 0) {
            retval = lstart + Qentry->local_offset;
            memcpy(retval, hdr->data, mlength);
            Qentry->local_offset += mlength;
        }
    } else {
        assert(hdr->length + hdr->dest_offset <= llength);
        if (mlength > 0) {
            retval = lstart + hdr->dest_offset;
            memcpy(retval, hdr->data, mlength);
        }
    }
    return retval;
}

#define PTL_INTERNAL_INIT_TEVENT(e,hdr) do { \
    e.event.tevent.pt_index = hdr->pt_index; \
    e.event.tevent.uid = 0; \
    e.event.tevent.jid = PTL_JID_NONE; \
    e.event.tevent.match_bits = hdr->match_bits; \
    e.event.tevent.rlength = hdr->length; \
    e.event.tevent.mlength = 0; \
    e.event.tevent.remote_offset = hdr->dest_offset; \
    e.event.tevent.user_ptr = hdr->user_ptr; \
    e.event.tevent.ni_fail_type = PTL_NI_OK; \
    if (hdr->ni <= 1) {                /* Logical */ \
        e.event.tevent.initiator.rank = hdr->src; \
    } else {                           /* Physical */ \
        e.event.tevent.initiator.phys.pid = hdr->src; \
        e.event.tevent.initiator.phys.nid = 0; \
    } \
    switch (hdr->type) { \
        case HDR_TYPE_PUT: e.type = PTL_EVENT_PUT; \
            e.event.tevent.hdr_data = hdr->info.put.hdr_data; \
            break; \
        case HDR_TYPE_ATOMIC: e.type = PTL_EVENT_ATOMIC; \
            e.event.tevent.hdr_data = hdr->info.atomic.hdr_data; \
            break; \
        case HDR_TYPE_FETCHATOMIC: e.type = PTL_EVENT_ATOMIC; \
            e.event.tevent.hdr_data = hdr->info.fetchatomic.hdr_data; \
            break; \
        case HDR_TYPE_SWAP: e.type = PTL_EVENT_ATOMIC; \
            e.event.tevent.hdr_data = hdr->info.swap.hdr_data; \
            break; \
        case HDR_TYPE_GET: e.type = PTL_EVENT_GET; \
            e.event.tevent.hdr_data = 0; \
            break; \
    } \
} while (0)

static void PtlInternalAnnounceMEDelivery(
    const ptl_handle_eq_t eq_handle,
    const ptl_handle_ct_t ct_handle,
    const unsigned char type,
    const unsigned int options,
    const uint64_t mlength,
    const uintptr_t start,
    const int overflow,
    ptl_internal_header_t * const restrict hdr)
{
    int ct_announce = ct_handle != PTL_CT_NONE;
    if (ct_announce != 0) {
        if (overflow) {
            ct_announce = options & PTL_ME_EVENT_CT_OVERFLOW;
        } else {
            ct_announce = options & PTL_ME_EVENT_CT_COMM;
        }
    }
    if (ct_announce != 0) {
        if ((options & PTL_ME_EVENT_CT_BYTES) == 0) {
            const ptl_ct_event_t cte = { 1, 0 };
            PtlCTInc(ct_handle, cte);
        } else {
            const ptl_ct_event_t cte = { mlength, 0 };
            PtlCTInc(ct_handle, cte);
        }
    }
    if (eq_handle != PTL_EQ_NONE &&
        (options & (PTL_ME_EVENT_COMM_DISABLE | PTL_ME_EVENT_SUCCESS_DISABLE))
        == 0) {
        ptl_event_t e;
        PTL_INTERNAL_INIT_TEVENT(e, hdr);
        if (overflow) {
            switch (type) {
                case PTL_EVENT_PUT:
                    e.type = PTL_EVENT_PUT_OVERFLOW;
                    break;
                case PTL_EVENT_ATOMIC:
                    e.type = PTL_EVENT_ATOMIC_OVERFLOW;
                    break;
                default:
                    UNREACHABLE;
                    *(int *)0 = 0;
            }
        }
        e.event.tevent.mlength = mlength;
        e.event.tevent.start = (void *)start;
        PtlInternalPAPIDoneC(PTL_ME_PROCESS, 0);
        PtlInternalEQPush(eq_handle, &e);
        PtlInternalPAPIStartC();
    }
}

int API_FUNC PtlMEAppend(
    ptl_handle_ni_t ni_handle,
    ptl_pt_index_t pt_index,
    ptl_me_t *me,
    ptl_list_t ptl_list,
    void *user_ptr,
    ptl_handle_me_t * me_handle)
{
    const ptl_internal_handle_converter_t ni = { ni_handle };
    ptl_internal_handle_converter_t meh = {.s.selector = HANDLE_ME_CODE };
    ptl_internal_appendME_t *Qentry = NULL;
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
    if (ni.s.ni == 1 || ni.s.ni == 3) { // must be a non-matching NI
        VERBOSE_ERROR("must be a matching NI\n");
        return PTL_ARG_INVALID;
    }
    if (nit.tables[ni.s.ni] == NULL) { // this should never happen
        assert(nit.tables[ni.s.ni] != NULL);
        return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits.max_pt_index) {
        VERBOSE_ERROR("pt_index too high (%u > %u)\n", pt_index,
                      nit_limits.max_pt_index);
        return PTL_ARG_INVALID;
    }
    {
        int ptv = PtlInternalPTValidate(&nit.tables[ni.s.ni][pt_index]);
        if (ptv == 1 || ptv == 3) {    // Unallocated or bad EQ (enabled/disabled both allowed)
            VERBOSE_ERROR("MEAppend sees an invalid PT\n");
            return PTL_ARG_INVALID;
        }
    }
#endif
    PtlInternalPAPIStartC();
    assert(mes[ni.s.ni] != NULL);
    meh.s.ni = ni.s.ni;
    /* find an ME handle */
    for (uint32_t offset = 0; offset < nit_limits.max_entries; ++offset) {
        if (mes[ni.s.ni][offset].status == 0) {
            if (PtlInternalAtomicCas32
                (&(mes[ni.s.ni][offset].status), ME_FREE,
                 ME_ALLOCATED) == ME_FREE) {
                meh.s.code = offset;
                mes[ni.s.ni][offset].visible = *me;
                mes[ni.s.ni][offset].pt_index = pt_index;
                mes[ni.s.ni][offset].ptl_list = ptl_list;
                Qentry = &(mes[ni.s.ni][offset].Qentry);
                break;
            }
        }
    }
    if (Qentry == NULL) {
        return PTL_FAIL;
    }
    Qentry->user_ptr = user_ptr;
    Qentry->me_handle = meh;
    Qentry->local_offset = 0;
    Qentry->dont_ignore_bits = ~(me->ignore_bits);
    *me_handle = meh.a;
    /* append to associated list */
    assert(nit.tables[ni.s.ni] != NULL);
    t = &(nit.tables[ni.s.ni][pt_index]);
    //PtlInternalPAPISaveC(PTL_ME_APPEND, 0);
    ptl_assert(pthread_mutex_lock(&t->lock), 0);
    switch (ptl_list) {
        case PTL_PRIORITY_LIST:
            if (t->buffered_headers.head != NULL) {     // implies that overflow.head != NULL
                /* If there are buffered headers, then they get first priority on matching this priority append. */
                ptl_internal_buffered_header_t *cur =
                    (ptl_internal_buffered_header_t *) (t->buffered_headers.
                                                        head);
                ptl_internal_buffered_header_t *prev = NULL;
                const ptl_match_bits_t dont_ignore_bits = ~(me->ignore_bits);
                for (; cur != NULL; prev = cur, cur = cur->hdr.next) {
                    /* check the match_bits */
                    if (((cur->hdr.match_bits ^ me->
                          match_bits) & dont_ignore_bits) != 0)
                        continue;
                    /* check for forbidden truncation */
                    if ((me->options & PTL_ME_NO_TRUNCATE) != 0 &&
                        (cur->hdr.length + cur->hdr.dest_offset) > me->length)
                        continue;
                    /* check for match_id */
                    if (ni.s.ni <= 1) { // Logical
                        if (me->match_id.rank != PTL_RANK_ANY &&
                            me->match_id.rank != cur->hdr.target_id.rank)
                            continue;
                    } else {           // Physical
                        if (me->match_id.phys.nid != PTL_NID_ANY &&
                            me->match_id.phys.nid !=
                            cur->hdr.target_id.phys.nid)
                            continue;
                        if (me->match_id.phys.pid != PTL_PID_ANY &&
                            me->match_id.phys.pid !=
                            cur->hdr.target_id.phys.pid)
                            continue;
                    }
                    /* now, act like there was a delivery;
                     * 1. Dequeue header
                     * 2. Check permissions
                     * 3. Iff ME is persistent...
                     * 4a. Queue buffered header to ME buffer
                     * 5a. When done processing entire unexpected header list, send retransmit request
                     * ... else: deliver and return */
                    // dequeue header
                    if (prev != NULL) {
                        prev->hdr.next = cur->hdr.next;
                    } else {
                        t->buffered_headers.head = cur->hdr.next;
                    }
                    // check permissions
                    if (me->options & PTL_ME_AUTH_USE_JID) {
                        if (me->ac_id.jid != PTL_JID_ANY) {
                            goto permission_violation;
                        }
                    } else {
                        if (me->ac_id.uid != PTL_UID_ANY) {
                            goto permission_violation;
                        }
                    }
                    switch (cur->hdr.type) {
                        case HDR_TYPE_PUT:
                        case HDR_TYPE_ATOMIC:
                        case HDR_TYPE_FETCHATOMIC:
                        case HDR_TYPE_SWAP:
                            if ((me->options & PTL_ME_OP_PUT) == 0) {
                                goto permission_violation;
                            }
                    }
                    switch (cur->hdr.type) {
                        case HDR_TYPE_GET:
                        case HDR_TYPE_FETCHATOMIC:
                        case HDR_TYPE_SWAP:
                            if ((me->options & PTL_ME_OP_GET) == 0) {
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
                    // iff ME is persistent...
                    if ((me->options & PTL_ME_USE_ONCE) != 0) {
#warning PtlMEAppend() does not work with persistent MEs and buffered headers (implementation needs to be fleshed out)
                        /* suggested plan: put an ME-specific buffered header
                         * list on each ME, and when the ME is persistent, it
                         * gets the buffered headers that it matched, in order.
                         * Then, this list can be used to start reworking (e.g.
                         * retransmitting/restarting) the original order of
                         * deliveries. While this list exists on the ME, new
                         * packets get added to that list. Once the list is
                         * empty, the ME becomes a normal persistent ME. */
                        abort();
                        // Queue buffered header to ME buffer
                        // etc.
                    } else {
                        size_t mlength;
                        // deliver
                        if (me->length == 0) {
                            mlength = 0;
                        } else if (cur->hdr.length + cur->hdr.dest_offset >
                                   me->length) {
                            if (me->length > cur->hdr.dest_offset) {
                                mlength = me->length - cur->hdr.dest_offset;
                            } else {
                                mlength = 0;
                            }
                        } else {
                            mlength = cur->hdr.length;
                        }
#ifndef ALWAYS_TRIGGER_OVERFLOW_EVENTS
                        if (cur->buffered_data != NULL) {
                            PtlInternalPerformDelivery(cur->hdr.type,
                                                       (char *)me->start +
                                                       cur->hdr.dest_offset,
                                                       cur->buffered_data,
                                                       mlength, &(cur->hdr));
                            // notify
                            if (t->EQ != PTL_EQ_NONE ||
                                me->ct_handle != PTL_CT_NONE) {
                                PtlInternalAnnounceMEDelivery(t->EQ,
                                                              me->ct_handle,
                                                              cur->hdr.type,
                                                              me->options,
                                                              mlength,
                                                              (uintptr_t) me->
                                                              start +
                                                              cur->hdr.
                                                              dest_offset, 0,
                                                              &(cur->hdr));
                            }
                        } else {
                            /* Cannot deliver buffered messages without local data; so just emit the OVERFLOW event */
                            if (t->EQ != PTL_EQ_NONE ||
                                me->ct_handle != PTL_CT_NONE) {
                                PtlInternalAnnounceMEDelivery(t->EQ,
                                                              me->ct_handle,
                                                              cur->hdr.type,
                                                              me->options,
                                                              mlength,
                                                              (uintptr_t) 0,
                                                              1, &(cur->hdr));
                            }
                        }
#else
                        if (t->EQ != PTL_EQ_NONE ||
                            me->ct_handle != PTL_CT_NONE) {
                            PtlInternalAnnounceLEDelivery(t->EQ, me->ct_handle,
                                                          cur->hdr.type,
                                                          me->options, mlength,
                                                          (uintptr_t) cur->
                                                          buffered_data, 1,
                                                          &(cur->hdr));
                        }
#endif
                        // return
                        PtlInternalDeallocUnexpectedHeader(cur);
                        goto done_appending;
                    }
                }
                /* either nothing matched in the buffered_headers, or something
                 * did but we're appending a persistent ME, so go on and append
                 * to the priority list */
            }
            if (t->priority.tail == NULL) {
                t->priority.head = Qentry;
            } else {
                ((ptl_internal_appendME_t *) (t->priority.tail))->next =
                    Qentry;
            }
            t->priority.tail = Qentry;
            break;
        case PTL_OVERFLOW:
            if (t->overflow.tail == NULL) {
                t->overflow.head = Qentry;
            } else {
                ((ptl_internal_appendME_t *) (t->overflow.tail))->next =
                    Qentry;
            }
            t->overflow.tail = Qentry;
            break;
        case PTL_PROBE_ONLY:
            if (t->buffered_headers.head != NULL) {
                ptl_internal_buffered_header_t *cur =
                    (ptl_internal_buffered_header_t *) (t->buffered_headers.
                                                        head);
                ptl_internal_buffered_header_t *prev = NULL;
                const ptl_match_bits_t dont_ignore_bits = ~(me->ignore_bits);
                for (; cur != NULL; prev = cur, cur = cur->hdr.next) {
                    /* check the match_bits */
                    if (((cur->hdr.match_bits ^ me->
                          match_bits) & dont_ignore_bits) != 0)
                        continue;
                    /* check for forbidden truncation */
                    if ((me->options & PTL_ME_NO_TRUNCATE) != 0 &&
                        (cur->hdr.length + cur->hdr.dest_offset) > me->length)
                        continue;
                    /* check for match_id */
                    if (ni.s.ni <= 1) { // Logical
                        if (me->match_id.rank != PTL_RANK_ANY &&
                            me->match_id.rank != cur->hdr.target_id.rank)
                            continue;
                    } else {           // Physical
                        if (me->match_id.phys.nid != PTL_NID_ANY &&
                            me->match_id.phys.nid !=
                            cur->hdr.target_id.phys.nid)
                            continue;
                        if (me->match_id.phys.pid != PTL_PID_ANY &&
                            me->match_id.phys.pid !=
                            cur->hdr.target_id.phys.pid)
                            continue;
                    }
                    /* now, act like there was a delivery;
                     * 1. Check permissions
                     * 2. Queue buffered header to ME buffer
                     * 4a. When done processing entire unexpected header list, send retransmit request
                     * ... else: deliver and return */
                    // (1) check permissions
                    if (me->options & PTL_ME_AUTH_USE_JID) {
                        if (me->ac_id.jid != PTL_JID_ANY) {
                            goto permission_violationPO;
                        }
                    } else {
                        if (me->ac_id.uid != PTL_UID_ANY) {
                            goto permission_violationPO;
                        }
                    }
                    switch (cur->hdr.type) {
                        case HDR_TYPE_PUT:
                        case HDR_TYPE_ATOMIC:
                        case HDR_TYPE_FETCHATOMIC:
                        case HDR_TYPE_SWAP:
                            if ((me->options & PTL_ME_OP_PUT) == 0) {
                                goto permission_violationPO;
                            }
                    }
                    switch (cur->hdr.type) {
                        case HDR_TYPE_GET:
                        case HDR_TYPE_FETCHATOMIC:
                        case HDR_TYPE_SWAP:
                            if ((me->options & PTL_ME_OP_GET) == 0) {
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
                        if (me->length == 0) {
                            mlength = 0;
                        } else if (cur->hdr.length + cur->hdr.dest_offset >
                                   me->length) {
                            if (me->length > cur->hdr.dest_offset) {
                                mlength = me->length - cur->hdr.dest_offset;
                            } else {
                                mlength = 0;
                            }
                        } else {
                            mlength = cur->hdr.length;
                        }
                        // notify
                        if (t->EQ != PTL_EQ_NONE) {
                            ptl_event_t e;
                            PTL_INTERNAL_INIT_TEVENT(e, (&(cur->hdr)));
                            e.type = PTL_EVENT_PROBE;
                            e.event.tevent.mlength = mlength;
                            e.event.tevent.start = cur->buffered_data;
                            PtlInternalEQPush(t->EQ, &e);
                        }
                    }
                    if (me->options & PTL_ME_USE_ONCE) {
                        goto done_appending;
                    }
                }
            }
            break;
    }
  done_appending:
    ptl_assert(pthread_mutex_unlock(&t->lock), 0);
    PtlInternalPAPIDoneC(PTL_ME_APPEND, 1);
    return PTL_OK;
}

int API_FUNC PtlMEUnlink(
    ptl_handle_me_t me_handle)
{
    const ptl_internal_handle_converter_t me = { me_handle };
    ptl_table_entry_t *t;
#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        VERBOSE_ERROR("communication pad not initialized");
        return PTL_NO_INIT;
    }
    if (me.s.ni > 3 || me.s.code > nit_limits.max_entries ||
        nit.refcount[me.s.ni] == 0) {
        VERBOSE_ERROR
            ("ME Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n",
             me.s.ni, me.s.code, nit_limits.max_entries);
        return PTL_ARG_INVALID;
    }
    if (mes[me.s.ni] == NULL) {
        VERBOSE_ERROR("ME array uninitialized\n");
        return PTL_ARG_INVALID;
    }
    if (mes[me.s.ni][me.s.code].status == ME_FREE) {
        VERBOSE_ERROR("ME appears to be free already\n");
        return PTL_ARG_INVALID;
    }
#endif
    t = &(nit.tables[me.s.ni][mes[me.s.ni][me.s.code].pt_index]);
    ptl_assert(pthread_mutex_lock(&t->lock), 0);
    switch (mes[me.s.ni][me.s.code].ptl_list) {
        case PTL_PRIORITY_LIST:
        {
            ptl_internal_appendME_t *dq =
                (ptl_internal_appendME_t *) (t->priority.head);
            if (dq == &(mes[me.s.ni][me.s.code].Qentry)) {
                if (dq->next != NULL) {
                    t->priority.head = dq->next;
                } else {
                    t->priority.head = t->priority.tail = NULL;
                }
            } else {
                ptl_internal_appendME_t *prev = NULL;
                while (dq != &(mes[me.s.ni][me.s.code].Qentry) && dq != NULL) {
                    prev = dq;
                    dq = dq->next;
                }
                if (dq == NULL) {
                    fprintf(stderr, "attempted to link an un-queued ME\n");
                    abort();
                }
                prev->next = dq->next;
                if (dq->next == NULL) {
                    assert(t->priority.tail == dq);
                    t->priority.tail = prev;
                }
            }
        }
            break;
        case PTL_OVERFLOW:
        {
            ptl_internal_appendME_t *dq =
                (ptl_internal_appendME_t *) (t->overflow.head);
            if (dq == &(mes[me.s.ni][me.s.code].Qentry)) {
                if (dq->next != NULL) {
                    t->overflow.head = dq->next;
                } else {
                    t->overflow.head = t->overflow.tail = NULL;
                }
            } else {
                ptl_internal_appendME_t *prev = NULL;
                while (dq != &(mes[me.s.ni][me.s.code].Qentry) && dq != NULL) {
                    prev = dq;
                    dq = dq->next;
                }
                if (dq == NULL) {
                    fprintf(stderr, "attempted to link an un-queued ME\n");
                    abort();
                }
                prev->next = dq->next;
                if (dq->next == NULL) {
                    assert(t->overflow.tail == dq);
                    t->overflow.tail = prev;
                }
            }
        }
            break;
        case PTL_PROBE_ONLY:
            fprintf(stderr, "how on earth did this happen?\n");
            abort();
            break;
    }
    ptl_assert(pthread_mutex_unlock(&t->lock), 0);
    switch (PtlInternalAtomicCas32
            (&(mes[me.s.ni][me.s.code].status), ME_ALLOCATED, ME_FREE)) {
        case ME_IN_USE:
            return PTL_IN_USE;
        case ME_ALLOCATED:
            return PTL_OK;
#ifndef NO_ARG_VALIDATION
        case ME_FREE:
            VERBOSE_ERROR("ME unexpectedly became free");
            return PTL_ARG_INVALID;
#endif
    }
    return PTL_OK;
}

static void PtlInternalWalkMatchList(
    const ptl_match_bits_t incoming_bits,
    const unsigned char ni,
    const ptl_process_t target,
    const ptl_size_t length,
    const ptl_size_t offset,
    ptl_internal_appendME_t ** matchlist,
    ptl_internal_appendME_t ** mprev,
    ptl_me_t ** mme)
{
    ptl_internal_appendME_t *current = *matchlist;
    ptl_internal_appendME_t *prev = *mprev;
    ptl_me_t *me = *mme;

    for (; current != NULL; prev = current, current = current->next) {
        me = (ptl_me_t *) (((char *)current) +
                           offsetof(ptl_internal_me_t, visible));

        /* check the match_bits */
        if (((incoming_bits ^ me->match_bits) & current->dont_ignore_bits) !=
            0)
            continue;
        /* check for forbidden truncation */
        if ((me->options & PTL_ME_NO_TRUNCATE) != 0 &&
            (length + offset) > (me->length - current->local_offset))
            continue;
        /* check for match_id */
        if (ni <= 1) {                 // Logical
            if (me->match_id.rank != PTL_RANK_ANY &&
                me->match_id.rank != target.rank)
                continue;
        } else {                       // Physical
            if (me->match_id.phys.nid != PTL_NID_ANY &&
                me->match_id.phys.nid != target.phys.nid)
                continue;
            if (me->match_id.phys.pid != PTL_PID_ANY &&
                me->match_id.phys.pid != target.phys.pid)
                continue;
        }
        break;
    }
    *matchlist = current;
    *mprev = prev;
    *mme = me;
}

ptl_pid_t INTERNAL PtlInternalMEDeliver(
    ptl_table_entry_t * restrict t,
    ptl_internal_header_t * restrict hdr)
{
    assert(t);
    assert(hdr);
    enum { PRIORITY, OVERFLOW } foundin = PRIORITY;
    ptl_internal_appendME_t *prev = NULL, *entry = t->priority.head;
    ptl_me_t *me_ptr = NULL;
    ptl_handle_eq_t tEQ = t->EQ;
    char need_to_unlock = 1; // to decide whether to unlock the table upon return or whether it was unlocked earlier

    PtlInternalPAPIStartC();
    /* To match, one must check, in order:
     * 1. The match_bits (with the ignore_bits) against hdr->match_bits
     * 2. if notruncate, length
     * 3. the match_id against hdr->target_id
     */
    PtlInternalWalkMatchList(hdr->match_bits, hdr->ni, hdr->target_id,
                             hdr->length, hdr->dest_offset, &entry, &prev,
                             &me_ptr);
    if (entry == NULL && hdr->type != HDR_TYPE_GET) {   // check overflow list
        prev = NULL;
        entry = t->overflow.head;
        PtlInternalWalkMatchList(hdr->match_bits, hdr->ni, hdr->target_id,
                                 hdr->length, hdr->dest_offset, &entry, &prev,
                                 &me_ptr);
        if (entry != NULL) {
            foundin = OVERFLOW;
        }
    }
    if (entry != NULL) {               // Match
        /*************************************************************************
         * There is a matching ME present, and 'entry'/'me_ptr' points to it *
         *************************************************************************/
        ptl_size_t mlength = 0;
        const ptl_me_t me =
            *(ptl_me_t *) (((char *)entry) +
                           offsetof(ptl_internal_me_t, visible));
        assert(mes[hdr->ni][entry->me_handle.s.code].status != ME_FREE);
        // check permissions on the ME
        if (me.options & PTL_ME_AUTH_USE_JID) {
            if (me.ac_id.jid != PTL_JID_ANY) {
                goto permission_violation;
            }
        } else {
            if (me.ac_id.uid != PTL_UID_ANY) {
                goto permission_violation;
            }
        }
        switch (hdr->type) {
            case HDR_TYPE_PUT:
            case HDR_TYPE_ATOMIC:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                if ((me.options & PTL_ME_OP_PUT) == 0) {
                    goto permission_violation;
                }
        }
        switch (hdr->type) {
            case HDR_TYPE_GET:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                if ((me.options & PTL_ME_OP_GET) == 0) {
                    goto permission_violation;
                }
        }
        if (0) {
          permission_violation:
            (void)PtlInternalAtomicInc(&nit.regs[hdr->ni]
                                       [PTL_SR_PERMISSIONS_VIOLATIONS], 1);
            PtlInternalPAPIDoneC(PTL_ME_PROCESS, 1);
            ptl_assert(pthread_mutex_unlock(&t->lock), 0);
            return (ptl_pid_t) 3;
        }
        /*******************************************************************
         * We have permissions on this ME, now check if it's a use-once ME *
         *******************************************************************/
        if ((me.options & PTL_ME_USE_ONCE) ||
            ((me.options & (PTL_ME_MANAGE_LOCAL)) &&
             (me.min_free != 0) &&
             ((me.length - entry->local_offset) - me.min_free <= hdr->length))) {
            /* that last bit of math only works because we already know that
             * the hdr body can, at least partially, fit into this entry. In
             * essence, the comparison is:
             *      avalable_space - reserved_space <= incoming_block
             * We calculate how much space is available without using reserved
             * space (math which should NOT cause the offsets to roll-over or
             * go negative), and compare that to the length of the incoming
             * data. This works even if we will have to truncate the incoming
             * data. The gyrations here, rather than something straightforward
             * like
             *      available_space - incoming_block <= reserved_space
             * are to avoid problems with offsets rolling over when enormous
             * messages are sent (esp. ones that are allowed to be truncated).
             */
            /* unlink ME */
            if (prev != NULL) {
                prev->next = entry->next;
            } else {
                if (foundin == PRIORITY) {
                    t->priority.head = entry->next;
                    if (entry->next == NULL)
                        t->priority.tail = NULL;
                } else {
                    t->overflow.head = entry->next;
                    if (entry->next == NULL)
                        t->overflow.tail = NULL;
                }
            }
            /* now that the LE has been unlinked, we can unlock the portal
             * table, thus allowing deliveries and/or appends on the PT while
             * we do this delivery */
            need_to_unlock = 0;
            ptl_assert(pthread_mutex_unlock(&t->lock), 0);
            if (tEQ != PTL_EQ_NONE &&
                (me.options & PTL_ME_EVENT_UNLINK_DISABLE) == 0) {
                ptl_event_t e;
                PTL_INTERNAL_INIT_TEVENT(e, hdr);
                e.type = PTL_EVENT_AUTO_UNLINK;
                e.event.tevent.start = (char *)me.start + hdr->dest_offset;
                PtlInternalPAPIDoneC(PTL_ME_PROCESS, 2);
                PtlInternalEQPush(tEQ, &e);
                PtlInternalPAPIStartC();
            }
        }
        /* check lengths */
        if (hdr->length + hdr->dest_offset > me.length) {
            if (me.length > hdr->dest_offset) {
                mlength = me.length - hdr->dest_offset;
            } else {
                mlength = 0;
            }
        } else {
            mlength = hdr->length;
        }
        /*************************
         * Perform the Operation *
         *************************/
        void *report_this_start = (char *)me.start + hdr->dest_offset;
        if (foundin == PRIORITY) {
            PtlInternalPerformDelivery(hdr->type, report_this_start,
                                       hdr->data, mlength, hdr);
            PtlInternalAnnounceMEDelivery(tEQ, me.ct_handle, hdr->type,
                                          me.options, mlength,
                                          (uintptr_t) report_this_start, 0,
                                          hdr);
        } else {
#warning Sending a PtlGet to the overflow list probably doesn't work
            report_this_start =
                PtlInternalPerformOverflowDelivery(entry, me.start, me.length,
                                                   me.options, mlength, hdr);
            PtlInternalPTBufferUnexpectedHeader(t, hdr, (uintptr_t)
                                                report_this_start);
        }
        switch (hdr->type) {
            case HDR_TYPE_PUT:
            case HDR_TYPE_ATOMIC:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                PtlInternalPAPIDoneC(PTL_ME_PROCESS, 3);
                if (need_to_unlock)
                    ptl_assert(pthread_mutex_unlock(&t->lock), 0);
                return (ptl_pid_t) ((me.
                                     options & (PTL_ME_ACK_DISABLE)) ? 0 : 1);
            default:
                PtlInternalPAPIDoneC(PTL_ME_PROCESS, 3);
                if (need_to_unlock)
                    ptl_assert(pthread_mutex_unlock(&t->lock), 0);
                return (ptl_pid_t) 1;
        }
    }
    // post dropped message event
    if (tEQ != PTL_EQ_NONE) {
        ptl_event_t e;
        PTL_INTERNAL_INIT_TEVENT(e, hdr);
        e.type = PTL_EVENT_DROPPED;
        e.event.tevent.start = NULL;
        PtlInternalPAPIDoneC(PTL_ME_PROCESS, 4);
        PtlInternalEQPush(tEQ, &e);
        PtlInternalPAPIStartC();
    }
    (void)PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_DROP_COUNT], 1);
    PtlInternalPAPIDoneC(PTL_ME_PROCESS, 5);
    if (need_to_unlock)
        ptl_assert(pthread_mutex_unlock(&t->lock), 0);
    return 0;                          // silent ACK
}

/* vim:set expandtab: */
