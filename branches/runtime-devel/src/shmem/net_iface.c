/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System headers */
#include <stdlib.h>
#include <limits.h>                    /* for UINT_MAX */
#include <inttypes.h>
#include <string.h>                    /* for memcpy() */
#include <sys/types.h>
#include <sys/stat.h>                  /* for S_IRUSR */
#include <sys/shm.h>                   /* for shmget() */
#include <unistd.h>                    /* for getpid() */
#ifndef IPC_RMID_IS_CLEANUP
# include <signal.h>                   /* for kill() */
#endif

#include <errno.h>
#include <stdio.h>

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_error.h"
#include "ptl_internal_ints.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_handles.h"
#include "ptl_internal_CT.h"
#include "ptl_internal_MD.h"
#include "ptl_internal_LE.h"
#include "ptl_internal_ME.h"
#include "ptl_internal_DM.h"
#include "ptl_internal_EQ.h"
#include "ptl_internal_fragments.h"
#include "ptl_internal_alignment.h"
#include "ptl_internal_shm.h"
#ifndef NO_ARG_VALIDATION
# include "ptl_internal_error.h"
#endif

ptl_internal_nit_t nit = { { 0, 0, 0, 0 },
                           { 0, 0, 0, 0 },
                           { 0, 0, 0, 0 },
                           { { 0, 0 },
                             { 0, 0 },
                             { 0, 0 },
                             { 0, 0 } } };
ptl_ni_limits_t    nit_limits[4];

static uint32_t nit_limits_init[4] = { 0, 0, 0, 0 };

struct rank_comm_pad *comm_pads[PTL_PID_MAX];
int                   comm_shmids[PTL_PID_MAX];
static int64_t        my_shmid = -2;

#define PTL_SHM_HIGH_BIT (PTL_PID_MAX << 10)

static inline int pid_exists(pid_t pid)
{
    if (kill(pid, 0) == 0) {
        return 0;
    }
    switch (errno) {
        case EINVAL: abort(); // this is just crazy
        case EPERM: return 0; // HA! It exists!

        case ESRCH: return -1; // It does NOT exist

        default: return -1; // Something weird happened
    }
}

static int64_t PtlInternalGetShmPid(int pid)
{
    // Note: This is not thread-safe
    /* I want a specific pid */
    int64_t shmid = shmget(pid | PTL_SHM_HIGH_BIT, per_proc_comm_buf_size + sizeof(struct rank_comm_pad), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if (shmid == -1) {
        /* attempt to recover */
        shmid = shmget(pid | PTL_SHM_HIGH_BIT, per_proc_comm_buf_size + sizeof(struct rank_comm_pad), IPC_CREAT | S_IRUSR | S_IWUSR);
        if (shmid != -1) {
            uint64_t the_owner;
            comm_shmids[pid] = shmid;
            comm_pads[pid]   = shmat(shmid, NULL, 0);
            the_owner        = comm_pads[pid]->owner;
            if ((the_owner == getpid()) || (pid_exists(the_owner) == -1)) {
                if (PtlInternalAtomicCas64(&(comm_pads[pid]->owner), the_owner, getpid()) == the_owner) {
                    /* it's mine! */
                    PtlInternalFragmentInitPid(pid);
                    return shmid;
                }
            }
            shmdt(comm_pads[pid]);
            comm_pads[pid]   = NULL;
            comm_shmids[pid] = -1;
            return -1;
        } else {
            return -1;
        }
    } else {
        // attach
        comm_shmids[pid] = shmid;
        comm_pads[pid]   = shmat(shmid, NULL, 0);
        assert(comm_pads[pid] != NULL);
        {
            uint64_t the_owner = comm_pads[pid]->owner;
            uint64_t mypid     = getpid();
            if ((the_owner == mypid) || (the_owner == 0) || (pid_exists(the_owner) == -1)) {
                if (PtlInternalAtomicCas64(&(comm_pads[pid]->owner), the_owner, mypid) == the_owner) {
                    PtlInternalFragmentInitPid(pid);
                    return shmid;
                }
            }
            /* it must have been "recovered" out from under me :P */
            shmdt(comm_pads[pid]);
            comm_pads[pid]   = NULL;
            comm_shmids[pid] = -1;
            return -1;
        }
    }
    assert(comm_pads[pid]);
    PtlInternalFragmentInitPid(pid);
    return shmid;
}

void INTERNAL PtlInternalMapInPid(int pid)
{
    int64_t               shmid = shmget(pid | PTL_SHM_HIGH_BIT, per_proc_comm_buf_size + sizeof(struct rank_comm_pad), S_IRUSR | S_IWUSR);
    struct rank_comm_pad *tmp_pad;

    if (shmid == -1) {
        VERBOSE_ERROR("failure to shmget pid %i\n", pid);
        return;
    }
    tmp_pad = shmat(shmid, NULL, 0);
    if (PtlInternalAtomicCasPtr(&(comm_pads[pid]), NULL, tmp_pad) != NULL) {
        int err = shmdt(tmp_pad);
        if (err) {
            VERBOSE_ERROR("failure to detach from redundant comm_pad (%p); will be leaked!\n", tmp_pad);
        }
    } else {
        // XXX Is this a race condition?... I don't think so, but it's remotely possible
        comm_shmids[pid] = shmid;
    }
}

void INTERNAL PtlInternalDetachCommPads(void)
{
    /* detach from peers (and self) */
    for (int i = 0; i < PTL_PID_MAX; ++i) {
        if (comm_pads[i] != NULL) {
            /* deallocate NI */
            struct shmid_ds buf;
            ptl_assert(shmdt(comm_pads[i]), 0);
            shmctl(comm_shmids[i], IPC_STAT, &buf);
            if (buf.shm_nattch == 0) {
                if (shmctl(comm_shmids[i], IPC_RMID, NULL) != 0) {
                    switch(errno) {
                        case EINVAL: break;
                        default:
                            perror("shmctl in DetachCommPads");
                            abort();
                    }
                }
            }
        }
    }
}

int API_FUNC PtlNIInit(ptl_interface_t        iface,
                       unsigned int           options,
                       ptl_pid_t              pid,
                       const ptl_ni_limits_t *desired,
                       ptl_ni_limits_t       *actual,
                       ptl_handle_ni_t       *ni_handle)
{   /*{{{*/
    ptl_internal_handle_converter_t ni = { .s = { HANDLE_NI_CODE, 0, 0 } };
    ptl_table_entry_t              *tmp;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if (pid != PTL_PID_ANY) {
        if ((proc_number != -1) && (pid != proc_number)) {
            VERBOSE_ERROR("Invalid pid (%i), rank may already be set (%i)\n", (int)pid, (int)proc_number);
            return PTL_ARG_INVALID;
        }
        if (pid > PTL_PID_MAX) {
            VERBOSE_ERROR("Pid too large (%li > %li)\n", (long)pid, (long)PTL_PID_MAX);
            return PTL_ARG_INVALID;
        }
    }
    if ((iface != 0) && (iface != PTL_IFACE_DEFAULT)) {
        VERBOSE_ERROR("Invalid Interface (%i)\n", (int)iface);
        return PTL_ARG_INVALID;
    }
    if (options & ~(PTL_NI_INIT_OPTIONS_MASK)) {
        VERBOSE_ERROR("Invalid options value (0x%x)\n", options);
        return PTL_ARG_INVALID;
    }
    if (options & PTL_NI_MATCHING && options & PTL_NI_NO_MATCHING) {
        VERBOSE_ERROR("Neither matching nor non-matching\n");
        return PTL_ARG_INVALID;
    }
    if (options & PTL_NI_LOGICAL && options & PTL_NI_PHYSICAL) {
        VERBOSE_ERROR("Neither logical nor physical\n");
        return PTL_ARG_INVALID;
    }
    if (ni_handle == NULL) {
        VERBOSE_ERROR("ni_handle == NULL\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    if (iface == PTL_IFACE_DEFAULT) {
        iface = 0;
    }
    ni.s.code = iface;
    switch (options) {
        case (PTL_NI_MATCHING | PTL_NI_LOGICAL):
            ni.s.ni = 0;
            break;
        case PTL_NI_NO_MATCHING | PTL_NI_LOGICAL:
            ni.s.ni = 1;
            break;
        case (PTL_NI_MATCHING | PTL_NI_PHYSICAL):
            ni.s.ni = 2;
            break;
        case PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL:
            ni.s.ni = 3;
            break;
#ifndef NO_ARG_VALIDATION
        default:
            return PTL_ARG_INVALID;
#endif
    }
    *ni_handle = ni.a;
    if ((desired != NULL) &&
        (PtlInternalAtomicCas32(&nit_limits_init[ni.s.ni], 0, 1) == 0)) {
        /* nit_limits_init[ni.s.ni] now marked as "being initialized" */
        if ((desired->max_entries > 0) &&
            (desired->max_entries < (1 << HANDLE_CODE_BITS))) {
            nit_limits[ni.s.ni].max_entries = desired->max_entries;
        }
        if (desired->max_unexpected_headers > 0) {
            nit_limits[ni.s.ni].max_unexpected_headers = desired->max_unexpected_headers;
        }
        if ((desired->max_mds > 0) &&
            (desired->max_mds < (1 << HANDLE_CODE_BITS))) {
            nit_limits[ni.s.ni].max_mds = desired->max_mds;
        }
        if ((desired->max_cts > 0) &&
            (desired->max_cts < (1 << HANDLE_CODE_BITS))) {
            nit_limits[ni.s.ni].max_cts = desired->max_cts;
        }
        if ((desired->max_eqs > 0) &&
            (desired->max_eqs < (1 << HANDLE_CODE_BITS))) {
            nit_limits[ni.s.ni].max_eqs = desired->max_eqs;
        }
        if (desired->max_pt_index >= 63) {      // XXX: there may need to be more restrictions on this
            nit_limits[ni.s.ni].max_pt_index = desired->max_pt_index;
        }
        // nit_limits[ni.s.ni].max_iovecs = INT_MAX;      // ???
        if ((desired->max_list_size > 0) &&
            (desired->max_list_size < (1ULL << (sizeof(uint32_t) * 8)))) {
            nit_limits[ni.s.ni].max_list_size = desired->max_list_size;
        }
        if ((desired->max_triggered_ops >= 0) &&
            (desired->max_triggered_ops < (1ULL << (sizeof(uint32_t) * 8)))) {
            nit_limits[ni.s.ni].max_triggered_ops = desired->max_triggered_ops;
        }
        if ((desired->max_msg_size > 0) &&
            (desired->max_msg_size < UINT32_MAX)) {
            nit_limits[ni.s.ni].max_msg_size = desired->max_msg_size;
        }
        if ((desired->max_atomic_size >= 8) &&
#ifdef USE_TRANSFER_ENGINE
            (desired->max_atomic_size <= UINT32_MAX)
#else
            (desired->max_atomic_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))
#endif
            ) {
            nit_limits[ni.s.ni].max_atomic_size = desired->max_atomic_size;
        }
        if ((desired->max_fetch_atomic_size >= 8) &&
#ifdef USE_TRANSFER_ENGINE
            (desired->max_fetch_atomic_size <= UINT32_MAX)
#else
            (desired->max_fetch_atomic_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))
#endif
            ) {
            nit_limits[ni.s.ni].max_fetch_atomic_size = desired->max_fetch_atomic_size;
        }
        if ((desired->max_waw_ordered_size >= 8) &&
            (desired->max_waw_ordered_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))) {
            nit_limits[ni.s.ni].max_waw_ordered_size = desired->max_waw_ordered_size;
        }
        if ((desired->max_war_ordered_size >= 8) &&
            (desired->max_war_ordered_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))) {
            nit_limits[ni.s.ni].max_war_ordered_size = desired->max_war_ordered_size;
        }
        if ((desired->max_volatile_size >= 8) &&
            (desired->max_volatile_size <= (LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t)))) {
            nit_limits[ni.s.ni].max_volatile_size = desired->max_volatile_size;
        }
        nit_limits[ni.s.ni].features = PTL_TARGET_BIND_INACCESSIBLE | PTL_TOTAL_DATA_ORDERING;
        nit_limits_init[ni.s.ni]     = 2;       // mark it as done being initialized
    }
    while (nit_limits_init[ni.s.ni] == 1) SPINLOCK_BODY();     /* if being initialized by another thread, wait for it to be initialized */
    if (actual != NULL) {
        *actual = nit_limits[ni.s.ni];
    }

shmid_gen:
    if (PtlInternalAtomicCas64(&my_shmid, -2, -1) != -2) {
        while (my_shmid == -1) SPINLOCK_BODY();  // wait for the other thread to do the allocaiton
        if (my_shmid < 0) { goto shmid_gen; }
    } else {
        memset(comm_pads, 0, sizeof(struct rank_comm_pad *) * PTL_PID_MAX);
        for (int i = 0; i < PTL_PID_MAX; ++i) comm_shmids[i] = -1;
        if ((pid == PTL_PID_ANY) && (proc_number != PTL_PID_ANY)) {
            pid = proc_number;
        }
        if (pid == PTL_PID_ANY) {
            /* I want any pid */
            int64_t tmp_shmid;
            /* first try my Unix pid */
            tmp_shmid = PtlInternalGetShmPid(getpid() % PTL_PID_MAX);
            /* next try to get a random pid */
            if (tmp_shmid == -1) {
                pid       = (rand() % PTL_PID_MAX) + 1; // XXX: arbitrary cap
                tmp_shmid = PtlInternalGetShmPid(pid);
                if (tmp_shmid == -1) {
                    pid       = (rand() % PTL_PID_MAX) + 1; // XXX: arbitrary cap
                    tmp_shmid = PtlInternalGetShmPid(pid);
                }
            }
            if (tmp_shmid == -1) {
                for (pid = 1; pid <= PTL_PID_MAX; ++pid) {
                    tmp_shmid = PtlInternalGetShmPid(pid);
                    if (tmp_shmid != -1) { break; }
                }
            }
            my_shmid    = tmp_shmid;
            proc_number = pid;
        } else {
            my_shmid = PtlInternalGetShmPid(pid);
        }
        if (my_shmid == -1) {
            my_shmid = -2;
            VERBOSE_ERROR("could not allocate a shmid!\n");
            return PTL_NO_SPACE;
        }
    }

    /* BWB: FIX ME: This isn't thread safe (parallel NIInit calls may return too quickly) */
    if (PtlInternalAtomicInc(&(nit.refcount[ni.s.ni]), 1) == 0) {
        PtlInternalCTNISetup(ni.s.ni, nit_limits[ni.s.ni].max_cts);
        PtlInternalMDNISetup(ni.s.ni, nit_limits[ni.s.ni].max_mds);
        PtlInternalEQNISetup(ni.s.ni);
        if (options & PTL_NI_MATCHING) {
            PtlInternalMENISetup(ni.s.ni, nit_limits[ni.s.ni].max_entries);
        } else {
            PtlInternalLENISetup(ni.s.ni, nit_limits[ni.s.ni].max_entries);
        }
        /* Okay, now this is tricky, because it needs to be thread-safe, even with respect to PtlNIFini(). */
        while ((tmp = PtlInternalAtomicCasPtr(&(nit.tables[ni.s.ni]), NULL,
                                              (void *)1)) == (void *)1) SPINLOCK_BODY();
        if (tmp == NULL) {
            ALIGNED_CALLOC(tmp, CACHELINE_WIDTH, nit_limits[ni.s.ni].max_pt_index + 1, sizeof(ptl_table_entry_t));
            if (tmp == NULL) {
                nit.tables[ni.s.ni] = NULL;
                return PTL_NO_SPACE;
            }
            nit.unexpecteds[ni.s.ni] = nit.unexpecteds_buf[ni.s.ni] =
                                           calloc(nit_limits[ni.s.ni].max_unexpected_headers, sizeof(ptl_internal_buffered_header_t));
            if (nit.unexpecteds[ni.s.ni] == NULL) {
                free(tmp);
                nit.tables[ni.s.ni] = NULL;
                return PTL_NO_SPACE;
            }
            for (size_t u = 0; u < nit_limits[ni.s.ni].max_unexpected_headers - 1; ++u) {
                nit.unexpecteds[ni.s.ni][u].hdr.next = &(nit.unexpecteds[ni.s.ni][u + 1]);
            }
            for (size_t e = 0; e <= nit_limits[ni.s.ni].max_pt_index; ++e) {
                PtlInternalPTInit(tmp + e);
            }
            __sync_synchronize();      // full memory fence
            nit.tables[ni.s.ni] = tmp;
        }
        assert(nit.tables[ni.s.ni] != NULL);
        PtlInternalDMSetup();          // This MUST happen AFTER the tables are set up
    }

#ifdef USE_TRANSFER_ENGINE
    /* xfe_init MUST be thread safe. */
    xfe_init();
#endif

    return PTL_OK;
} /*}}}*/

int API_FUNC PtlNIFini(ptl_handle_ni_t ni_handle)
{   /*{{{*/
    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("Bad NI (%lu)\n", (unsigned long)ni_handle);
        return PTL_ARG_INVALID;
    }
    assert(my_shmid != -1);
#endif
    if (PtlInternalAtomicInc(&(nit.refcount[ni.s.ni]), -1) == 1) {
        while (nit.internal_refcount[ni.s.ni] != 0) SPINLOCK_BODY();
        PtlInternalDMTeardown();
        PtlInternalCTNITeardown(ni.s.ni);
        PtlInternalMDNITeardown(ni.s.ni);
        PtlInternalEQNITeardown(ni.s.ni);
        switch (ni.s.ni) {
            case 0:
            case 2:
                PtlInternalMENITeardown(ni.s.ni);
                break;
            case 1:
            case 3:
                PtlInternalLENITeardown(ni.s.ni);
                break;
        }
        free(nit.unexpecteds_buf[ni.s.ni]);
        ALIGNED_FREE(nit.tables[ni.s.ni], CACHELINE_WIDTH);
        nit.unexpecteds[ni.s.ni]     = NULL;
        nit.unexpecteds_buf[ni.s.ni] = NULL;
        nit.tables[ni.s.ni]          = NULL;
    }
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlNIStatus(ptl_handle_ni_t ni_handle,
                         ptl_sr_index_t  status_register,
                         ptl_sr_value_t *status)
{   /*{{{*/
    const ptl_internal_handle_converter_t ni = { ni_handle };

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        return PTL_ARG_INVALID;
    }
    if (status == NULL) {
        return PTL_ARG_INVALID;
    }
    if (status_register >= PTL_SR_LAST) {
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    *status = nit.regs[ni.s.ni][status_register];
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlNIHandle(ptl_handle_any_t handle,
                         ptl_handle_ni_t *ni_handle)
{   /*{{{*/
    ptl_internal_handle_converter_t ehandle;

#ifndef NO_ARG_VALIDATION
    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
#endif
    ehandle.a = handle;
    switch (ehandle.s.selector) {
        case HANDLE_NI_CODE:
            *ni_handle = ehandle.i;
            break;
        case HANDLE_EQ_CODE:
        case HANDLE_CT_CODE:
        case HANDLE_MD_CODE:
        case HANDLE_LE_CODE:
        case HANDLE_ME_CODE:
            ehandle.s.code     = 0;
            ehandle.s.selector = HANDLE_NI_CODE;
            *ni_handle         = ehandle.i;
            break;
        default:
            return PTL_ARG_INVALID;
    }
    return PTL_OK;
} /*}}}*/

int API_FUNC PtlSetMap(ptl_handle_ni_t      ni_handle,
                       ptl_size_t           map_size,
                       const ptl_process_t *mapping)
{   /*{{{*/
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ni = { ni_handle };

    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("NI handle is invalid.\n");
        return PTL_ARG_INVALID;
    }
    if (ni.s.ni > 1) {
        VERBOSE_ERROR("NI handle is for physical interface\n");
        return PTL_ARG_INVALID;
    }
    if (map_size == 0) {
        VERBOSE_ERROR("Input map_size is zero\n");
        return PTL_ARG_INVALID;
    }
    if (mapping == NULL) {
        VERBOSE_ERROR("Input mapping is NULL\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
       /* The mapping in the shmem Portals4 implementation is fixed and static. It cannot be changed. */
    return PTL_IGNORED;
} /*}}}*/

int API_FUNC PtlGetMap(ptl_handle_ni_t ni_handle,
                       ptl_size_t      map_size,
                       ptl_process_t  *mapping,
                       ptl_size_t     *actual_map_size)
{   /*{{{*/
#ifndef NO_ARG_VALIDATION
    const ptl_internal_handle_converter_t ni = { ni_handle };

    if (PtlInternalLibraryInitialized() == PTL_FAIL) {
        return PTL_NO_INIT;
    }
    if ((ni.s.ni >= 4) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        VERBOSE_ERROR("NI handle is invalid.\n");
        return PTL_ARG_INVALID;
    }
    if ((map_size == 0) && ((mapping != NULL) || (actual_map_size == NULL))) {
        VERBOSE_ERROR("Input map_size is zero\n");
        return PTL_ARG_INVALID;
    }
    if ((mapping == NULL) && ((map_size != 0) || (actual_map_size == NULL))) {
        VERBOSE_ERROR("Output mapping ptr is NULL\n");
        return PTL_ARG_INVALID;
    }
#endif /* ifndef NO_ARG_VALIDATION */
    if (actual_map_size != NULL) {
        *actual_map_size = num_siblings;
    }
    for (int i = 0; i < map_size && i < num_siblings; ++i) {
        mapping[i].phys.nid = 0;
        mapping[i].phys.pid = (ptl_pid_t)i;
    }
    return PTL_OK;
} /*}}}*/

int INTERNAL PtlInternalNIValidator(const ptl_internal_handle_converter_t ni)
{   /*{{{*/
#ifndef NO_ARG_VALIDATION
    if (ni.s.selector != HANDLE_NI_CODE) {
        return PTL_ARG_INVALID;
    }
    if ((ni.s.ni > 3) || (ni.s.code != 0) || (nit.refcount[ni.s.ni] == 0)) {
        return PTL_ARG_INVALID;
    }
#endif
    return PTL_OK;
} /*}}}*/

ptl_internal_buffered_header_t INTERNAL *PtlInternalAllocUnexpectedHeader(const uint_fast8_t ni)
{   /*{{{*/
    ptl_internal_buffered_header_t *hdr = nit.unexpecteds[ni];

    if (hdr != NULL) {
        ptl_internal_buffered_header_t *foundhdr;
        while ((foundhdr =
                    PtlInternalAtomicCasPtr(&nit.unexpecteds[ni], hdr,
                                            hdr->hdr.next)) != hdr) {
            hdr = foundhdr;
        }
    }
    return hdr;
} /*}}}*/

void INTERNAL PtlInternalDeallocUnexpectedHeader(ptl_internal_buffered_header_t *const hdr)
{   /*{{{*/
    ptl_internal_buffered_header_t **const   ni_unex = &nit.unexpecteds[hdr->hdr.ni];
    ptl_internal_buffered_header_t *restrict expectednext;
    ptl_internal_buffered_header_t *restrict foundnext;

    expectednext = hdr->hdr.next = *ni_unex;
    while ((foundnext = PtlInternalAtomicCasPtr(ni_unex, expectednext, hdr)) != expectednext) {
        expectednext = hdr->hdr.next = foundnext;
    }
} /*}}}*/

/* vim:set expandtab: */
