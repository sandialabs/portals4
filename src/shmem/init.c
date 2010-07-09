/* The API definition */
#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* System libraries */
#include <assert.h>
#include <stddef.h>		       /* for NULL */
#include <sys/mman.h>		       /* for mmap() and shm_open() */
#include <sys/stat.h>		       /* for S_IRUSR and friends */
#include <fcntl.h>		       /* for O_RDWR */
#include <stdlib.h>		       /* for getenv() */
#include <unistd.h>		       /* for close() */

/* Internals */
#include "ptl_visibility.h"
#include "ptl_internal_atomic.h"

static unsigned int init_ref_count = 0;
static void *comm_pad = NULL;
const size_t comm_pad_size = 4096 * 16;	// XXX: completely arbitrary
const char *comm_pad_shm_name = NULL;

/* The trick to this function is making it thread-safe: multiple threads can
 * all call PtlInit concurrently, and all will wait until initialization is
 * complete, and if there is a failure, all will report failure. Multiple
 * process issues (e.g. if one fails to mmap and the others succeed) are all
 * handled transparently by the standard shm_open/shm_unlink semantics. */
int API_FUNC PtlInit(void)
{
    unsigned int race = PtlInternalAtomicInc(&init_ref_count, 1);
    static volatile int done_initializing = 0;
    static volatile int failure = 0;

    if (race == 0) {
	int fd;

	/* Open the communication pad */
	assert(comm_pad == NULL);
	comm_pad_shm_name = getenv("PORTALS4_SHM_NAME");
	assert(comm_pad_shm_name != NULL);
	if (comm_pad_shm_name) {
	    goto exit_fail;
	}
	fd = shm_open(comm_pad_shm_name, O_RDWR, S_IRUSR | S_IWUSR);
	assert(fd >= 0);
	if (fd >= 0) {
	    comm_pad =
		mmap(NULL, comm_pad_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		     fd, 0);
	    if (comm_pad != MAP_FAILED) {
		close(fd);
		/* Release any concurrent initialization calls */
		__sync_synchronize();
		done_initializing = 1;
		return PTL_OK;
	    } else {
		goto exit_fail;
	    }
	} else {
	    goto exit_fail;
	}
    } else {
	/* Should block until other inits finish. */
	while (!done_initializing) ;
	if (!failure)
	    return PTL_OK;
	else
	    goto exit_fail_fast;
    }
  exit_fail:
    failure = 1;
    __sync_synchronize();
    done_initializing = 1;
  exit_fail_fast:
    PtlInternalAtomicInc(&init_ref_count, -1);
    return PTL_FAIL;
}

void API_FUNC PtlFini(void)
{
    unsigned int lastone;
    assert(init_ref_count > 0);
    if (init_ref_count == 0)
	return;
    lastone = PtlInternalAtomicInc(&init_ref_count, -1);
    if (lastone == 1) {
	/* Clean up */
	assert(munmap(comm_pad, comm_pad_size) == 0);
    }
}
