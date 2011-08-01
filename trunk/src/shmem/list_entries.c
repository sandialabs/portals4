/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <string.h>                    /* for memcpy() */
#include <stdio.h>
#include <stdlib.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_ints.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_LE.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_PT.h"
#include "ptl_internal_DM.h"
#ifndef NO_ARG_VALIDATION
# include "ptl_internal_error.h"
#endif
#include "ptl_internal_performatomic.h"
#include "ptl_internal_papi.h"
#include "ptl_internal_fragments.h"
#include "ptl_internal_alignment.h"
#include "ptl_internal_transfer_engine.h"

#define LE_FREE      0
#define LE_ALLOCATED 1
#define LE_IN_USE    2

typedef struct {
    void                           *next;
    void                           *user_ptr;
    ptl_internal_handle_converter_t le_handle;
} ptl_internal_appendLE_t;

typedef struct {
    ptl_internal_appendLE_t Qentry;
    ptl_le_t                visible;
    volatile uint32_t       status;     // 0=free, 1=allocated, 2=in-use
    ptl_pt_index_t          pt_index;
    ptl_list_t              ptl_list;
} ptl_internal_le_t;

static ptl_internal_le_t *les[4] = { NULL, NULL, NULL, NULL };

#ifdef STRICT_UID_JID
# define EXT_UID extern ptl_uid_t the_ptl_uid
# define CHECK_JID(a, b) (((a) != PTL_JID_ANY) && ((a) != (b)))
# define CHECK_UID(a, b) (((a) != PTL_UID_ANY) && ((a) != (b)))
#else
# define EXT_UID do { } while (0)
# define CHECK_JID(a, b) ((a) != PTL_JID_ANY)
# define CHECK_UID(a, b) (0)
#endif

/* Static functions */
static void PtlInternalPerformDelivery(const uint_fast8_t                    type,
                                       void *const restrict                  local_data,
                                       uint8_t *const restrict               message_data,
                                       const size_t                          nbytes,
                                       ptl_internal_header_t *const restrict hdr);
static void PtlInternalAnnounceLEDelivery(const ptl_handle_eq_t                 eq_handle,
                                          const ptl_handle_ct_t                 ct_handle,
                                          const uint_fast8_t                    type,
                                          const unsigned int                    options,
                                          const uint_fast64_t                   mlength,
                                          const uintptr_t                       start,
                                          const uint_fast8_t                    overflow,
                                          void *const                           user_ptr,
                                          ptl_internal_header_t *const restrict hdr);
#ifdef USE_TRANSFER_ENGINE
static inline void PtlInternalPerformDelivery2(const uint_fast8_t                    type,
                                               void *const restrict                  local_data,
                                               uint8_t *const restrict               message_data,
                                               const size_t                          nbytes,
                                               ptl_internal_header_t *const restrict hdr,
                                               uint8_t *const restrict               op);
static void PtlInternalPerformDeliveryXFE(const uint_fast8_t                    type,
                                          void *const restrict                  local_data,
                                          const uint64_t                        msg_xfe_handle1,
                                          const size_t                          msg_xfe_offset1,
                                          const uint64_t                        msg_xfe_handle2,
                                          const size_t                          msg_xfe_offset2,
                                          const size_t                          nbytes,
                                          ptl_internal_header_t *const restrict hdr,
                                          uint8_t *const restrict               op);
#endif /* ifdef USE_TRANSFER_ENGINE */

void INTERNAL PtlInternalLENISetup(const uint_fast8_t ni,
                                   const ptl_size_t   limit)
{                                      /*{{{ */
    ptl_internal_le_t *tmp;

    while ((tmp = PtlInternalAtomicCasPtr(&(les[ni]), NULL,
                                          (void *)1)) == (void *)1) SPINLOCK_BODY();
    if (tmp == NULL) {
        ALIGNED_CALLOC(tmp, CACHELINE_WIDTH, limit, sizeof(ptl_internal_le_t));
        assert(tmp != NULL);
        __sync_synchronize();
        les[ni] = tmp;
    }
}                                      /*}}} */

void INTERNAL PtlInternalLENITeardown(const uint_fast8_t ni)
{                                      /*{{{ */
    ptl_internal_le_t *tmp;

    tmp     = les[ni];
    les[ni] = NULL;
    assert(tmp != NULL);
    assert(tmp != (void *)1);
    ALIGNED_FREE(tmp, CACHELINE_WIDTH);
}                                      /*}}} */

#ifdef STRICT_UID_JID
# define HDRUID the_ptl_uid
# define HDRJID(hdr) hdr->jid
#else
# define HDRUID ((ptl_internal_uid_t)PTL_UID_ANY)
# define HDRJID(hdr) ((ptl_internal_uid_t)PTL_JID_NONE)
#endif

#define PTL_INTERNAL_INIT_TEVENT(e, hdr, uptr) do {               \
        EXT_UID;                                                  \
        e.pt_index      = hdr->pt_index;                          \
        e.uid           = HDRUID;                                 \
        e.jid           = HDRJID(hdr);                            \
        e.match_bits    = hdr->match_bits;                        \
        e.rlength       = hdr->length;                            \
        e.mlength       = 0;                                      \
        e.remote_offset = hdr->dest_offset;                       \
        e.user_ptr      = uptr;                                   \
        e.ni_fail_type  = PTL_NI_OK;                              \
        if (hdr->ni <= 1) { /* Logical */                         \
            e.initiator.rank = hdr->src;                          \
        } else { /* Physical */                                   \
            e.initiator.phys.pid = hdr->src;                      \
            e.initiator.phys.nid = 0;                             \
        }                                                         \
        switch (hdr->type & HDR_TYPE_BASICMASK) {                 \
            case HDR_TYPE_PUT: e.type = PTL_EVENT_PUT;            \
                e.hdr_data            = hdr->hdr_data;            \
                break;                                            \
            case HDR_TYPE_ATOMIC: e.type = PTL_EVENT_ATOMIC;      \
                e.hdr_data               = hdr->hdr_data;         \
                break;                                            \
            case HDR_TYPE_FETCHATOMIC: e.type = PTL_EVENT_ATOMIC; \
                e.hdr_data                    = hdr->hdr_data;    \
                break;                                            \
            case HDR_TYPE_SWAP: e.type = PTL_EVENT_ATOMIC;        \
                e.hdr_data             = hdr->hdr_data;           \
                break;                                            \
            case HDR_TYPE_GET: e.type = PTL_EVENT_GET;            \
                e.hdr_data            = 0;                        \
                break;                                            \
        }                                                         \
} while (0)

int API_FUNC PtlLEAppend(ptl_handle_ni_t  ni_handle,
                         ptl_pt_index_t   pt_index,
                         ptl_le_t        *le,
                         ptl_list_t       ptl_list,
                         void            *user_ptr,
                         ptl_handle_le_t *le_handle)
{                                      /*{{{ */
    const ptl_internal_handle_converter_t ni     = { ni_handle };
    ptl_internal_handle_converter_t       leh    = { .s.selector = HANDLE_LE_CODE };
    ptl_internal_appendLE_t              *Qentry = NULL;
    ptl_table_entry_t                    *t;

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni == 0) || (ni.s.ni == 2)) { // must be a non-matching NI
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
        if ((ptv == 1) || (ptv == 3)) {    // Unallocated or bad EQ (enabled/disabled both allowed)
            VERBOSE_ERROR("LEAppend sees an invalid PT\n");
            return PTL_ARG_INVALID;
        }
    }
#endif /* ifndef NO_ARG_VALIDATION */
    assert(les[ni.s.ni] != NULL);
    leh.s.ni = ni.s.ni;
    /* find an LE handle */
    for (int offset = 0; offset < nit_limits[ni.s.ni].max_entries; ++offset) {
        if (les[ni.s.ni][offset].status == 0) {
            if (PtlInternalAtomicCas32(&(les[ni.s.ni][offset].status), LE_FREE, LE_ALLOCATED) == LE_FREE) {
                leh.s.code                    = offset;
                les[ni.s.ni][offset].visible  = *le;
                les[ni.s.ni][offset].pt_index = pt_index;
                les[ni.s.ni][offset].ptl_list = ptl_list;
                Qentry                        = &(les[ni.s.ni][offset].Qentry);
                break;
            }
        }
    }
    if (Qentry == NULL) {
        return PTL_FAIL;
    }
    Qentry->user_ptr  = user_ptr;
    Qentry->le_handle = leh;
    *le_handle        = leh.a;
    /* append to associated list */
    assert(nit.tables[ni.s.ni] != NULL);
    t = &(nit.tables[ni.s.ni][pt_index]);
    PTL_LOCK_LOCK(t->lock);
    switch (ptl_list) {
        case PTL_PRIORITY_LIST:
            if (t->buffered_headers.head != NULL) {     // implies that overflow.head != NULL
                /* If there are buffered headers, then they get first priority on matching this priority append. */
                ptl_internal_buffered_header_t *cur  = (ptl_internal_buffered_header_t *)(t->buffered_headers.head);
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
                        if (le->ac_id.jid == PTL_JID_NONE) {
                            goto permission_violation;
                        }
                        if (CHECK_JID(le->ac_id.jid, cur->hdr.jid)) {
                            goto permission_violation;
                        }
                    } else {
                        EXT_UID;
                        if (CHECK_UID(le->ac_id.uid, the_ptl_uid)) {
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
                        (void)PtlInternalAtomicInc(&nit.regs[cur->hdr.ni][PTL_SR_PERMISSIONS_VIOLATIONS], 1);
                        tmp            = cur;
                        prev->hdr.next = cur->hdr.next;
                        cur            = prev;
                        PtlInternalDeallocUnexpectedHeader(tmp);
                        continue;
                    }
                    // (2) iff LE is persistent
                    if ((le->options & PTL_LE_USE_ONCE) == 0) {
                        fprintf(stderr, "PORTALS4-> PtlLEAppend() does not work with persistent LEs and buffered headers (implementation needs to be fleshed out)\n");
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
                        ptl_handle_eq_t tEQ = t->EQ;
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
                                                       (uint8_t *)le->start +
                                                       cur->hdr.dest_offset,
                                                       cur->buffered_data,
                                                       mlength, &(cur->hdr));
                            // notify
                            if ((tEQ != PTL_EQ_NONE) ||
                                (le->ct_handle != PTL_CT_NONE)) {
                                __sync_synchronize();
                                PtlInternalAnnounceLEDelivery(tEQ,
                                                              le->ct_handle,
                                                              cur->hdr.type,
                                                              le->options,
                                                              mlength,
                                                              (uintptr_t)le->start + cur->hdr.dest_offset,
                                                              0,
                                                              user_ptr,
                                                              &(cur->hdr));
                            }
                        } else {
                            /* Cannot deliver buffered messages without local data; so just emit the OVERFLOW event */
                            if ((tEQ != PTL_EQ_NONE) ||
                                (le->ct_handle != PTL_CT_NONE)) {
                                __sync_synchronize();
                                PtlInternalAnnounceLEDelivery(tEQ,
                                                              le->ct_handle,
                                                              cur->hdr.type,
                                                              le->options,
                                                              mlength,
                                                              (uintptr_t)0,
                                                              1, user_ptr,
                                                              &(cur->hdr));
                            }
                        }
#else               /* ifndef ALWAYS_TRIGGER_OVERFLOW_EVENTS */
                        if ((tEQ != PTL_EQ_NONE) ||
                            (le->ct_handle != PTL_CT_NONE)) {
                            __sync_synchronize();
                            PtlInternalAnnounceLEDelivery(tEQ,
                                                          le->ct_handle,
                                                          cur->hdr.type,
                                                          le->options,
                                                          mlength, (uintptr_t)
                                                          cur->buffered_data,
                                                          1, user_ptr,
                                                          &(cur->hdr));
                        }
#endif              /* ifndef ALWAYS_TRIGGER_OVERFLOW_EVENTS */
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
                ((ptl_internal_appendLE_t *)(t->priority.tail))->next =
                    Qentry;
            }
            t->priority.tail = Qentry;
            break;
        case PTL_OVERFLOW:
            if (t->overflow.tail == NULL) {
                t->overflow.head = Qentry;
            } else {
                ((ptl_internal_appendLE_t *)(t->overflow.tail))->next =
                    Qentry;
            }
            t->overflow.tail = Qentry;
            break;
#if 0
        case PTL_PROBE_ONLY:
            if (t->buffered_headers.head != NULL) {
                ptl_internal_buffered_header_t *cur =
                    (ptl_internal_buffered_header_t *)(t->buffered_headers.
                                                       head);
                for (; cur != NULL; cur = cur->hdr.next) {
                    /* act like there was a delivery;
                    * 1. Check permissions
                    * 2. Iff LE is persistent...
                    * 3a. Queue buffered header to LE buffer
                    * 4a. When done processing entire unexpected header list, send retransmit request
                    * ... else: deliver and return */
                    // (1) check permissions
                    if (le->options & PTL_LE_AUTH_USE_JID) {
                        if (CHECK_JID(le->ac_id.jid, cur->hdr.jid)) {
                            goto permission_violationPO;
                        }
                    } else {
                        EXT_UID;
                        if (CHECK_UID(le->ac_id.uid, the_ptl_uid)) {
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
                        (void)PtlInternalAtomicInc(&nit.regs[cur->hdr.ni][PTL_SR_PERMISSIONS_VIOLATIONS], 1);
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
                            ptl_internal_event_t e;
                            PTL_INTERNAL_INIT_TEVENT(e, (&(cur->hdr)),
                                                     user_ptr);
                            e.type    = PTL_EVENT_PROBE;
                            e.mlength = mlength;
                            e.start   = cur->buffered_data;
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
#endif  /* if 0 */
    }
done_appending:
    PTL_LOCK_UNLOCK(t->lock);
    return PTL_OK;
}                                      /*}}} */

int API_FUNC PtlLESearch(ptl_handle_ni_t ni_handle,
                         ptl_pt_index_t  pt_index,
                         ptl_le_t       *le,
                         ptl_search_op_t ptl_search_op,
                         void           *user_ptr)
{
    const ptl_internal_handle_converter_t ni  = { ni_handle };
    ptl_internal_handle_converter_t       leh = { .s.selector = HANDLE_LE_CODE };
    ptl_table_entry_t                    *t;
    uint_fast8_t                          found = 0;

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("ni code wrong\n");
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni == 0) || (ni.s.ni == 2)) { // must be a non-matching NI
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
        if ((ptv == 1) || (ptv == 3)) {    // Unallocated or bad EQ (enabled/disabled both allowed)
            VERBOSE_ERROR("LEAppend sees an invalid PT\n");
            return PTL_ARG_INVALID;
        }
    }
#endif /* ifndef NO_ARG_VALIDATION */
    assert(les[ni.s.ni] != NULL);
    leh.s.ni = ni.s.ni;
    /* append to associated list */
    assert(nit.tables[ni.s.ni] != NULL);
    t = &(nit.tables[ni.s.ni][pt_index]);
    PTL_LOCK_LOCK(t->lock);
    if (t->buffered_headers.head != NULL) {
        ptl_internal_buffered_header_t *cur  = (ptl_internal_buffered_header_t *)(t->buffered_headers.head);
        ptl_internal_buffered_header_t *prev = NULL;
        for (; cur != NULL; prev = cur, cur = cur->hdr.next) {
            /* act like there was a delivery;
            * 1. Check permissions
            * 2. Iff LE is persistent...
            * 3a. Queue buffered header to LE buffer
            * 4a. When done processing entire unexpected header list, send retransmit request
            * ... else: deliver and return */
            // (1) check permissions
            if (le->options & PTL_LE_AUTH_USE_JID) {
                if (CHECK_JID(le->ac_id.jid, cur->hdr.jid)) {
                    goto permission_violationPO;
                }
            } else {
                EXT_UID;
                if (CHECK_UID(le->ac_id.uid, the_ptl_uid)) {
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
                (void)PtlInternalAtomicInc(&nit.regs[cur->hdr.ni][PTL_SR_PERMISSIONS_VIOLATIONS], 1);
                continue;
            }
            found = 1;
            if (ptl_search_op == PTL_SEARCH_DELETE) {
                // dequeue header
                prev->hdr.next = cur->hdr.next;
            } else {
                t->buffered_headers.head = cur->hdr.next;
            }
            {
                size_t mlength;
                // deliver
                if (le->length == 0) {
                    mlength = 0;
                } else if (cur->hdr.length + cur->hdr.dest_offset > le->length) {
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
                    ptl_internal_event_t e;
                    PTL_INTERNAL_INIT_TEVENT(e, (&(cur->hdr)), user_ptr);
                    if (ptl_search_op == PTL_SEARCH_ONLY) {
                        e.type = PTL_EVENT_SEARCH;
                    } else {
                        switch(cur->hdr.type) {
                            case 0: /* put */
                                e.type = PTL_EVENT_PUT_OVERFLOW;
                                break;
                            case 1: /* get */
                                abort();
                            case 2: /* atomic */
                            case 3: /* fetchatomic */
                            case 4: /* swap */
                                e.type = PTL_EVENT_ATOMIC_OVERFLOW;
                                break;
                        }
                    }
                    e.mlength = mlength;
                    e.start   = cur->buffered_data;
                    PtlInternalEQPush(t->EQ, &e);
                }
            }
            // (2) iff LE is NOT persistent
            if (le->options & PTL_LE_USE_ONCE) {
                goto done_searching;
            }
        }
    }
done_searching:
    if (!found) {
        if (t->EQ != PTL_EQ_NONE) {
            ptl_internal_event_t e;
            e.type           = PTL_EVENT_SEARCH;
            e.initiator.rank = proc_number;
            e.pt_index       = pt_index;
            {
                ptl_uid_t tmp;
                PtlGetUid(ni_handle, &tmp);
                e.uid = tmp;
            }
            {
                ptl_jid_t tmp;
                PtlGetJid(ni_handle, &tmp);
                e.jid = tmp;
            }
            e.match_bits    = 0;
            e.rlength       = 0;
            e.mlength       = 0;
            e.remote_offset = 0;
            e.start         = 0;
            e.user_ptr      = user_ptr;
            e.hdr_data      = 0;
            e.ni_fail_type  = PTL_NI_UNDELIVERABLE;
        }
    }
    PTL_LOCK_UNLOCK(t->lock);
    return PTL_OK;
}

int API_FUNC PtlLEUnlink(ptl_handle_le_t le_handle)
{                                      /*{{{ */
    const ptl_internal_handle_converter_t le = { le_handle };
    ptl_table_entry_t *restrict const     t  =
        &(nit.tables[le.s.ni][les[le.s.ni][le.s.code].pt_index]);
    const ptl_internal_appendLE_t *restrict const dq_target =
        &(les[le.s.ni][le.s.code].Qentry);

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        VERBOSE_ERROR("communication pad not initialized\n");
        return PTL_NO_INIT;
    }
    if ((le.s.ni > 3) || (le.s.code > nit_limits[le.s.ni].max_entries) ||
        (nit.refcount[le.s.ni] == 0)) {
        VERBOSE_ERROR("LE Handle has bad NI (%u > 3) or bad code (%u > %u) or the NIT is uninitialized\n",
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
#endif /* ifndef NO_ARG_VALIDATION */
    PTL_LOCK_LOCK(t->lock);
    if (les[le.s.ni][le.s.code].ptl_list == PTL_PRIORITY_LIST) {
        ptl_internal_appendLE_t *dq = (ptl_internal_appendLE_t *)(t->priority.head);
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
                dq   = dq->next;
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
    } else {     /* PTL_OVERFLOW */
        ptl_internal_appendLE_t *dq = (ptl_internal_appendLE_t *)(t->overflow.head);
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
                dq   = dq->next;
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

    PTL_LOCK_UNLOCK(t->lock);
    switch (PtlInternalAtomicCas32(&(les[le.s.ni][le.s.code].status), LE_ALLOCATED, LE_FREE)) {
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

ptl_pid_t INTERNAL PtlInternalLEDeliver(ptl_table_entry_t *restrict     t,
                                        ptl_internal_header_t *restrict hdr)
{                                      /*{{{ */
    enum {PRIORITY, OVERFLOW} foundin = PRIORITY;
    ptl_internal_appendLE_t *entry    = NULL;
    ptl_handle_eq_t          tEQ      = t->EQ;
    ptl_le_t                 le;
    ptl_size_t               msg_mlength    = 0, fragment_mlength = 0;
    uint_fast8_t             need_more_data = 0;
    uint_fast8_t             need_to_unlock = 1; // to decide whether to unlock the table upon return or whether it was unlocked earlier
#ifdef USE_TRANSFER_ENGINE
    uint_fast8_t use_xfe = 0;                    // whether we're using the transfer engine
#endif

    PtlInternalPAPIStartC();
    assert(t);
    assert(hdr);
    if (hdr->entry == NULL) {
        /* Find an entry */
        if (t->priority.head) {
            entry = t->priority.head;
        } else if (t->overflow.head) {
            entry   = t->overflow.head;
            foundin = OVERFLOW;
        }
        hdr->entry = entry;
    } else {
        entry = hdr->entry;
        le    = *(ptl_le_t *)(((uint8_t *)entry) +
                              offsetof(ptl_internal_le_t, visible));
        goto check_lengths;
    }
    if (entry != NULL) {
        /*********************************************************
        * There is an LE present, and 'entry' points to it *
        *********************************************************/
        le = *(ptl_le_t *)(((uint8_t *)entry) +
                           offsetof(ptl_internal_le_t, visible));
        assert(les[hdr->ni][entry->le_handle.s.code].status != LE_FREE);
        // check the permissions on the LE
        if (le.options & PTL_LE_AUTH_USE_JID) {
            if (CHECK_JID(le.ac_id.jid, hdr->jid)) {
                goto permission_violation;
            }
        } else {
            EXT_UID;
            if (CHECK_UID(le.ac_id.uid, the_ptl_uid)) {
                goto permission_violation;
            }
        }
        switch (hdr->type & HDR_TYPE_BASICMASK) {
            case HDR_TYPE_PUT:
            case HDR_TYPE_ATOMIC:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                if ((le.options & PTL_LE_OP_PUT) == 0) {
                    goto permission_violation;
                }
        }
        switch (hdr->type & HDR_TYPE_BASICMASK) {
            case HDR_TYPE_GET:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                if ((le.options & (PTL_LE_ACK_DISABLE | PTL_LE_OP_GET)) ==
                    0) {
                    goto permission_violation;
                }
        }
        if (0) {
permission_violation:
            (void)PtlInternalAtomicInc(&nit.regs[hdr->ni]
                                       [PTL_SR_PERMISSIONS_VIOLATIONS], 1);
            // PtlInternalPAPIDoneC(PTL_LE_PROCESS, 0);
            PTL_LOCK_UNLOCK(t->lock);
            return (ptl_pid_t)3;
        }
        /*******************************************************************
        * We have permissions on this LE, now check if it's a use-once LE *
        *******************************************************************/
        if (le.options & PTL_LE_USE_ONCE) {
            // unlink LE
            if (foundin == PRIORITY) {
                t->priority.head = entry->next;
                if (entry->next == NULL) {
                    t->priority.tail = NULL;
                }
            } else {
                t->overflow.head = entry->next;
                if (entry->next == NULL) {
                    t->overflow.tail = NULL;
                }
            }
            entry->next = NULL;
            /* now that the LE has been unlinked, we can unlock the portal
             * table, thus allowing appends on the PT while we do this delivery
             */
            need_to_unlock = 0;
            PTL_LOCK_UNLOCK(t->lock);
            if ((tEQ != PTL_EQ_NONE) &&
                ((le.options & (PTL_LE_EVENT_UNLINK_DISABLE)) == 0)) {
                ptl_internal_event_t e;
                PTL_INTERNAL_INIT_TEVENT(e, hdr, entry->user_ptr);
                e.type  = PTL_EVENT_AUTO_UNLINK;
                e.start = (uint8_t *)le.start + hdr->dest_offset;
                PtlInternalEQPush(tEQ, &e);
            }
        }
check_lengths:
        /* check lengths */
        {
            size_t max_payload;

#ifdef USE_TRANSFER_ENGINE
            if (hdr->xfe_handle1) {
                /* we can actually do the entire transfer now, so fake our
                 * max_payload value */
                max_payload = hdr->length;
                use_xfe     = 1;
            } else {
                max_payload = PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t);
            }
#else
            max_payload = PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t);
#endif

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
            if (msg_mlength < hdr->length) {
                hdr->length    = msg_mlength;
                hdr->remaining = msg_mlength;
                hdr->type     |= HDR_TYPE_TRUNCFLAG;
            }
            if (max_payload >= msg_mlength) {
                /* the entire operation fits into a single fragment */
                fragment_mlength = msg_mlength;
                need_more_data   = 0;
            } else {
                /* the operation requires multiple fragments */
                if (hdr->remaining > max_payload) {
                    /* this is NOT the last fragment */
                    fragment_mlength = max_payload;
                    need_more_data   = 1;
                } else {
                    /* this IS the last fragment */
                    fragment_mlength = hdr->remaining;
                    need_more_data   = 0;
                }
            }
        }
        /*************************
        * Perform the Operation *
        *************************/
        /*
         * msg_mlength is the total bytecount of the message fragment_mlength
         * is the total bytecount of this packet remaining is the total
         * bytecount that has not been transmitted yet Thus, the offset from
         * the beginning of the message that this fragment refers to is...
         * me.start + dest_offset + (msg_mlength - fragment_mlength - remaining)
         * >_____+--------####====+____<
         * |     |        |   |   |    `--> le.start + le.length
         * |     |        |   |   `-------> le.start + hdr->dest_offset + ( msg_mlength )
         * |     |        |   `-----------> le.start + hdr->dest_offset + ( msg_mlength - ( remaining - fragment_mlength ) )
         * |     |        `---------------> le.start + hdr->dest_offset + ( msg_mlength - remaining )
         * |     `------------------------> le.start + hdr->dest_offset
         * `------------------------------> le.start
         */
        void *report_this_start = (uint8_t *)le.start + hdr->dest_offset;
        void *effective_start   =
            (uint8_t *)le.start + hdr->dest_offset + (msg_mlength -
                                                      hdr->remaining);
        if (foundin == PRIORITY) {
            if (fragment_mlength > 0) {
#ifdef USE_TRANSFER_ENGINE
                if (use_xfe) {
/* One subtle difference with register-on-data-movement is that we only
 * register the exact range of memory being transfered. As opposed to
 * register-on-bind, where the entire MD memory region is registered.
 * So we need to be careful with our use of offsets... */
# ifdef REGISTER_ON_BIND
                    const size_t xfe_offset1 = hdr->local_offset1;
                    const size_t xfe_offset2 = hdr->local_offset2;
# else
                    const size_t xfe_offset1 = 0;
                    const size_t xfe_offset2 = 0;
# endif
                    PtlInternalPerformDeliveryXFE(hdr->type, effective_start,
                                                  hdr->xfe_handle1,
                                                  xfe_offset1,
                                                  hdr->xfe_handle2,
                                                  xfe_offset2,
                                                  fragment_mlength, hdr,
                                                  hdr->data);
                } else {
                    PtlInternalPerformDelivery(hdr->type, effective_start,
                                               hdr->data, fragment_mlength,
                                               hdr);
                }
#else           /* ifdef USE_TRANSFER_ENGINE */
                PtlInternalPerformDelivery(hdr->type, effective_start,
                                           hdr->data, fragment_mlength,
                                           hdr);
#endif          /* ifdef USE_TRANSFER_ENGINE */
            }
            if (need_more_data == 0) {
                __sync_synchronize();
                PtlInternalAnnounceLEDelivery(tEQ, le.ct_handle, hdr->type,
                                              le.options, msg_mlength,
                                              (uintptr_t)report_this_start,
                                              0, entry->user_ptr, hdr);
            }
        } else {
            if ((fragment_mlength != msg_mlength) && (le.length > 0)) {
                fprintf(stderr, "multi-fragment (oversize) messages into the overflow list doesn't work\n");
                abort();
            }
            assert(hdr->length + hdr->dest_offset <= fragment_mlength);
            if (fragment_mlength > 0) {
                memcpy(report_this_start, hdr->data, fragment_mlength);
            } else {
                report_this_start = NULL;
            }
            PtlInternalPTBufferUnexpectedHeader(t, hdr, (uintptr_t)entry,
                                                (uintptr_t)report_this_start);
        }
        switch (hdr->type & HDR_TYPE_BASICMASK) {
            case HDR_TYPE_PUT:
            case HDR_TYPE_ATOMIC:
            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                PtlInternalPAPIDoneC(PTL_LE_PROCESS, 0);
                if (need_to_unlock) {
                    PTL_LOCK_UNLOCK(t->lock);
                }
                return (ptl_pid_t)((le.
                                    options & PTL_LE_ACK_DISABLE) ? 0 : 1);

            default:
                PtlInternalPAPIDoneC(PTL_ME_PROCESS, 0);
                if (need_to_unlock) {
                    PTL_LOCK_UNLOCK(t->lock);
                }
                return (ptl_pid_t)1;
        }
    }
#ifdef LOUD_DROPS
    fprintf(stderr, "PORTALS4-> Rank %u dropped a message from rank %u, no LEs posted on PT %u on NI %u\n",
            (unsigned)proc_number, (unsigned)hdr->src,
            (unsigned)hdr->pt_index, (unsigned)hdr->ni);
    fflush(stderr);
#endif
    // post dropped message event
    if (tEQ != PTL_EQ_NONE) {
        ptl_internal_event_t e;
        PTL_INTERNAL_INIT_TEVENT(e, hdr, NULL);
        e.start        = NULL;
        e.ni_fail_type = PTL_NI_DROPPED;
        PtlInternalEQPush(tEQ, &e);
    }
    (void)PtlInternalAtomicInc(&nit.regs[hdr->ni][PTL_SR_DROP_COUNT], 1);
    PtlInternalPAPIDoneC(PTL_LE_PROCESS, 0);
    if (need_to_unlock) {
        PTL_LOCK_UNLOCK(t->lock);
    }
    return 0;                          // silent ACK
}                                      /*}}} */

#ifdef USE_TRANSFER_ENGINE
static void PtlInternalPerformDeliveryXFE(const uint_fast8_t              type,
                                          void *const restrict            local_data,
                                          const uint64_t                  msg_xfe_handle1,
                                          const size_t                    msg_xfe_offset1,
                                          const uint64_t                  msg_xfe_handle2,
                                          const size_t                    msg_xfe_offset2,
                                          const size_t                    nbytes,
                                          ptl_internal_header_t *restrict hdr,
                                          uint8_t *const restrict         op)
{                                      /*{{{ */
    uint_fast8_t       copy_back    = 0;
    uint_fast8_t       have_operand = 0;
    const uint_fast8_t basictype    = type & HDR_TYPE_BASICMASK;

    /* Determine if our transfer engine can give us direct access to the
     * remote memory. If so, we can perform delivery in-place, without
     * doing extra copying.
     *
     * Caveat: For FetchAtomic and Swap, if Put MD != Get MD, we can't
     *         avoid the copy.
     */
    uint8_t *remote_buf = xfe_attach(msg_xfe_handle1);

    if (remote_buf) {
        switch (basictype) {
            case HDR_TYPE_PUT:
            case HDR_TYPE_GET:
            case HDR_TYPE_ATOMIC:
                PtlInternalPerformDelivery2(type, local_data, remote_buf,
                                            nbytes, hdr, op);
                return; // we're done

            case HDR_TYPE_FETCHATOMIC:
            case HDR_TYPE_SWAP:
                if (msg_xfe_handle1 == msg_xfe_handle2) {
                    PtlInternalPerformDelivery2(type, local_data, remote_buf,
                                                nbytes, hdr, op);
                    return; // we're done
                }
        }
    }

    switch (basictype) {
        case HDR_TYPE_PUT:
            xfe_copy_from(local_data, msg_xfe_handle1, msg_xfe_offset1, nbytes);
            break;
        case HDR_TYPE_GET:
            xfe_copy_to(msg_xfe_handle1, msg_xfe_offset1, local_data, nbytes);
            break;
        case HDR_TYPE_SWAP:
            have_operand = 1;
        // fall through
        case HDR_TYPE_FETCHATOMIC:
            copy_back = 1;
        // fall through
        case HDR_TYPE_ATOMIC:
        {
            /* copy and operate on the remote data piece-by-piece */
            uint8_t chunk[2 * LARGE_FRAG_SIZE];     // TODO totally arbitrary
            size_t  remaining = nbytes;
            size_t  offset    = 0;

            while (remaining) {
                const size_t chunk_size = (remaining > sizeof(chunk))
                                          ? sizeof(chunk)
                                          : remaining;

                /* copy chunk-sized piece of initiator data */
                xfe_copy_from(chunk, msg_xfe_handle1,
                              msg_xfe_offset1 + offset,
                              chunk_size);

                if (have_operand) {
                    PtlInternalPerformAtomicArg(local_data + offset,
                                                chunk, op, chunk_size,
                                                (ptl_op_t)hdr->atomic_operation,
                                                (ptl_datatype_t)hdr->atomic_datatype);
                } else {
                    PtlInternalPerformAtomic(local_data + offset,
                                             chunk, chunk_size,
                                             (ptl_op_t)hdr->atomic_operation,
                                             (ptl_datatype_t)hdr->atomic_datatype);
                }

                /* copy result data back to the initiator's 'get' region */
                if (copy_back) {
                    xfe_copy_to(msg_xfe_handle2, msg_xfe_offset2 + offset,
                                chunk, chunk_size);
                }

                remaining -= chunk_size;
                offset    += chunk_size;
            }
            break;
        }
        default:
            UNREACHABLE;
            abort();
    }
}                                      /*}}} */

static inline void PtlInternalPerformDelivery2(const uint_fast8_t              type,
                                               void *const restrict            local_data,
                                               uint8_t *const restrict         message_data,
                                               const size_t                    nbytes,
                                               ptl_internal_header_t *restrict hdr,
                                               uint8_t *const restrict         op)
{                                      /*{{{ */
    switch (type & HDR_TYPE_BASICMASK) {
        case HDR_TYPE_PUT:
            memcpy(local_data, message_data, nbytes);
            break;
        case HDR_TYPE_GET:
            memcpy(message_data, local_data, nbytes);
            break;
        case HDR_TYPE_ATOMIC:
        case HDR_TYPE_FETCHATOMIC:
            PtlInternalPerformAtomic(local_data, message_data, nbytes,
                                     (ptl_op_t)hdr->atomic_operation,
                                     (ptl_datatype_t)hdr->atomic_datatype);
            break;
        case HDR_TYPE_SWAP:
            PtlInternalPerformAtomicArg(local_data,
                                        message_data,
                                        op, nbytes,
                                        (ptl_op_t)hdr->atomic_operation,
                                        (ptl_datatype_t)hdr->atomic_datatype);
            break;
        default:
            UNREACHABLE;
            abort();
    }
}                                      /*}}} */

static void PtlInternalPerformDelivery(const uint_fast8_t              type,
                                       void *const restrict            local_data,
                                       uint8_t *const restrict         message_data,
                                       const size_t                    nbytes,
                                       ptl_internal_header_t *restrict hdr)
{                                      /*{{{ */
    switch (type & HDR_TYPE_BASICMASK) {
        case HDR_TYPE_PUT:
        case HDR_TYPE_ATOMIC:
        case HDR_TYPE_FETCHATOMIC:
        case HDR_TYPE_GET:
            PtlInternalPerformDelivery2(type, local_data, message_data,
                                        nbytes, hdr, NULL);
            break;
        case HDR_TYPE_SWAP:
            PtlInternalPerformDelivery2(type, local_data, message_data + 32,
                                        nbytes, hdr, message_data);
            break;
        default:
            UNREACHABLE;
            abort();
    }
}                                      /*}}} */

#else /* ifdef USE_TRANSFER_ENGINE */
static void PtlInternalPerformDelivery(const uint_fast8_t              type,
                                       void *const restrict            local_data,
                                       uint8_t *const restrict         message_data,
                                       const size_t                    nbytes,
                                       ptl_internal_header_t *restrict hdr)
{   /*{{{*/
    switch (type & HDR_TYPE_BASICMASK) {
        case HDR_TYPE_PUT:
            memcpy(local_data, message_data, nbytes);
            break;
        case HDR_TYPE_ATOMIC:
        case HDR_TYPE_FETCHATOMIC:
            PtlInternalPerformAtomic(local_data,
                                     message_data,
                                     nbytes,
                                     (ptl_op_t)hdr->atomic_operation,
                                     (ptl_datatype_t)hdr->atomic_datatype);
            break;
        case HDR_TYPE_GET:
            memcpy(message_data, local_data, nbytes);
            break;
        case HDR_TYPE_SWAP:
            PtlInternalPerformAtomicArg(local_data,
                                        message_data + 32,
                                        message_data,
                                        nbytes,
                                        (ptl_op_t)hdr->atomic_operation,
                                        (ptl_datatype_t)hdr->atomic_datatype);
            break;
        default:
            UNREACHABLE;
            abort();
    }
} /*}}}*/

#endif /* ifdef USE_TRANSFER_ENGINE */

static void PtlInternalAnnounceLEDelivery(const ptl_handle_eq_t                 eq_handle,
                                          const ptl_handle_ct_t                 ct_handle,
                                          const uint_fast8_t                    type,
                                          const unsigned int                    options,
                                          const uint_fast64_t                   mlength,
                                          const uintptr_t                       start,
                                          const uint_fast8_t                    overflow,
                                          void *const                           user_ptr,
                                          ptl_internal_header_t *const restrict hdr)
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
            PtlInternalCTSuccessInc(ct_handle, 1);
        } else {
            PtlInternalCTSuccessInc(ct_handle, mlength);
        }
        if (PtlInternalAmITheCatcher()) {
            PtlInternalCTPullTriggers(ct_handle);
        } else {
            PtlInternalCTTriggerCheck(ct_handle);
        }
    }
    if (eq_handle != PTL_EQ_NONE) {
        if (((overflow) && ((options & PTL_LE_EVENT_OVER_DISABLE) == 0)) ||
            ((!overflow) && ((options & (PTL_LE_EVENT_COMM_DISABLE | PTL_LE_EVENT_SUCCESS_DISABLE)) == 0))) {
            ptl_internal_event_t e;
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
            e.start   = (void *)start;
            PtlInternalEQPush(eq_handle, &e);
        }
    }
}                                      /*}}} */

/* vim:set expandtab: */
