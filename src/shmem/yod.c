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

#include <sys/types.h>
#include <pwd.h>

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
#include "ptl_internal_assert.h"

#ifndef PSHMNAMLEN
# define PSHMNAMLEN 100
#endif

/* global to allow signal to clean up */
static pid_t *pids = NULL;

/* globals for communicating with the collator thread */
static long count = 0;
static ptl_handle_ct_t collator_ct_handle;
static ptl_handle_ni_t ni_physical;

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
    ptl_assert(setenv(env_str, str, 1), 0); \
} while (0)

int main(
    int argc,
    char *argv[])
{
    const size_t pagesize = getpagesize();
    size_t commsize;
    size_t buffsize = pagesize;
    int shm_fd;
    int err = 0;
    pthread_t collator_thread;
    int ct_spawned = 1;
    size_t small_frag_size = 128;
    size_t small_frag_count = 512;
    size_t large_frag_size = 4096;
    size_t large_frag_count = 128;
    const size_t max_count = buffsize - 1;
    void *commpad = NULL;

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
		    if (count > max_count) {	/* technically, this is an arbitrary limit; more would require multiple pages for the initial init barier. */
			fprintf(stderr,
				"Error: Exceeded max of %lu processes. (complain to developer)\n",
				(unsigned long)max_count);
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
        struct passwd* pw = getpwuid(geteuid());
        if (NULL == pw) {
            /* fix me */
            perror("yod-> getpwuid()");
            exit(EXIT_FAILURE);
        }

	long int r1 = random();
	long int r2 = random();
	long int r3 = random();
	r1 = r2 = r3 = 0;	       // for testing
	memset(shmname, 0, PSHMNAMLEN + 1);
	snprintf(shmname, PSHMNAMLEN, "ptl4_%s_%lx%lx%lx", pw->pw_name, r1, r2, r3);
    }
    ptl_assert(setenv("PORTALS4_SHM_NAME", shmname, 0), 0);
    commsize =
	(small_frag_count * small_frag_size) +
	(large_frag_count * large_frag_size) +
	sizeof(NEMESIS_blocking_queue);
    buffsize += commsize * (count + 1);	// the one extra is for the collator

    /* Establish the communication pad */
    shm_fd = shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (shm_fd < 0) {
        perror("yod-> shm_open");
	if (shm_unlink(shmname) == -1) {
	    perror("yod-> attempting to clean up; shm_unlink failed");
	    exit(EXIT_FAILURE);
	}
	shm_fd =
	    shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    }
    if (shm_fd >= 0) {
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
#ifndef HAVE_PTHREAD_SHMEM_LOCKS
	for (size_t i = 0; i <= count; ++i) {
	    char *remote_pad = ((char *)commpad) + pagesize + (commsize * i);
	    NEMESIS_blocking_queue *q1 =
		(NEMESIS_blocking_queue *) remote_pad;
	    ptl_assert(pipe(q1->pipe), 0);
	}
#endif /* PTHREAD_SHMEM_LOCKS */
    } else {
	perror("yod-> shm_open failed");
	if (shm_unlink(shmname) == -1) {
	    perror("yod-> attempting to clean up; shm_unlink failed");
	}
	exit(EXIT_FAILURE);
    }

    pids = malloc(sizeof(pid_t) * (count + 1));
    assert(pids != NULL);
    EXPORT_ENV_NUM("PORTALS4_COMM_SIZE", commsize);
    EXPORT_ENV_NUM("PORTALS4_NUM_PROCS", count);
    EXPORT_ENV_NUM("PORTALS4_COLLECTOR_NID", 0);
    EXPORT_ENV_NUM("PORTALS4_COLLECTOR_PID", count);
    EXPORT_ENV_NUM("PORTALS4_SMALL_FRAG_SIZE", small_frag_size);
    EXPORT_ENV_NUM("PORTALS4_LARGE_FRAG_SIZE", large_frag_size);
    EXPORT_ENV_NUM("PORTALS4_SMALL_FRAG_COUNT", small_frag_count);
    EXPORT_ENV_NUM("PORTALS4_LARGE_FRAG_COUNT", large_frag_count);

    /*****************************
     * Provide COLLATOR services *
     *****************************/
    EXPORT_ENV_NUM("PORTALS4_RANK", count);
    ptl_assert(PtlInit(), PTL_OK);
    ptl_assert(PtlNIInit
	   (PTL_IFACE_DEFAULT, PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
	    PTL_PID_ANY, NULL, NULL, 0, NULL, NULL, &ni_physical), PTL_OK);
    {
	ptl_pt_index_t pt_index;
	ptl_assert(PtlPTAlloc(ni_physical, 0, PTL_EQ_NONE, 0, &pt_index),
	       PTL_OK);
	assert(pt_index == 0);
    }
    collator_ct_handle = PTL_CT_NONE;
    if (pthread_create(&collator_thread, NULL, collator, &ni_physical) != 0) {
	perror("pthread_create");
	fprintf(stderr, "yod-> failed to create collator thread\n");
	ct_spawned = 0;		       /* technically not fatal, though maybe should be */
    }

    /* Launch children */
    for (long c = 0; c < count; ++c) {
	EXPORT_ENV_NUM("PORTALS4_RANK", c);
	if ((pids[c] = fork()) == 0) {
	    /* child */
	    execv(argv[optind], argv + optind);
	    perror("yod-> child execv failed!");
	    exit(EXIT_FAILURE);
	} else if (pids[c] == -1) {
	    perror("yod-> could not launch process!");
	    if (c > 0) {
		fprintf(stderr,
			"... I should probably kill any that have been spawned so far. Kick the lazy developer.\n");
	    }
	    exit(EXIT_FAILURE);
	}
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
		    (int)exited, WTERMSIG(status));
	    for (d = 0; d < count; ++d) {
		if (pids[d] != exited) {
		    int stat;
		    switch (waitpid(pids[d], &stat, WNOHANG)) {
			case 0:
			    if (kill(pids[d], SIGKILL) == -1) {
				if (errno != ESRCH) {
				    perror("yod-> kill failed!");
				} else {
				    fprintf(stderr,
					    "yod-> child %i already dead\n",
					    (int)pids[d]);
				}
			    }
			    break;
			case -1:
			    break;
			default:
			    ++c;
			    if (WIFSIGNALED(stat) && !WIFSTOPPED(stat)) {
				++err;
				fprintf(stderr,
					"yod-> child pid %i (%u) died prematurely (%i)\n",
					(int)pids[d], (unsigned)d,
					WTERMSIG(stat));
			    } else if (WIFEXITED(stat) &&
				       WEXITSTATUS(stat) > 0) {
				++err;
				fprintf(stderr,
					"yod-> child pid %i exited %i\n",
					(int)pids[d], WEXITSTATUS(status));
			    }
		    }
		}
	    }
	    break;
	} else if (WIFEXITED(status) && WEXITSTATUS(status) > 0) {
	    ++err;
	    fprintf(stderr, "yod-> child pid %i exited %i\n", (int)exited,
		    WEXITSTATUS(status));
	}
    }
    if (ct_spawned == 1) {
	ptl_ct_event_t ctc;
	memset(&ctc, 0xff, sizeof(ptl_ct_event_t));
	PtlCTSet(collator_ct_handle, ctc);
	switch (pthread_join(collator_thread, NULL)) {
	    case 0:
		break;
	    case EDEADLK:
		fprintf(stderr,
			"yod-> joining thread would create deadlock!\n");
		break;
	    case EINVAL:
		fprintf(stderr, "yod-> collator thread not joinable\n");
		break;
	    case ESRCH:
		fprintf(stderr, "yod-> collator thread missing\n");
		break;
	    default:
		perror("yod-> pthread_join");
		break;
	}
    }

    /* Cleanup */
    ptl_assert(PtlPTFree(ni_physical, 0), PTL_OK);
    ptl_assert(PtlNIFini(ni_physical), PTL_OK);
    PtlFini();
    for (size_t i = 0; i <= count; ++i) {
	char *remote_pad = ((char *)commpad) + pagesize + (commsize * i);
	NEMESIS_blocking_queue *q1 = (NEMESIS_blocking_queue *) remote_pad;
#ifdef HAVE_PTHREAD_SHMEM_LOCKS
	int perr;
	if ((perr = pthread_cond_destroy(&q1->trigger)) != 0) {
	    char buf[200];
	    strerror_r(perr, buf, 200);
	    fprintf(stderr, "yod-> destroying queue1 trigger(%i): %s\n", perr,
		    buf);
	    abort();
	}
	if ((perr = pthread_mutex_destroy(&q1->trigger_lock)) != 0) {
	    char buf[200];
	    strerror_r(perr, buf, 200);
	    fprintf(stderr, "yod-> destroying queue1 trigger lock(%i): %s\n",
		    perr, buf);
	    abort();
	}
#else
	if (close(q1->pipe[0]) != 0) {
	    perror("yod-> closing queue1 pipe[0]");
	    abort();
	}
	if (close(q1->pipe[1]) != 0) {
	    perror("yod-> closing queue1 pipe[1]");
	    abort();
	}
#endif
    }
    if (munmap(commpad, buffsize) != 0) {
	perror("yod-> munmap failed"); /* technically non-fatal */
    }
    if (shm_unlink(shmname) != 0) {
	perror("yod-> shm_unlink failed");
	exit(EXIT_FAILURE);
    }
    free(pids);
    return err;
}

static void cleanup(
    Q_UNUSED int s)
{
    if (pids != NULL && count > 0) {
	for (int d = 0; d < count; ++d) {
	    if (kill(pids[d], SIGKILL) == -1) {
		if (errno != ESRCH) {
		    perror("yod-> kill failed!");
		}
	    }
	}
    }
}

void *collator(
    void *Q_UNUSED junk) Q_NORETURN
{
    ptl_process_t *mapping;

    /* set up the landing pad to collect and distribute mapping information */
    mapping = calloc(count, sizeof(ptl_process_t));
    assert(mapping != NULL);
    ptl_le_t le;
    ptl_md_t md;
    ptl_handle_le_t le_handle;
    ptl_handle_md_t md_handle;
    md.start = le.start = mapping;
    md.length = le.length = count * sizeof(ptl_process_t);
    le.ac_id.uid = PTL_UID_ANY;
    le.options = PTL_LE_OP_PUT | PTL_LE_EVENT_CT_PUT;
    ptl_assert(PtlCTAlloc(ni_physical, &le.ct_handle), PTL_OK);
    collator_ct_handle = le.ct_handle;
    ptl_assert(PtlLEAppend
	   (ni_physical, 0, le, PTL_PRIORITY_LIST, NULL,
	    &le_handle), PTL_OK);
    /* wait for everyone to post to the mapping */
    {
	ptl_ct_event_t ct_data;
	if (PtlCTWait(le.ct_handle, count, &ct_data) == PTL_INTERRUPTED) {
	    goto cleanup_phase;
	}
	assert(ct_data.failure == 0);  // XXX: should do something useful
	ct_data.success = ct_data.failure = 0;
	ptl_assert(PtlCTSet(le.ct_handle, ct_data), PTL_OK);
    }
    /* cleanup */
    ptl_assert(PtlLEUnlink(le_handle), PTL_OK);
    /* prepare for being a barrier captain */
    le.start = NULL;
    le.length = 0;
    ptl_assert(PtlLEAppend
	   (ni_physical, 0, le, PTL_PRIORITY_LIST, NULL, &le_handle),
	   PTL_OK);
    /* now distribute the mapping */
    md.options = PTL_MD_EVENT_DISABLE | PTL_MD_EVENT_CT_ACK;
    md.eq_handle = PTL_EQ_NONE;
    ptl_assert(PtlCTAlloc(ni_physical, &md.ct_handle), PTL_OK);
    ptl_assert(PtlMDBind(ni_physical, &md, &md_handle), PTL_OK);
    for (uint64_t r = 0; r < count; ++r) {
	ptl_assert(PtlPut
	       (md_handle, 0, count * sizeof(ptl_process_t), PTL_CT_ACK_REQ,
		mapping[r], 0, 0, 0, NULL, 0), PTL_OK);
    }
    /* wait for the puts to finish */
    {
	ptl_ct_event_t ct_data;
	ptl_assert(PtlCTWait(md.ct_handle, count, &ct_data), PTL_OK);
	assert(ct_data.failure == 0);
	ct_data.success = ct_data.failure = 0;
	ptl_assert(PtlCTSet(md.ct_handle, ct_data), PTL_OK);
    }
    /* cleanup */
    ptl_assert(PtlMDRelease(md_handle), PTL_OK);
    /* prepare for being a barrier captain */
    md.start = NULL;
    md.length = 0;
    ptl_assert(PtlMDBind(ni_physical, &md, &md_handle), PTL_OK);
    /*******************************************************
     * Transition point to being a general BARRIER captain *
     *******************************************************/
    do {
	/* wait for everyone to post to the barrier */
	ptl_ct_event_t ct_data;
	if (PtlCTWait(le.ct_handle, count, &ct_data) == PTL_INTERRUPTED) {
	    goto cleanup_phase;
	}
	assert(ct_data.failure == 0);
	/* reset the LE's CT */
	ct_data.success = ct_data.failure = 0;
	ptl_assert(PtlCTSet(le.ct_handle, ct_data), PTL_OK);
	/* release everyone */
	for (uint64_t r = 0; r < count; ++r) {
	    ptl_assert(PtlPut
		   (md_handle, 0, 0, PTL_CT_ACK_REQ, mapping[r], 0, 0, 0,
		    NULL, 0), PTL_OK);
	}
	/* wait for the releases to finish */
	ptl_assert(PtlCTWait(md.ct_handle, count, &ct_data), PTL_OK);
	assert(ct_data.failure == 0);
	/* reset the MD's CT */
	ct_data.success = ct_data.failure = 0;
	ptl_assert(PtlCTSet(le.ct_handle, ct_data), PTL_OK);
    } while (0);
    /* cleanup */
  cleanup_phase:
    free(mapping);
    ptl_assert(PtlLEUnlink(le_handle), PTL_OK);
    return NULL;
}

void print_usage(
    int ex)
{
    printf("yod {options} executable\n");
    printf("Options are:\n");
    printf("\t-c [num_procs]            Spawn num_procs processes.\n");
    printf
	("\t-l [large_fragment_size]  The size in bytes of large message buffers (default: 4k).\n");
    printf
	("\t-s [small_fragment_size]  The size in bytes of small message buffers (default: 128)\n");
    printf("\t-h                        Print this help.\n");
    printf
	("\t-L [large_fragment_count] The number of large message buffers to allocate.\n");
    printf
	("\t-S [small_fragment_count] The number of small message buffers to allocate.\n");
    fflush(stdout);
    if (ex) {
	exit(EXIT_FAILURE);
    } else {
	exit(EXIT_SUCCESS);
    }
}
