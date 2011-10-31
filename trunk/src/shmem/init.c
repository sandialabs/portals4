/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System libraries */
#include <stddef.h>                    /* for NULL */
#include <sys/mman.h>                  /* for mmap() and shm_open() */
#include <sys/stat.h>                  /* for S_IRUSR and friends */
#include <fcntl.h>                     /* for O_RDWR */
#include <stdlib.h>                    /* for getenv() */
#include <unistd.h>                    /* for close() */
#include <limits.h>                    /* for UINT_MAX */
#include <string.h>                    /* for memset() */

#if defined(PARANOID) || defined(LOUD_DROPS)
# include <stdio.h>
#endif

/* Internals */
#include "portals4_runtime.h"
#include "ptl_visibility.h"
#include "ptl_internal_assert.h"
#include "ptl_internal_atomic.h"
#include "ptl_internal_commpad.h"
#include "ptl_internal_nit.h"
#include "ptl_internal_fragments.h"
#include "ptl_internal_nemesis.h"
#include "ptl_internal_papi.h"
#include "ptl_internal_locks.h"

size_t            num_siblings           = 0;
ptl_pid_t         proc_number            = PTL_PID_ANY;
size_t            per_proc_comm_buf_size = 0;
size_t            firstpagesize          = 0;

static unsigned int init_ref_count    = 0;

#define PARSE_ENV_NUM(env_str, var, reqd) do {                           \
        char       *strerr;                                              \
        const char *str = getenv(env_str);                               \
        if (str == NULL) {                                               \
            if (reqd == 1) { goto exit_fail; }                           \
        } else {                                                         \
            size_t tmp = strtol(str, &strerr, 10);                       \
            if ((strerr == NULL) || (strerr == str) || (*strerr != 0)) { \
                goto exit_fail;                                          \
            }                                                            \
            var = tmp;                                                   \
        }                                                                \
} while (0)

int INTERNAL PtlInternalLibraryInitialized(void)
{   /*{{{*/
    if (init_ref_count == 0) {
        return PTL_FAIL;
    }
    return PTL_OK;
} /*}}}*/

/* The trick to this function is making it thread-safe: multiple threads can
 * all call PtlInit concurrently, and all will wait until initialization is
 * complete, and if there is a failure, all will report failure.
 *
 * PtlInit() will only work if the process has been executed by yod (which
 * handles important aspects of the init/cleanup and passes data via
 * envariables).
 */
int API_FUNC PtlInit(void)
{   /*{{{*/
    unsigned int        race              = PtlInternalAtomicInc(&init_ref_count, 1);
    static volatile int done_initializing = 0;
    static volatile int failure           = 0;
    extern ptl_uid_t    the_ptl_uid;

    if (race == 0) {

#ifdef _SC_PAGESIZE
        firstpagesize = sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
        firstpagesize = sysconf(_SC_PAGE_SIZE);
#elif defined(HAVE_GETPAGESIZE)
        firstpagesize = getpagesize();
#else
        firstpagesize = 4096;
#endif

        the_ptl_uid = geteuid();

        /* Parse the official yod-provided environment variables */
        PARSE_ENV_NUM("PORTALS4_NUM_PROCS", num_siblings, 1);
        PARSE_ENV_NUM("PORTALS4_RANK", proc_number, 1);
        PARSE_ENV_NUM("PORTALS4_COMM_SIZE", per_proc_comm_buf_size, 1);
        PARSE_ENV_NUM("PORTALS4_SMALL_FRAG_SIZE", SMALL_FRAG_SIZE, 0);
        PARSE_ENV_NUM("PORTALS4_LARGE_FRAG_SIZE", LARGE_FRAG_SIZE, 0);
        PARSE_ENV_NUM("PORTALS4_SMALL_FRAG_COUNT", SMALL_FRAG_COUNT, 0);
        PARSE_ENV_NUM("PORTALS4_LARGE_FRAG_COUNT", LARGE_FRAG_COUNT, 0);
        assert(((SMALL_FRAG_COUNT * SMALL_FRAG_SIZE) +
                (LARGE_FRAG_COUNT * LARGE_FRAG_SIZE) +
                sizeof(NEMESIS_blocking_queue)) == per_proc_comm_buf_size);

        memset(&nit, 0, sizeof(ptl_internal_nit_t));
        for (int ni = 0; ni < 4; ++ni) {
            nit_limits[ni].max_list_size          = 16384;                            // Arbitrary
            nit_limits[ni].max_pt_index           = 63;                               // Minimum required by spec
            nit_limits[ni].max_entries            = nit_limits[ni].max_list_size * 4; // Maybe this should be max_pt_index+1 instead of 4
            nit_limits[ni].max_unexpected_headers = 128;                              // Arbitrary
            nit_limits[ni].max_mds                = 128;                              // Arbitrary
            nit_limits[ni].max_cts                = 128;                              // Arbitrary
            nit_limits[ni].max_eqs                = 128;                              // Arbitrary
            nit_limits[ni].max_iovecs             = 0;                                // XXX: ???
            nit_limits[ni].max_triggered_ops      = 128;                              // Arbitrary
            nit_limits[ni].max_msg_size           = UINT32_MAX;
#ifdef USE_TRANSFER_ENGINE
            nit_limits[ni].max_atomic_size       = UINT32_MAX;
            nit_limits[ni].max_fetch_atomic_size = UINT32_MAX;
#else
            nit_limits[ni].max_atomic_size       = LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t);
            nit_limits[ni].max_fetch_atomic_size = LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t);
#endif
            nit_limits[ni].max_waw_ordered_size = LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t);   // single payload
            nit_limits[ni].max_war_ordered_size = LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t);   // single payload
            nit_limits[ni].max_volatile_size    = LARGE_FRAG_PAYLOAD - sizeof(ptl_internal_header_t);   // single payload
        }
        PtlInternalPAPIInit();

        /* Release any concurrent initialization calls */
        __sync_synchronize();
        done_initializing = 1;
        runtime_init();
        return PTL_OK;
    } else {
        /* Should block until other inits finish. */
        while (!done_initializing) SPINLOCK_BODY();
        if (!failure) {
            return PTL_OK;
        } else {
            goto exit_fail_fast;
        }
    }
exit_fail:
    failure = 1;
    __sync_synchronize();
    done_initializing = 1;
exit_fail_fast:
    PtlInternalAtomicInc(&init_ref_count, -1);
    return PTL_FAIL;
} /*}}}*/

void API_FUNC PtlFini(void)
{   /*{{{*/
    unsigned int lastone;

    runtime_finalize();
#ifdef PARANOID
    if (init_ref_count <= 0) {
        fprintf(stderr, "PORTALS4-> init_ref_count = %u\n", init_ref_count);
    }
    assert(init_ref_count > 0);
#endif
    if (init_ref_count == 0) {
        return;
    }
    lastone = PtlInternalAtomicInc(&init_ref_count, -1);
    if (lastone == 1) {
        /* Clean up */
        PtlInternalPAPITeardown();
        PtlInternalDetachCommPads();
    }
} /*}}}*/

/* vim:set expandtab: */
