/* System V Interface Definition; for random() and waitpid() */
#define _SVID_SOURCE
/* For BSD definitions (ftruncate, setenv) */
#define _BSD_SOURCE
/* For POSIX definitions (kill) on Linux */
#define _POSIX_SOURCE
/* for Darwin definitions (getpagesize) on Darwin;
 * only necessary because _POSIX_SOURCE conflicts */
#define _DARWIN_C_SOURCE

#include <portals4.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <getopt.h>		       /* for getopt() */
#include <unistd.h>		       /* for fork() and ftruncate() */

#include <sys/mman.h>		       /* for shm_open() */
#include <sys/stat.h>		       /* for S_IRUSR and friends */
#include <fcntl.h>		       /* for O_RDWR */
#include <sys/wait.h>		       /* for waitpid() */
#include <string.h>		       /* for memset() */
#include <pthread.h>		       /* for all pthread functions */
#include <sys/types.h>		       /* for kill() */
#include <signal.h>		       /* for kill() */
#include <errno.h>		       /* for errno */

#ifdef HAVE_SYS_POSIX_SHM_H
/* this is getting kinda idiotic */
# define _DARWIN_C_SOURCE
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <sys/types.h>
# include <sys/posix_shm.h>	       /* for PSHMNAMLEN */
#endif

#include "ptl_internal_nemesis.h"

#ifndef PSHMNAMLEN
# define PSHMNAMLEN 100
#endif

static long count = 0;

static char shmname[PSHMNAMLEN + 1];
static void cleanup(
    int s);

static void print_usage(
    int ex);
static void *collator(
    void *junk);

#define EXPORT_ENV_NUM(env_str, val) do { \
    char str[21]; \
    snprintf(str, 21, "%lu", (unsigned long)val); \
    assert(setenv(env_str, str, 1) == 0); \
} while (0)

int main(
    int argc,
    char *argv[])
{
    size_t commsize = 1048576;	// 1MB
    size_t buffsize = getpagesize();
    pid_t *pids;
    int shm_fd;
    int err = 0;
    pthread_t collator_thread;
    int ct_spawned = 1;
    size_t small_frag_size = 128;
    size_t small_frag_count = 512;
    size_t large_frag_size = 4096;
    size_t large_frag_count = 128;

    {
	int opt;
	while ((opt = getopt(argc, argv, "l:s:hc:L:S:")) != -1) {
	    switch (opt) {
		case 'h':
		    print_usage(0);
		    break;
		case 'c':
		{
		    char *opterr = NULL;
		    count = strtol(optarg, &opterr, 0);
		    if (opterr == NULL || opterr == optarg) {
			fprintf(stderr,
				"Error: Unparseable number of processes! (%s)\n",
				optarg);
			print_usage(1);
		    }
		    if (count > 128) {
			fprintf(stderr,
				"Error: Exceeded max of 128 processes. (complain to developer)\n");
			exit(EXIT_FAILURE);
		    }
		}
		    break;
		case 's':
		{
		    char *opterr = NULL;
		    small_frag_size = strtol(optarg, &opterr, 0);
		    if (opterr == NULL || opterr == optarg || *opterr == 0) {
			fprintf(stderr,
				"Error: Unparseable small fragment size! (%s)\n",
				optarg);
			print_usage(1);
		    }
		    break;
		}
		case 'l':
		{
		    char *opterr = NULL;
		    large_frag_size = strtol(optarg, &opterr, 0);
		    if (opterr == NULL || opterr == optarg || *opterr == 0) {
			fprintf(stderr,
				"Error: Unparseable large fragment size! (%s)\n",
				optarg);
			print_usage(1);
		    }
		    break;
		}
		case 'S':
		{
		    char *opterr = NULL;
		    small_frag_count = strtol(optarg, &opterr, 0);
		    if (opterr == NULL || opterr == optarg || *opterr == 0) {
			fprintf(stderr,
				"Error: Unparseable small fragment count! (%s)\n",
				optarg);
			print_usage(1);
		    }
		    break;
		}
		case 'L':
		{
		    char *opterr = NULL;
		    large_frag_count = strtol(optarg, &opterr, 0);
		    if (opterr == NULL || opterr == optarg || *opterr == 0) {
			fprintf(stderr,
				"Error: Unparseable large fragment count! (%s)\n",
				optarg);
			print_usage(1);
		    }
		    break;
		}
		case ':':
		    fprintf(stderr, "Error: Option `%c' needs a value!\n",
			    optopt);
		    print_usage(1);
		    break;
		case '?':
		    fprintf(stderr, "Error: No such option: `%c'\n", optopt);
		    print_usage(1);
		    break;
	    }
	}
    }
    if (count == 0) {
	count = 1;
    }
    if (argv[optind] == NULL) {
	fprintf(stderr, "Error: no executable specified!\n");
	print_usage(1);
    }

    srandom(time(NULL));
    {
	long int r1 = random();
	long int r2 = random();
	long int r3 = random();
	r1 = r2 = r3 = 0;	       // for testing
	memset(shmname, 0, PSHMNAMLEN + 1);
	snprintf(shmname, PSHMNAMLEN, "ptl4_%lx%lx%lx", r1, r2, r3);
    }
    assert(setenv("PORTALS4_SHM_NAME", shmname, 0) == 0);
    commsize =
	(small_frag_count * small_frag_size) +
	(large_frag_count * large_frag_size) +
	(sizeof(NEMESIS_blocking_queue) * 2);
    buffsize += commsize * (count + 1);	// the one extra is for the collator

    /* Establish the communication pad */
    shm_fd = shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (shm_fd < 0) {
	if (shm_unlink(shmname) == -1) {
	    perror("yod-> attempting to clean up; shm_unlink failed");
	    exit(EXIT_FAILURE);
	}
	shm_fd = shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    }
    if (shm_fd >= 0) {
	void *commpad;
	/* pre-allocate the shared memory ... necessary on BSD */
	if (ftruncate(shm_fd, buffsize) != 0) {
	    perror("yod-> ftruncate failed");
	    if (shm_unlink(shmname) == -1) {
		perror("yod-> shm_unlink failed");
	    }
	    exit(EXIT_FAILURE);
	}
	if ((commpad =
	     mmap(NULL, buffsize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd,
		  0)) == MAP_FAILED) {
	    perror("yod-> mmap failed");
	    if (shm_unlink(shmname) == -1) {
		perror("yod-> shm_unlink failed");
	    }
	    exit(EXIT_FAILURE);
	}
	memset(commpad, 0, buffsize);
	if (munmap(commpad, buffsize) != 0) {
	    perror("yod-> munmap failed");	/* technically non-fatal */
	}
    } else {
	perror("yod-> shm_open failed");
	if (shm_unlink(shmname) == -1) {
	    perror("yod-> attempting to clean up; shm_unlink failed");
	}
	exit(EXIT_FAILURE);
    }

    pids = malloc(sizeof(pid_t) * (count + 1));
    EXPORT_ENV_NUM("PORTALS4_COMM_SIZE", commsize);
    EXPORT_ENV_NUM("PORTALS4_NUM_PROCS", count);
    EXPORT_ENV_NUM("PORTALS4_COLLECTOR_NID", 0);
    EXPORT_ENV_NUM("PORTALS4_COLLECTOR_PID", count);
    EXPORT_ENV_NUM("PORTALS4_SMALL_FRAG_SIZE", small_frag_size);
    EXPORT_ENV_NUM("PORTALS4_LARGE_FRAG_SIZE", large_frag_size);
    EXPORT_ENV_NUM("PORTALS4_SMALL_FRAG_COUNT", small_frag_count);
    EXPORT_ENV_NUM("PORTALS4_LARGE_FRAG_COUNT", large_frag_count);

    /* Launch children */
    for (long c = 0; c < count; ++c) {
	EXPORT_ENV_NUM("PORTALS4_RANK", c);
	if ((pids[c] = fork()) == 0) {
	    /* child */
	    execv(argv[optind], argv + optind);
	    perror("yod-> child execv failed!");
	    exit(EXIT_FAILURE);
	} else if (pids[c] == -1) {
	    perror("yod-> could not launch process!\n");
	    if (c > 0) {
		fprintf(stderr,
			"... I should probably kill any that have been spawned so far. Kick the lazy developer.\n");
	    }
	    exit(EXIT_FAILURE);
	}
    }

    /*****************************
     * Provide COLLATOR services *
     *****************************/
    if (pthread_create(&collator_thread, NULL, collator, NULL) != 0) {
	perror("pthread_create");
	fprintf(stderr, "yod-> failed to create collator thread\n");
	ct_spawned = 0;		       /* technically not fatal, though maybe should be */
    }
    if (pthread_detach(collator_thread) != 0) {
	perror("pthread_detach"); /* failure is not a big deal */
    }

    /* Clean up after Ctrl-C */
    signal(SIGINT, cleanup);

    /* Wait for all children to exit */
    for (size_t c = 0; c < count; ++c) {
	int status;
	pid_t exited;
	if ((exited = wait(&status)) == -1) {
	    perror("yod-> wait failed");
	}
	if (WIFSIGNALED(status) && !WIFSTOPPED(status)) {
	    size_t d;
	    ++err;
	    fprintf(stderr,
		    "yod-> child pid %i died unexpectedly (%i), killing everyone\n",
		    (int)pids[c], WTERMSIG(status));
	    for (d = 0; d < count; ++d) {
		if (pids[d] != exited) {
		    if (kill(pids[d], SIGKILL) == -1) {
			if (errno != ESRCH) {
			    perror("yod-> kill failed!\n");
			} else {
			    fprintf(stderr, "yod-> child %i already dead\n",
				    (int)pids[d]);
			}
		    }
		}
	    }
	} else if (WIFEXITED(status) && WEXITSTATUS(status) > 0) {
	    ++err;
	    fprintf(stderr, "yod-> child pid %i exited %i\n", (int)pids[c],
		    WEXITSTATUS(status));
	}
    }
    if (ct_spawned == 1) {
	switch (pthread_cancel(collator_thread)) {
	    case 0: // success!
	    case ESRCH: // thread already gone
		break;
	    default:
		perror("yod-> pthread_cancel");
		break;
	}
	PtlFini();
    }

    /* Cleanup */
    cleanup(0);
    return err;
}

static void cleanup(
    int s)
{
    if (shm_unlink(shmname) != 0) {
	perror("yod-> shm_unlink failed");
	exit(EXIT_FAILURE);
    }
    if (s != 0) {
	exit(EXIT_FAILURE);
    }
}

void *collator(
    void * __attribute__ ((unused)) junk)
{
    char procstr[10];
    ptl_process_id_t *mapping;
    ptl_pt_index_t pt_index;

    snprintf(procstr, 10, "%li", count);
    assert(setenv("PORTALS4_RANK", procstr, 1) == 0);
    assert(PtlInit() == PTL_OK);
    ptl_handle_ni_t ni_physical;
    assert(PtlNIInit
	   (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
	    PTL_PID_ANY, NULL, NULL, 0, NULL, NULL, &ni_physical) == PTL_OK);
    assert(PtlPTAlloc(ni_physical, 0, PTL_EQ_NONE, 0, &pt_index) == PTL_OK);
    /* set up the landing pad to collect and distribute mapping information */
    mapping = calloc(count, sizeof(ptl_process_id_t));
    assert(mapping != NULL);
    ptl_le_t le;
    ptl_md_t md;
    ptl_handle_le_t le_handle;
    ptl_handle_md_t md_handle;
    md.start = le.start = mapping;
    md.length = le.length = count * sizeof(ptl_process_id_t);
    le.ac_id.uid = PTL_UID_ANY;
    le.options =
	PTL_LE_OP_PUT | PTL_LE_OP_GET | PTL_LE_EVENT_CT_PUT |
	PTL_LE_EVENT_CT_GET;
    assert(PtlCTAlloc(ni_physical, &le.ct_handle) == PTL_OK);
    assert(PtlLEAppend
	   (ni_physical, 0, le, PTL_PRIORITY_LIST, NULL,
	    &le_handle) == PTL_OK);
    /* wait for everyone to post to the mapping */
    {
	ptl_ct_event_t ct_data;
	assert(PtlCTWait(le.ct_handle, count, &ct_data) == PTL_OK);
	assert(ct_data.failure == 0); // XXX: should do something useful
    }
    for (unsigned int i = 0; i < count; i++) {
	printf("mapping[%u] = {%u,%u}\n", i, mapping[i].phys.pid,
	       mapping[i].phys.nid);
    }
    /* cleanup */
    assert(PtlCTFree(le.ct_handle) == PTL_OK);
    assert(PtlLEUnlink(le_handle) == PTL_OK);
    /* now distribute the mapping */
    md.options = PTL_MD_EVENT_CT_ACK;
    md.eq_handle = PTL_EQ_NONE;
    assert(PtlCTAlloc(ni_physical, &md.ct_handle) == PTL_OK);
    assert(PtlMDBind(ni_physical, &md, &md_handle) == PTL_OK);
    for (uint64_t r = 0; r < count; ++r) {
	assert(PtlPut
	       (md_handle, 0, count * sizeof(ptl_process_id_t),
		PTL_CT_ACK_REQ, mapping[r], 0, 0, 0, NULL, 0) == PTL_OK);
    }
    /* wait for the puts to finish */
    {
	ptl_ct_event_t ct_data;
	assert(PtlCTWait(md.ct_handle, count, &ct_data) == PTL_OK);
	assert(ct_data.failure == 0);
    }
    /* cleanup */
    assert(PtlCTFree(md.ct_handle) == PTL_OK);
    assert(PtlMDRelease(md_handle) == PTL_OK);
    assert(PtlPTFree(ni_physical, pt_index) == PTL_OK);
    assert(PtlNIFini(ni_physical) == PTL_OK);
    free(mapping);
    return NULL;
}

void print_usage(
    int ex)
{
    printf("yod {options} executable\n");
    printf("Options are:\n");
    printf("\t-c [num_procs]            Spawn num_procs processes.\n");
    printf("\t-l [large_fragment_size]  The size in bytes of large message buffers (default: 4k).\n");
    printf("\t-s [small_fragment_size]  The size in bytes of small message buffers (default: 128)\n");
    printf("\t-h                        Print this help.\n");
    printf("\t-L [large_fragment_count] The number of large message buffers to allocate.\n");
    printf("\t-S [small_fragment_count] The number of small message buffers to allocate.\n");
    fflush(stdout);
    if (ex) {
	exit(EXIT_FAILURE);
    } else {
	exit(EXIT_SUCCESS);
    }
}
