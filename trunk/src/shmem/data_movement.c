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
#include "ptl_internal_assert.h"
#include "ptl_internal_queues.h"
#include "ptl_internal_DM.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_pid.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_fragments.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_LE.h"
#include "ptl_internal_ME.h"
#include "ptl_internal_papi.h"
#ifndef NO_ARG_VALIDATION
# include "ptl_internal_error.h"
#endif

static uint32_t  spawned;
static pthread_t catcher;

#if 0
# define dm_printf(format, ...) printf("%u ~> " format,           \
                                       (unsigned int)proc_number, \
                                       ## __VA_ARGS__)
#else
# define dm_printf(format, ...)
#endif

#if 0
# define ack_printf(format, ...) printf("%u +> " format,           \
                                        (unsigned int)proc_number, \
                                        ## __VA_ARGS__)
#else
# define ack_printf(format, ...)
#endif

static void PtlInternalHandleCmd(ptl_internal_header_t *restrict hdr)
{   /*{{{*/
    /* Command packets are always from the local process */
    switch(hdr->pt_index) { // this is the selector of what kind of command packet
        case CMD_TYPE_CTFREE:
            PtlInternalCTFree(hdr);
            break;
        case CMD_TYPE_CHECK:
            PtlInternalCTPullTriggers(hdr->hdr_data);
            break;
        case CMD_TYPE_ENQUEUE:
            PtlInternalCTUnorderedEnqueue(hdr);
            break;
        default:
            abort();
    }
    /* now, put the fragment back in the freelist */
    PtlInternalFragmentFree(hdr);
} /*}}}*/

static void PtlInternalHandleAck(ptl_internal_header_t *restrict hdr)
{                                      /*{{{ */
    ptl_md_t           *mdptr     = NULL;
    ptl_handle_md_t     md_handle = PTL_INVALID_HANDLE;
    ptl_handle_eq_t     md_eq     = PTL_EQ_NONE;
    ptl_handle_ct_t     md_ct     = PTL_CT_NONE;
    unsigned int        md_opts   = 0;
    int                 acktype;
    int                 truncated = hdr->type & HDR_TYPE_TRUNCFLAG;
    const unsigned char basictype = hdr->type & HDR_TYPE_BASICMASK;

    ack_printf("got an ACK (%p)\n", hdr);
    /* first, figure out what to do with the ack */
    switch (basictype) {
        case HDR_TYPE_PUT:
            ack_printf("it's an ACK for a PUT\n");
            md_handle = hdr->md_handle1.a;
            mdptr     = PtlInternalMDFetch(md_handle);
            if (hdr->moredata) {
                /* we only sent partial data; need to refill the fragment and toss it back */
                size_t payload =
                    PtlInternalFragmentSize(hdr) -
                    sizeof(ptl_internal_header_t);
                /* update the value of remaining... */
                if (payload > hdr->remaining) {
                    hdr->remaining = 0;
                } else {
                    hdr->remaining -= payload;
                }
                if (hdr->remaining > 0) {
                    /* there are more fragments to go */
                    if (hdr->remaining < payload) {
                        /* this will be the last fragment */
                        payload = hdr->remaining;
                    }
                    ack_printf("refilling with %i data\n", (int)payload);
                    memcpy(hdr->data, hdr->moredata, payload);
                    hdr->moredata += payload;
                    if (hdr->remaining == payload) {
                        /* this will be the last fragment */
                        /* announce that we're done with the send-buffer */
                        const ptl_handle_eq_t eqh     = mdptr->eq_handle;
                        const ptl_handle_ct_t cth     = mdptr->ct_handle;
                        const unsigned int    options = mdptr->options;
                        ack_printf("announce that we're done with the send-buffer\n");
                        PtlInternalMDCleared(md_handle);
                        if (options & PTL_MD_EVENT_CT_SEND) {
                            if ((options & PTL_MD_EVENT_CT_BYTES) == 0) {
                                PtlInternalCTSuccessInc(cth, 1);
                            } else {
                                PtlInternalCTSuccessInc(cth, hdr->length);
                            }
                            PtlInternalCTPullTriggers(cth);
                        }
                        if ((eqh != PTL_EQ_NONE) &&
                            ((options & PTL_MD_EVENT_SUCCESS_DISABLE) ==
                             0)) {
                            PtlInternalEQPushESEND(eqh, hdr->length,
                                                   hdr->local_offset1,
                                                   hdr->user_ptr);
                        }
                    }
                    hdr->src = proc_number;
                    PtlInternalFragmentToss(hdr, hdr->target);
                    return;            // do not free fragment, because we just sent it
                } else if (truncated) {
                    /* announce that we're done with the send-buffer */
                    const ptl_handle_eq_t eqh     = mdptr->eq_handle;
                    const ptl_handle_ct_t cth     = mdptr->ct_handle;
                    const unsigned int    options = mdptr->options;
                    ack_printf
                        ("announce that we're done with the send-buffer\n");
                    PtlInternalMDCleared(md_handle);
                    if (options & PTL_MD_EVENT_CT_SEND) {
                        if ((options & PTL_MD_EVENT_CT_BYTES) == 0) {
                            PtlInternalCTSuccessInc(cth, 1);
                        } else {
                            PtlInternalCTSuccessInc(cth, hdr->length);
                        }
                        PtlInternalCTPullTriggers(cth);
                    }
                    if ((eqh != PTL_EQ_NONE) &&
                        ((options & PTL_MD_EVENT_SUCCESS_DISABLE) == 0)) {
                        PtlInternalEQPushESEND(eqh, hdr->length,
                                               hdr->
                                               local_offset1, hdr->user_ptr);
                    }
                }
            }
            break;
        case HDR_TYPE_GET:
            ack_printf("it's an ACK for a GET\n");
            md_handle = hdr->md_handle1.a;
            mdptr     = PtlInternalMDFetch(md_handle);
            if (hdr->moredata) {
                /* this ack contains partial data */
                size_t reply_size =
                    PtlInternalFragmentSize(hdr) -
                    sizeof(ptl_internal_header_t);
                if (reply_size > hdr->remaining) {
                    reply_size = hdr->remaining;
                }
                ack_printf("replied with %i data\n", (int)reply_size);
                /* pull out the partial data */
                memcpy(hdr->moredata, hdr->data, reply_size);
                hdr->moredata  += reply_size;
                hdr->remaining -= reply_size;
                if (hdr->remaining > 0) {
                    hdr->src = proc_number;
                    PtlInternalFragmentToss(hdr, hdr->target);
                    return;            // do not free fragment, because we just sent it
                }
            } else {
                /* pull the data out of the reply */
                ack_printf("replied with %i data\n", (int)hdr->length);
                if ((mdptr != NULL) &&
                    ((hdr->src == 1) || (hdr->src == 0))) {
                    memcpy((uint8_t *)(mdptr->start) +
                           hdr->local_offset1, hdr->data,
                           hdr->length);
                }
            }
            break;
        case HDR_TYPE_ATOMIC:
            ack_printf("it's an ACK for an ATOMIC\n");
            if (truncated) {
                fprintf(stderr,
                        "PORTALS4-> truncated ATOMICs not yet supported\n");
                abort();
            }
            md_handle = hdr->md_handle1.a;
            mdptr     = PtlInternalMDFetch(md_handle);
            break;
        case HDR_TYPE_FETCHATOMIC:
            ack_printf("it's an ACK for a FETCHATOMIC\n");
            if (truncated) {
                fprintf(stderr, "PORTALS4-> truncated FETCHATOMICs not yet supported\n");
                abort();
            }
            md_handle = hdr->md_handle1.a;
            if ((hdr->md_handle2.a !=
                 PTL_INVALID_HANDLE) &&
                (hdr->md_handle2.a != md_handle)) {
                PtlInternalMDCleared(hdr->md_handle2.a);
            }
            mdptr = PtlInternalMDFetch(md_handle);
            /* pull the data out of the reply */
            ack_printf("replied with %i data\n", (int)hdr->length);
            if ((mdptr != NULL) && ((hdr->src == 1) || (hdr->src == 0))) {
                memcpy((uint8_t *)(mdptr->start) +
                       hdr->local_offset1,
                       hdr->data, hdr->length);
            }
            break;
        case HDR_TYPE_SWAP:
            ack_printf("it's an ACK for a SWAP\n");
            if (truncated) {
                fprintf(stderr,
                        "PORTALS4-> truncated SWAPs not yet supported\n");
                abort();
            }
            md_handle = hdr->md_handle1.a;
            if ((hdr->md_handle2.a != PTL_INVALID_HANDLE) &&
                (hdr->md_handle2.a != md_handle)) {
                PtlInternalMDCleared(hdr->md_handle2.a);
            }
            mdptr = PtlInternalMDFetch(md_handle);
            /* pull the data out of the reply */
            ack_printf("replied with %i data\n", (int)hdr->length);
            if ((mdptr != NULL) && ((hdr->src == 1) || (hdr->src == 0))) {
                memcpy((uint8_t *)(mdptr->start) +
                       hdr->local_offset1,
                       hdr->data + 32, hdr->length);
            }
            break;
        default:                      // impossible
            UNREACHABLE;
            *(int *)0 = 0;
    }
    /* allow the MD to be deallocated (happens *before* notifying listeners that we're done, to avoid race conditions) */
    if (mdptr != NULL) {
        md_eq   = mdptr->eq_handle;
        md_ct   = mdptr->ct_handle;
        md_opts = mdptr->options;
    }
    switch (basictype) {
        case HDR_TYPE_GET:
        case HDR_TYPE_FETCHATOMIC:
        case HDR_TYPE_SWAP:
            if ((mdptr != NULL) && (md_handle != PTL_INVALID_HANDLE)) {
                ack_printf("clearing ACK's md_handle\n");
                PtlInternalMDCleared(md_handle);
            }
    }
    /* determine desired acktype */
    acktype = 2;                       // any acktype
    switch (basictype) {
        case HDR_TYPE_PUT:
        case HDR_TYPE_ATOMIC:
            switch (hdr->ack_req) {
                case PTL_ACK_REQ:
                    acktype = 2;
                    break;
                case PTL_NO_ACK_REQ:
                    hdr->src = 0;
                    break;
                case PTL_CT_ACK_REQ:
                case PTL_OC_ACK_REQ:
                    acktype = 1;
            }
    }
    /* Report the ack */
    switch (hdr->src) {
        case 0:                       // Pretend we didn't recieve an ack
            ack_printf("it's a secret ACK\n");
            break;
        case 1:                       // success
        case 2:                       // overflow
            ack_printf("it's a successful/overflow ACK (%p)\n", mdptr);
            if (mdptr != NULL) {
                int ct_enabled = 0;
                switch(basictype) {
                    case HDR_TYPE_PUT: case HDR_TYPE_ATOMIC:
                        if (acktype != 0) {
                            ct_enabled = md_opts & PTL_MD_EVENT_CT_ACK;
                        }
                        break;
                    default:
                        ct_enabled = md_opts & PTL_MD_EVENT_CT_REPLY;
                }
                if ((md_ct != PTL_CT_NONE) && (ct_enabled != 0)) {
                    if ((md_opts & PTL_MD_EVENT_CT_BYTES) == 0) {
                        PtlInternalCTSuccessInc(md_ct, 1);
                    } else {
                        PtlInternalCTSuccessInc(md_ct, hdr->length);
                    }
                    PtlInternalCTPullTriggers(md_ct);
                }
                if ((md_eq != PTL_EQ_NONE) &&
                    ((md_opts & PTL_MD_EVENT_SUCCESS_DISABLE) == 0) &&
                    (acktype > 1)) {
                    ptl_internal_event_t e;
                    switch (basictype) {
                        case HDR_TYPE_PUT:
                        case HDR_TYPE_ATOMIC:
                            e.type = PTL_EVENT_ACK;
                            break;
                        default:
                            e.type = PTL_EVENT_REPLY;
                            break;
                    }
                    e.mlength       = hdr->length;
                    e.remote_offset = hdr->dest_offset;
                    e.user_ptr      = hdr->user_ptr;
                    e.ni_fail_type  = PTL_NI_OK;
                    PtlInternalEQPush(md_eq, &e);
                }
            }
            ack_printf("finished notification of successful ACK\n");
            break;
        case 3:                       // Permission Violation
            ack_printf("ACK says permission violation\n");
        // goto reporterror;
        case 4:                       // nothing matched, report error
            // reporterror:
            ack_printf("ACK says nothing matched!\n");
            if (mdptr != NULL) {
                int ct_enabled = 0;
                switch (basictype) {
                    case HDR_TYPE_PUT:
                        ct_enabled = md_opts & PTL_MD_EVENT_CT_ACK;
                        break;
                    default:
                        ct_enabled = md_opts & PTL_MD_EVENT_CT_REPLY;
                        break;
                }
                if (ct_enabled) {
                    if (md_ct != PTL_CT_NONE) {
                        PtlInternalCTFailureInc(md_ct);
                    } else {
                        /* this should never happen */
                        fprintf(stderr, "enabled CT counting, but no CT!\n");
                        abort();
                    }
                }
                if (md_eq != PTL_EQ_NONE) {
                    ptl_internal_event_t e;
                    e.type          = PTL_EVENT_ACK;
                    e.mlength       = hdr->length;
                    e.remote_offset = hdr->dest_offset;
                    e.user_ptr      = hdr->user_ptr;
                    switch (hdr->src) {
                        case 3:
                            e.ni_fail_type = PTL_NI_PERM_VIOLATION;
                            break;
                        default:
                            e.ni_fail_type = PTL_NI_OK;
                    }
                    PtlInternalEQPush(md_eq, &e);
                }
            }
            break;
    }
    ack_printf("freeing fragment (%p)\n", hdr);
    /* now, put the fragment back in the freelist */
    PtlInternalFragmentFree(hdr);
    ack_printf("freed\n");
}                                      /*}}} */

static void *PtlInternalDMCatcher(void *__attribute__ ((unused)) junk)
Q_NORETURN
{                                      /*{{{ */
    while (1) {
        ptl_pid_t              src;
        ptl_internal_header_t *hdr = PtlInternalFragmentReceive();
        assert(hdr != NULL);
        if (hdr->type == HDR_TYPE_TERM) { // TERMINATE!
            dm_printf("termination command received in DMCatcher!\n");
            return NULL;
        }
        if (hdr->type & HDR_TYPE_ACKFLAG) {
            hdr->type &= HDR_TYPE_ACKMASK;
            PtlInternalHandleAck(hdr);
            continue;
        }
        if (hdr->type == HDR_TYPE_CMD) {
            PtlInternalHandleCmd(hdr);
            continue;
        }
        dm_printf("got a header! %p points to ni %i\n", hdr, hdr->ni);
        src = hdr->src;
        assert(hdr->target == proc_number);
        assert(nit.tables != NULL);
        PtlInternalAtomicInc(&nit.internal_refcount[hdr->ni], 1);
        if (nit.tables[hdr->ni] != NULL) {
            ptl_table_entry_t *table_entry =
                &(nit.tables[hdr->ni][hdr->pt_index]);
            PTL_LOCK_LOCK(table_entry->lock);
            if (table_entry->status == 1) {
                switch (PtlInternalPTValidate(table_entry)) {
                    case 1:           // uninitialized
                        fprintf(stderr, "PORTALS4-> rank %u sent to an uninitialized PT! (%u)\n",
                                (unsigned)src, (unsigned)hdr->pt_index);
                        abort();
                        break;
                    case 2:           // disabled
                        fprintf(stderr, "PORTALS4-> rank %u sent to a disabled PT! (%u)\n",
                                (unsigned)src, (unsigned)hdr->pt_index);
                        abort();
                        break;
                }
                if (hdr->type == HDR_TYPE_PUT) {
                    dm_printf("received NI = %u, pt_index = %u, PUT hdr_data = %u -> priority=%p, overflow=%p\n",
                              (unsigned int)hdr->ni, hdr->pt_index,
                              (unsigned)hdr->hdr_data,
                              table_entry->priority.head,
                              table_entry->overflow.head);
                } else {
                    dm_printf("received NI = %u, pt_index = %u, hdr_type = %u -> priority=%p, overflow=%p\n",
                              (unsigned int)hdr->ni, hdr->pt_index,
                              (unsigned)hdr->type, table_entry->priority.head,
                              table_entry->overflow.head);
                }
                switch (hdr->ni) {
                    case 0:
                    case 2:           // Matching (ME)
                        dm_printf("delivering to ME table\n");
                        /* this will unlock the table_entry */
                        hdr->src = PtlInternalMEDeliver(table_entry, hdr);
                        break;
                    case 1:
                    case 3:           // Non-matching (LE)
                        dm_printf("delivering to LE table\n");
                        /* this will unlock the table_entry */
                        hdr->src = PtlInternalLEDeliver(table_entry, hdr);
                        break;
                }
                switch (hdr->src) {
                    case 0:           // target said silent ACK (might be no ME posted)
                        dm_printf("not sending an ack\n");
                        break;
                    case 1:           // success
                        dm_printf("delivery success!\n");
                        break;
                    case 2:           // overflow
                        dm_printf("delivery overflow!\n");
                        break;
                    case 3:           // Permission Violation
                        dm_printf("permission violation!\n");
                        hdr->length = 0;
                        break;
                }
            } else {
                /* Invalid PT: increment the dropped counter */
                (void)
                PtlInternalAtomicInc(&nit.regs[hdr->ni]
                                     [PTL_SR_DROP_COUNT], 1);
#ifdef LOUD_DROPS
                fprintf(stderr, "PORTALS4-> Rank %u dropped a message from rank %u sent to an invalid PT (%u) on NI %u\n",
                        (unsigned)proc_number, (unsigned)hdr->src,
                        (unsigned)hdr->pt_index, (unsigned)hdr->ni);
                fflush(stderr);
#endif
                /* silently ACK */
                hdr->src = 0;
                dm_printf("table_entry->status == 0 ... unlocking\n");
                PTL_LOCK_UNLOCK(table_entry->lock);
            }
        } else {                       // uninitialized NI
#ifdef LOUD_DROPS
            fprintf(stderr, "PORTALS4-> Rank %u dropped a message from rank %u sent to an uninitialized NI %u\n",
                    (unsigned)proc_number, (unsigned)hdr->src,
                    (unsigned)hdr->ni);
            fflush(stderr);
#endif
            hdr->src = 0;              // silent ACK
        }
        PtlInternalAtomicInc(&nit.internal_refcount[hdr->ni], -1);
        dm_printf("returning fragment\n");
        /* Now, return the fragment to the sender */
        hdr->type |= HDR_TYPE_ACKFLAG; // mark it as an ack
        PtlInternalFragmentToss(hdr, src);
        dm_printf("back to the beginning\n");
    }
}                                      /*}}} */

void INTERNAL PtlInternalDMSetup(void)
{                                      /*{{{ */
    if (PtlInternalAtomicInc(&spawned, 1) == 0) {
        ptl_assert(pthread_create(&catcher, NULL, PtlInternalDMCatcher, NULL),
                   0);
    }
}                                      /*}}} */

void PtlInternalDMStop(void)
{                                      /*{{{ */
    static int already_stopped = 0;

    if (already_stopped == 0) {
        already_stopped = 1;
        ptl_internal_header_t *restrict hdr;
        /* Using a termination sigil, rather than pthread_cancel(), so that the queues are always left in a valid/useable state (e.g. unlocked), so that late sends and late acks don't cause hangs. */
        hdr       = PtlInternalFragmentFetch(sizeof(ptl_internal_header_t));
        hdr->type = HDR_TYPE_TERM;
        PtlInternalFragmentToss(hdr, proc_number);
    }
}                                      /*}}} */

void INTERNAL PtlInternalDMTeardown(void)
{                                      /*{{{ */
    if (PtlInternalAtomicInc(&spawned, -1) == 1) {
        PtlInternalDMStop();
        ptl_assert(pthread_join(catcher, NULL), 0);
    }
}                                      /*}}} */

int API_FUNC PtlPut(ptl_handle_md_t  md_handle,
                    ptl_size_t       local_offset,
                    ptl_size_t       length,
                    ptl_ack_req_t    ack_req,
                    ptl_process_t    target_id,
                    ptl_pt_index_t   pt_index,
                    ptl_match_bits_t match_bits,
                    ptl_size_t       remote_offset,
                    void            *user_ptr,
                    ptl_hdr_data_t   hdr_data)
{                                      /*{{{ */
    ptl_internal_header_t *restrict       hdr;
    int                                   quick_exit = 0;
    const ptl_internal_handle_converter_t md         = { md_handle };

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(md_handle) < local_offset + length) {
        VERBOSE_ERROR("MD too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(md_handle), local_offset + length);
        return PTL_ARG_INVALID;
    }
    switch (md.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            if (PtlInternalLogicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            if (PtlInternalPhysicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (pid=%u, nid=%u)\n",
                              (unsigned int)target_id.phys.pid,
                              (unsigned int)target_id.phys.nid);
                return PTL_ARG_INVALID;
            }
            break;
    }
    {
        ptl_md_t *mdptr;
        mdptr = PtlInternalMDFetch(md_handle);
        if ((mdptr->options & PTL_MD_VOLATILE) && (length > nit_limits[md.s.ni].max_volatile_size)) {
            VERBOSE_ERROR("asking for too big a send (%u bytes) from an MD marked VOLATILE (max %u bytes)\n",
                          length, nit_limits[md.s.ni].max_volatile_size);
            return PTL_ARG_INVALID;
        }
    }
    if (pt_index > nit_limits[md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (local_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    PtlInternalPAPIStartC();
    PtlInternalMDPosted(md_handle);
    /* step 1: get a local memory fragment */
    hdr = PtlInternalFragmentFetch(sizeof(ptl_internal_header_t) + length);
    // printf("got fragment %p, commpad = %p\n", hdr, comm_pad);
    /* step 2: fill the op structure */
    hdr->type = HDR_TYPE_PUT;
    hdr->ni   = md.s.ni;
    // printf("hdr->NI = %u, md.s.ni = %u\n", (unsigned int)hdr->ni, (unsigned int)md.s.ni);
    hdr->src = proc_number;
    switch (md.s.ni) {
        case 0: case 1: // Logical
            hdr->target = target_id.rank;
            break;
        case 2: case 3: // Physical
            hdr->target = target_id.phys.pid;
            break;
    }
    hdr->pt_index      = pt_index;
    hdr->match_bits    = match_bits;
    hdr->dest_offset   = remote_offset;
    hdr->length        = length;
    hdr->user_ptr      = user_ptr;
    hdr->entry         = NULL;
    hdr->md_handle1.a  = md_handle;
    hdr->local_offset1 = local_offset;
    hdr->remaining     = length;
    hdr->hdr_data      = hdr_data;
#ifdef STRICT_UID_JID
    {
        extern ptl_jid_t the_ptl_jid;
        hdr->jid = the_ptl_jid;
    }
#endif
    hdr->ack_req = ack_req;
    char *dataptr = PtlInternalMDDataPtr(md_handle) + local_offset;
    // PtlInternalPAPISaveC(PTL_PUT, 0);
    /* step 3: load up the data */
    if (PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t) >=
        length) {
        memcpy(hdr->data, dataptr, length);
        hdr->moredata = NULL;
        quick_exit    = 1;
    } else {
        size_t payload =
            PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t);
        memcpy(hdr->data, dataptr, payload);
        hdr->moredata = dataptr + payload;
    }
    /* step 4: enqueue the op structure on the target */
    PtlInternalFragmentToss(hdr, hdr->target);
    // PtlInternalPAPISaveC(PTL_PUT, 1);
    if (quick_exit) {
        unsigned int    options;
        ptl_handle_eq_t eqh;
        ptl_handle_ct_t cth;
        /* the send is completed immediately */
        {
            ptl_md_t *mdptr;
            mdptr   = PtlInternalMDFetch(md_handle);
            options = mdptr->options;
            eqh     = mdptr->eq_handle;
            cth     = mdptr->ct_handle;
        }
        /* allow the MD to be deleted */
        PtlInternalMDCleared(md_handle);
        /* step 5: report the send event */
        if (options & PTL_MD_EVENT_CT_SEND) {
            // printf("%u PtlPut incrementing ct %u (SEND)\n", (unsigned)proc_number, cth);
            if ((options & PTL_MD_EVENT_CT_BYTES) == 0) {
                PtlInternalCTSuccessInc(cth, 1);
            } else {
                PtlInternalCTSuccessInc(cth, length);
            }
            PtlInternalCTTriggerCheck(cth);
        } else {
            // printf("%u PtlPut NOT incrementing ct\n", (unsigned)proc_number);
        }
        if ((eqh != PTL_EQ_NONE) && ((options & PTL_MD_EVENT_SUCCESS_DISABLE)
                                     == 0)) {
            PtlInternalEQPushESEND(eqh, length, local_offset, user_ptr);
        }
    }
    PtlInternalPAPIDoneC(PTL_PUT, 2);
    return PTL_OK;
}                                      /*}}} */

int API_FUNC PtlGet(ptl_handle_md_t  md_handle,
                    ptl_size_t       local_offset,
                    ptl_size_t       length,
                    ptl_process_t    target_id,
                    ptl_pt_index_t   pt_index,
                    ptl_match_bits_t match_bits,
                    ptl_size_t       remote_offset,
                    void            *user_ptr)
{                                      /*{{{ */
    const ptl_internal_handle_converter_t md = { md_handle };
    ptl_internal_header_t                *hdr;

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(md_handle) < local_offset + length) {
        VERBOSE_ERROR("MD too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(md_handle), local_offset + length);
        return PTL_ARG_INVALID;
    }
    switch (md.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            if (PtlInternalLogicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            if (PtlInternalPhysicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (pid=%u, nid=%u)\n",
                              (unsigned int)target_id.phys.pid,
                              (unsigned int)target_id.phys.nid);
                return PTL_ARG_INVALID;
            }
            break;
    }
    if (pt_index > nit_limits[md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (local_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    PtlInternalMDPosted(md_handle);
    /* step 1: get a local memory fragment */
    hdr = PtlInternalFragmentFetch(sizeof(ptl_internal_header_t) + length);
    /* step 2: fill the op structure */
    hdr->type = HDR_TYPE_GET;
    hdr->ni   = md.s.ni;
    hdr->src  = proc_number;
    switch (md.s.ni) {
        case 0: case 1: // Logical
            hdr->target = target_id.rank;
            break;
        case 2: case 3: // Physical
            hdr->target = target_id.phys.pid;
            break;
    }
#ifdef STRICT_UID_JID
    {
        extern ptl_jid_t the_ptl_jid;
        hdr->jid = the_ptl_jid;
    }
#endif
    hdr->pt_index      = pt_index;
    hdr->match_bits    = match_bits;
    hdr->dest_offset   = remote_offset;
    hdr->length        = length;
    hdr->user_ptr      = user_ptr;
    hdr->entry         = NULL;
    hdr->md_handle1.a  = md_handle;
    hdr->local_offset1 = local_offset;
    hdr->entry         = NULL;
    hdr->remaining     = length;
    if (PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t) >=
        length) {
        hdr->moredata = NULL;
    } else {
        hdr->moredata =
            PtlInternalMDDataPtr(md_handle) + local_offset;
    }
    /* step 3: enqueue the op structure on the target */
    switch (md.s.ni) {
        case 0:
        case 1:                       // Logical
            PtlInternalFragmentToss(hdr, target_id.rank);
            break;
        case 2:
        case 3:                       // Physical
            PtlInternalFragmentToss(hdr, target_id.phys.pid);
            break;
    }
    /* no send event to report */
    return PTL_OK;
}                                      /*}}} */

int API_FUNC PtlAtomic(ptl_handle_md_t  md_handle,
                       ptl_size_t       local_offset,
                       ptl_size_t       length,
                       ptl_ack_req_t    ack_req,
                       ptl_process_t    target_id,
                       ptl_pt_index_t   pt_index,
                       ptl_match_bits_t match_bits,
                       ptl_size_t       remote_offset,
                       void            *user_ptr,
                       ptl_hdr_data_t   hdr_data,
                       ptl_op_t         operation,
                       ptl_datatype_t   datatype)
{                                      /*{{{ */
    ptl_internal_header_t                *hdr;
    ptl_md_t                             *mdptr;
    const ptl_internal_handle_converter_t md = { md_handle };

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (length > nit_limits[md.s.ni].max_atomic_size) {
        VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                      (unsigned int)length,
                      (unsigned int)nit_limits[md.s.ni].max_atomic_size);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(md_handle, 1)) {
        VERBOSE_ERROR("Invalid MD\n");
        return PTL_ARG_INVALID;
    }
    {
        ptl_md_t *mdptr;
        mdptr = PtlInternalMDFetch(md_handle);
        if ((mdptr->options & PTL_MD_VOLATILE) && (length > nit_limits[md.s.ni].max_volatile_size)) {
            VERBOSE_ERROR("asking for too big a send (%u bytes) from an MD marked VOLATILE (max %u bytes)\n",
                          length, nit_limits[md.s.ni].max_volatile_size);
            return PTL_ARG_INVALID;
        }
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_CHAR:
            case PTL_UCHAR:
                multiple = 1;
                break;
            case PTL_SHORT:
            case PTL_USHORT:
                multiple = 2;
                break;
            case PTL_INT:
            case PTL_UINT:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_LONG:
            case PTL_ULONG:
            case PTL_DOUBLE:
            case PTL_FLOAT_COMPLEX:
                multiple = 8;
                break;
            case PTL_LONG_DOUBLE:
            case PTL_DOUBLE_COMPLEX:
                multiple = 16;
                break;
            case PTL_LONG_DOUBLE_COMPLEX:
                multiple = 32;
                break;
        }
        if (length % multiple != 0) {
            VERBOSE_ERROR("Length not a multiple of datatype size\n");
            return PTL_ARG_INVALID;
        }
    }
    if (PtlInternalMDLength(md_handle) < local_offset + length) {
        VERBOSE_ERROR("MD too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(md_handle), local_offset + length);
        return PTL_ARG_INVALID;
    }
    switch (md.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            if (PtlInternalLogicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            if (PtlInternalPhysicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (pid=%u, nid=%u)\n",
                              (unsigned int)target_id.phys.pid,
                              (unsigned int)target_id.phys.nid);
                return PTL_ARG_INVALID;
            }
            break;
    }
    switch (operation) {
        case PTL_SWAP:
        case PTL_CSWAP:
        case PTL_MSWAP:
            VERBOSE_ERROR("SWAP/CSWAP/MSWAP invalid optypes for PtlAtomic()\n");
            return PTL_ARG_INVALID;

        case PTL_LOR:
        case PTL_LAND:
        case PTL_LXOR:
        case PTL_BOR:
        case PTL_BAND:
        case PTL_BXOR:
            switch (datatype) {
                case PTL_DOUBLE:
                case PTL_FLOAT:
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes for logical/binary operations\n");
                    return PTL_ARG_INVALID;

                default:
                    break;
            }
        default:
            break;
    }
    if (pt_index > nit_limits[md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (local_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    PtlInternalMDPosted(md_handle);
    /* step 1: get a local memory fragment */
    hdr = PtlInternalFragmentFetch(sizeof(ptl_internal_header_t) + length);
    /* step 2: fill the op structure */
    hdr->type = HDR_TYPE_ATOMIC;
    hdr->ni   = md.s.ni;
    hdr->src  = proc_number;
    switch (md.s.ni) {
        case 0: case 1: // Logical
            hdr->target = target_id.rank;
            break;
        case 2: case 3: // Physical
            hdr->target = target_id.phys.pid;
            break;
    }
#ifdef STRICT_UID_JID
    {
        extern ptl_jid_t the_ptl_jid;
        hdr->jid = the_ptl_jid;
    }
#endif
    hdr->pt_index         = pt_index;
    hdr->match_bits       = match_bits;
    hdr->dest_offset      = remote_offset;
    hdr->length           = length;
    hdr->user_ptr         = user_ptr;
    hdr->entry            = NULL;
    hdr->md_handle1.a     = md_handle;
    hdr->hdr_data         = hdr_data;
    hdr->ack_req          = ack_req;
    hdr->atomic_operation = operation;
    hdr->atomic_datatype  = datatype;
    hdr->remaining        = length;
    /* step 3: load up the data */
    char *dataptr = PtlInternalMDDataPtr(md_handle) + local_offset;
    if (PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t) >=
        length) {
        memcpy(hdr->data, dataptr, length);
        hdr->moredata = NULL;
    } else {
        size_t payload =
            PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t);
        memcpy(hdr->data, dataptr, payload);
        hdr->moredata = dataptr + payload;
    }
    /* step 4: enqueue the op structure on the target */
    switch (md.s.ni) {
        case 0:
        case 1:                       // Logical
            PtlInternalFragmentToss(hdr, target_id.rank);
            break;
        case 2:
        case 3:                       // Physical
            PtlInternalFragmentToss(hdr, target_id.phys.pid);
            break;
    }
    /* the send is completed immediately */
    /* allow the MD to be deleted */
    PtlInternalMDCleared(md_handle);
    /* step 5: report the send event */
    mdptr = PtlInternalMDFetch(md_handle);
    if (mdptr->options & PTL_MD_EVENT_CT_SEND) {
        if ((mdptr->options & PTL_MD_EVENT_CT_BYTES) == 0) {
            PtlInternalCTSuccessInc(mdptr->ct_handle, 1);
        } else {
            PtlInternalCTSuccessInc(mdptr->ct_handle, length);
        }
        PtlInternalCTTriggerCheck(mdptr->ct_handle);
    }
    if ((mdptr->eq_handle != PTL_EQ_NONE) &&
        ((mdptr->options & PTL_MD_EVENT_SUCCESS_DISABLE) == 0)) {
        PtlInternalEQPushESEND(mdptr->eq_handle, length, local_offset,
                               user_ptr);
    }
    return PTL_OK;
}                                      /*}}} */

int API_FUNC PtlFetchAtomic(ptl_handle_md_t  get_md_handle,
                            ptl_size_t       local_get_offset,
                            ptl_handle_md_t  put_md_handle,
                            ptl_size_t       local_put_offset,
                            ptl_size_t       length,
                            ptl_process_t    target_id,
                            ptl_pt_index_t   pt_index,
                            ptl_match_bits_t match_bits,
                            ptl_size_t       remote_offset,
                            void            *user_ptr,
                            ptl_hdr_data_t   hdr_data,
                            ptl_op_t         operation,
                            ptl_datatype_t   datatype)
{                                      /*{{{ */
    ptl_internal_header_t                *hdr;
    ptl_md_t                             *mdptr;
    const ptl_internal_handle_converter_t get_md = { get_md_handle };
    const ptl_internal_handle_converter_t put_md = { put_md_handle };

#ifndef NO_ARG_VALIDATION
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(get_md_handle, 1)) {
        VERBOSE_ERROR("Invalid get_md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(put_md_handle, 1)) {
        VERBOSE_ERROR("Invalid put_md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (length > nit_limits[get_md.s.ni].max_atomic_size) {
        VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                      (unsigned int)length,
                      (unsigned int)nit_limits[get_md.s.ni].max_atomic_size);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(get_md_handle) < local_get_offset + length) {
        VERBOSE_ERROR("FetchAtomic saw get_md too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(get_md_handle), local_get_offset + length);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(put_md_handle) < local_put_offset + length) {
        VERBOSE_ERROR("FetchAtomic saw put_md too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(put_md_handle), local_put_offset + length);
        return PTL_ARG_INVALID;
    }
    {
        ptl_md_t *mdptr;
        mdptr = PtlInternalMDFetch(put_md_handle);
        if ((mdptr->options & PTL_MD_VOLATILE) && (length > nit_limits[put_md.s.ni].max_volatile_size)) {
            VERBOSE_ERROR("asking for too big a send (%u bytes) from an MD marked VOLATILE (max %u bytes)\n",
                          length, nit_limits[put_md.s.ni].max_volatile_size);
            return PTL_ARG_INVALID;
        }
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_CHAR:
            case PTL_UCHAR:
                multiple = 1;
                break;
            case PTL_SHORT:
            case PTL_USHORT:
                multiple = 2;
                break;
            case PTL_INT:
            case PTL_UINT:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_LONG:
            case PTL_ULONG:
            case PTL_DOUBLE:
            case PTL_FLOAT_COMPLEX:
                multiple = 8;
                break;
            case PTL_LONG_DOUBLE:
            case PTL_DOUBLE_COMPLEX:
                multiple = 16;
                break;
            case PTL_LONG_DOUBLE_COMPLEX:
                multiple = 32;
                break;
        }
        if (length % multiple != 0) {
            VERBOSE_ERROR("Length not a multiple of datatype size\n");
            return PTL_ARG_INVALID;
        }
    }
    if (get_md.s.ni != put_md.s.ni) {
        VERBOSE_ERROR("MDs *must* be on the same NI\n");
        return PTL_ARG_INVALID;
    }
    switch (get_md.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            if (PtlInternalLogicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            if (PtlInternalPhysicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (pid=%u, nid=%u)\n",
                              (unsigned int)target_id.phys.pid,
                              (unsigned int)target_id.phys.nid);
                return PTL_ARG_INVALID;
            }
            break;
    }
    switch (operation) {
        case PTL_CSWAP:
        case PTL_MSWAP:
            VERBOSE_ERROR("MSWAP/CSWAP should be performed with PtlSwap\n");
            return PTL_ARG_INVALID;

        case PTL_LOR:
        case PTL_LAND:
        case PTL_LXOR:
        case PTL_BOR:
        case PTL_BAND:
        case PTL_BXOR:
            switch (datatype) {
                case PTL_DOUBLE:
                case PTL_FLOAT:
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes for logical/binary operations\n");
                    return PTL_ARG_INVALID;

                default:
                    break;
            }
        default:
            break;
    }
    if (pt_index > nit_limits[get_md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[get_md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (local_put_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (local_get_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    PtlInternalMDPosted(put_md_handle);
    if (get_md_handle != put_md_handle) {
        PtlInternalMDPosted(get_md_handle);
    }
    /* step 1: get a local memory fragment */
    hdr = PtlInternalFragmentFetch(sizeof(ptl_internal_header_t) + length);
    /* step 2: fill the op structure */
    hdr->type = HDR_TYPE_FETCHATOMIC;
    hdr->ni   = get_md.s.ni;
    hdr->src  = proc_number;
    switch (get_md.s.ni) {
        case 0: case 1: // Logical
            hdr->target = target_id.rank;
            break;
        case 2: case 3: // Physical
            hdr->target = target_id.phys.pid;
            break;
    }
#ifdef STRICT_UID_JID
    {
        extern ptl_jid_t the_ptl_jid;
        hdr->jid = the_ptl_jid;
    }
#endif
    hdr->pt_index         = pt_index;
    hdr->match_bits       = match_bits;
    hdr->dest_offset      = remote_offset;
    hdr->length           = length;
    hdr->user_ptr         = user_ptr;
    hdr->entry            = NULL;
    hdr->md_handle1.a     = get_md_handle;
    hdr->local_offset1    = local_get_offset;
    hdr->md_handle2.a     = put_md_handle;
    hdr->local_offset2    = local_put_offset;
    hdr->hdr_data         = hdr_data;
    hdr->atomic_operation = operation;
    hdr->atomic_datatype  = datatype;
    hdr->remaining        = length;
    /* step 3: load up the data */
    char *dataptr = PtlInternalMDDataPtr(put_md_handle) + local_put_offset;
    if (PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t) >=
        length) {
        memcpy(hdr->data, dataptr, length);
        hdr->moredata = NULL;
    } else {
        size_t payload =
            PtlInternalFragmentSize(hdr) - sizeof(ptl_internal_header_t);
        memcpy(hdr->data, dataptr, payload);
        hdr->moredata = dataptr + payload;
    }
    /* step 4: enqueue the op structure on the target */
    switch (put_md.s.ni) {
        case 0:
        case 1:                       // Logical
            PtlInternalFragmentToss(hdr, target_id.rank);
            break;
        case 2:
        case 3:                       // Physical
            PtlInternalFragmentToss(hdr, target_id.phys.pid);
            break;
    }
    /* step 5: report the send event */
    mdptr = PtlInternalMDFetch(put_md_handle);
    if (mdptr->options & PTL_MD_EVENT_CT_SEND) {
        if ((mdptr->options & PTL_MD_EVENT_CT_BYTES) == 0) {
            PtlInternalCTSuccessInc(mdptr->ct_handle, 1);
        } else {
            PtlInternalCTSuccessInc(mdptr->ct_handle, length);
        }
        PtlInternalCTTriggerCheck(mdptr->ct_handle);
    }
    if ((mdptr->eq_handle != PTL_EQ_NONE) &&
        ((mdptr->options & PTL_MD_EVENT_SUCCESS_DISABLE) == 0)) {
        PtlInternalEQPushESEND(mdptr->eq_handle, length, local_put_offset,
                               user_ptr);
    }
    return PTL_OK;
}                                      /*}}} */

int API_FUNC PtlSwap(ptl_handle_md_t  get_md_handle,
                     ptl_size_t       local_get_offset,
                     ptl_handle_md_t  put_md_handle,
                     ptl_size_t       local_put_offset,
                     ptl_size_t       length,
                     ptl_process_t    target_id,
                     ptl_pt_index_t   pt_index,
                     ptl_match_bits_t match_bits,
                     ptl_size_t       remote_offset,
                     void            *user_ptr,
                     ptl_hdr_data_t   hdr_data,
                     void            *operand,
                     ptl_op_t         operation,
                     ptl_datatype_t   datatype)
{                                      /*{{{ */
    const ptl_internal_handle_converter_t get_md = { get_md_handle };
    ptl_internal_header_t                *hdr;

#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t put_md = { put_md_handle };
    if (comm_pad == NULL) {
        return PTL_NO_INIT;
    }
    if (PtlInternalMDHandleValidator(get_md_handle, 1)) {
        VERBOSE_ERROR("Swap saw invalid get_md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDHandleValidator(put_md_handle, 1)) {
        VERBOSE_ERROR("Swap saw invalid put_md_handle\n");
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(get_md_handle) < local_get_offset + length) {
        VERBOSE_ERROR("Swap saw get_md too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(get_md_handle), local_get_offset + length);
        return PTL_ARG_INVALID;
    }
    if (PtlInternalMDLength(put_md_handle) < local_put_offset + length) {
        VERBOSE_ERROR("Swap saw put_md too short for local_offset (%u < %u)\n",
                      PtlInternalMDLength(put_md_handle), local_put_offset + length);
        return PTL_ARG_INVALID;
    }
    {
        ptl_md_t *mdptr;
        mdptr = PtlInternalMDFetch(put_md_handle);
        if ((mdptr->options & PTL_MD_VOLATILE) && (length > nit_limits[put_md.s.ni].max_volatile_size)) {
            VERBOSE_ERROR("asking for too big a send (%u bytes) from an MD marked VOLATILE (max %u bytes)\n",
                          length, nit_limits[put_md.s.ni].max_volatile_size);
            return PTL_ARG_INVALID;
        }
    }
    {
        int multiple = 1;
        switch (datatype) {
            case PTL_CHAR:
            case PTL_UCHAR:
                multiple = 1;
                break;
            case PTL_SHORT:
            case PTL_USHORT:
                multiple = 2;
                break;
            case PTL_INT:
            case PTL_UINT:
            case PTL_FLOAT:
                multiple = 4;
                break;
            case PTL_LONG:
            case PTL_ULONG:
            case PTL_DOUBLE:
            case PTL_FLOAT_COMPLEX:
                multiple = 8;
                break;
            case PTL_LONG_DOUBLE:
            case PTL_DOUBLE_COMPLEX:
                multiple = 16;
                break;
            case PTL_LONG_DOUBLE_COMPLEX:
                multiple = 32;
                break;
        }
        if (length % multiple != 0) {
            VERBOSE_ERROR("Length not a multiple of datatype size\n");
            return PTL_ARG_INVALID;
        }
    }
    if (get_md.s.ni != put_md.s.ni) {
        VERBOSE_ERROR("MDs *must* be on the same NI\n");
        return PTL_ARG_INVALID;
    }
    switch (get_md.s.ni) {
        case 0:                       // Logical
        case 1:                       // Logical
            if (PtlInternalLogicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
        case 2:                       // Physical
        case 3:                       // Physical
            if (PtlInternalPhysicalProcessValidator(target_id)) {
                VERBOSE_ERROR("Invalid target_id (rank=%u)\n",
                              (unsigned int)target_id.rank);
                return PTL_ARG_INVALID;
            }
            break;
    }
    switch (operation) {
        case PTL_SWAP:
            if (length > nit_limits[get_md.s.ni].max_atomic_size) {
                VERBOSE_ERROR("Length (%u) is bigger than max_atomic_size (%u)\n",
                              (unsigned int)length,
                              (unsigned int)nit_limits[get_md.s.ni].max_atomic_size);
                return PTL_ARG_INVALID;
            }
            break;
        case PTL_CSWAP:
        case PTL_MSWAP:
            if (length > 32) {
                VERBOSE_ERROR("Length (%u) is bigger than one datatype (32)\n",
                              (unsigned int)length);
                return PTL_ARG_INVALID;
            }
            switch (datatype) {
                case PTL_DOUBLE:
                case PTL_FLOAT:
                    VERBOSE_ERROR("PTL_DOUBLE/PTL_FLOAT invalid datatypes for CSWAP/MSWAP\n");
                    return PTL_ARG_INVALID;

                default:
                    break;
            }
            break;
        default:
            VERBOSE_ERROR("Only PTL_SWAP/CSWAP/MSWAP may be used with PtlSwap\n");
            return PTL_ARG_INVALID;
    }
    if (pt_index > nit_limits[get_md.s.ni].max_pt_index) {
        VERBOSE_ERROR("PT index is too big (%lu > %lu)\n",
                      (unsigned long)pt_index,
                      (unsigned long)nit_limits[get_md.s.ni].max_pt_index);
        return PTL_ARG_INVALID;
    }
    if (local_put_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (local_get_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
    if (remote_offset >= (1ULL << 48)) {
        VERBOSE_ERROR("Offsets are only stored internally as 48 bits.\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    PtlInternalMDPosted(put_md_handle);
    if (get_md_handle != put_md_handle) {
        PtlInternalMDPosted(get_md_handle);
    }
    /* step 1: get a local memory fragment */
    hdr = PtlInternalFragmentFetch(sizeof(ptl_internal_header_t) + length + 32);
    /* step 2: fill the op structure */
    hdr->type = HDR_TYPE_SWAP;
    hdr->ni   = get_md.s.ni;
    hdr->src  = proc_number;
    switch (get_md.s.ni) {
        case 0: case 1: // Logical
            hdr->target = target_id.rank;
            break;
        case 2: case 3: // Physical
            hdr->target = target_id.phys.pid;
            break;
    }
#ifdef STRICT_UID_JID
    {
        extern ptl_jid_t the_ptl_jid;
        hdr->jid = the_ptl_jid;
    }
#endif
    hdr->pt_index         = pt_index;
    hdr->match_bits       = match_bits;
    hdr->dest_offset      = remote_offset;
    hdr->length           = length;
    hdr->user_ptr         = user_ptr;
    hdr->entry            = NULL;
    hdr->remaining        = length;
    hdr->md_handle1.a     = get_md_handle;
    hdr->local_offset1    = local_get_offset;
    hdr->md_handle2.a     = put_md_handle;
    hdr->local_offset2    = local_put_offset;
    hdr->hdr_data         = hdr_data;
    hdr->atomic_operation = operation;
    hdr->atomic_datatype  = datatype;
    /* step 3: load up the data */
    {
        uint8_t *dataptr = hdr->data;
        if ((operation == PTL_CSWAP) || (operation == PTL_MSWAP)) {
            switch (datatype) {
                case PTL_CHAR:
                case PTL_UCHAR:
                    memcpy(dataptr, operand, 1);
                    break;
                case PTL_SHORT:
                case PTL_USHORT:
                    memcpy(dataptr, operand, 2);
                    break;
                case PTL_INT:
                case PTL_UINT:
                case PTL_FLOAT:
                    memcpy(dataptr, operand, 4);
                    break;
                case PTL_LONG:
                case PTL_ULONG:
                case PTL_DOUBLE:
                case PTL_FLOAT_COMPLEX:
                    memcpy(dataptr, operand, 8);
                    break;
                case PTL_LONG_DOUBLE:
                case PTL_DOUBLE_COMPLEX:
                    memcpy(dataptr, operand, 16);
                    break;
                case PTL_LONG_DOUBLE_COMPLEX:
                    memcpy(dataptr, operand, 32);
                    break;
            }
        }
        dataptr += 32;
        memcpy(dataptr, PtlInternalMDDataPtr(put_md_handle) + local_put_offset, length);
    }
    /* step 4: enqueue the op structure on the target */
    switch (get_md.s.ni) {
        case 0:
        case 1:                       // Logical
            PtlInternalFragmentToss(hdr, target_id.rank);
            break;
        case 2:
        case 3:                       // Physical
            PtlInternalFragmentToss(hdr, target_id.phys.pid);
            break;
    }
    /* step 5: report the send event */
    {
        ptl_md_t *mdptr = PtlInternalMDFetch(put_md_handle);
        if (mdptr->options & PTL_MD_EVENT_CT_SEND) {
            if ((mdptr->options & PTL_MD_EVENT_CT_BYTES) == 0) {
                PtlInternalCTSuccessInc(mdptr->ct_handle, 1);
            } else {
                PtlInternalCTSuccessInc(mdptr->ct_handle, length);
            }
            PtlInternalCTTriggerCheck(mdptr->ct_handle);
        }
        if ((mdptr->eq_handle != PTL_EQ_NONE) &&
            ((mdptr->options & PTL_MD_EVENT_SUCCESS_DISABLE) == 0)) {
            PtlInternalEQPushESEND(mdptr->eq_handle, length, local_put_offset,
                                   user_ptr);
        }
    }
    return PTL_OK;
}                                      /*}}} */

/* vim:set expandtab: */
