/* System V Interface Definition; for random() and waitpid() */
#define _SVID_SOURCE
/* For BSD definitions (ftruncate, setenv) */
#define _BSD_SOURCE

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

#ifdef HAVE_SYS_POSIX_SHM_H
/* this is getting kinda idiotic */
# define _DARWIN_C_SOURCE
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <sys/types.h>
# include <sys/posix_shm.h>	       /* for PSHMNAMLEN */
#endif

#ifndef PSHMNAMLEN
# define PSHMNAMLEN 100
#endif

static void print_usage(
    int ex);

int main(
    int argc,
    char *argv[])
{
    char shmname[PSHMNAMLEN + 1];
    size_t commsize = 1048576;	// 1MB
    size_t buffsize = getpagesize();
    char countstr[10];
    char procstr[10];
    char commstr[20];
    long count = 0;
    pid_t *pids;
    int shm_fd;
    int err = 0;

    {
	int opt;
	while ((opt = getopt(argc, argv, "b:hc:")) != -1) {
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
		case 'b':
		{
		    char *opterr = NULL;
		    commsize = strtol(optarg, &opterr, 0);
		    if (opterr == NULL || opterr == optarg) {
			fprintf(stderr,
				"Error: Unparseable communication buffer size! (%s)\n",
				optarg);
			print_usage(1);
		    }
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
	fprintf(stderr, "Error: no count specified!\n");
	print_usage(1);
    } else if (argv[optind] == NULL) {
	fprintf(stderr, "Error: no executable specified!\n");
	print_usage(1);
    }

    srandom(time(NULL));
    {
	long int r1 = random();
	long int r2 = random();
	long int r3 = random();
	memset(shmname, 0, PSHMNAMLEN + 1);
	snprintf(shmname, PSHMNAMLEN, "ptl4_%lx%lx%lx", r1, r2, r3);
    }
    assert(setenv("PORTALS4_SHM_NAME", shmname, 0) == 0);
    buffsize += commsize * count;

    /* Establish the communication pad */
    shm_fd = shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
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
	exit(EXIT_FAILURE);
    }

    snprintf(commstr, 20, "%lu", commsize);
    assert(setenv("PORTALS4_COMM_SIZE", commstr, 1) == 0);
    snprintf(countstr, 10, "%li", count);
    assert(setenv("PORTALS4_NUM_PROCS", countstr, 1) == 0);
    pids = malloc(sizeof(pid_t) * count);
    assert(setenv("PORTALS4_COLLECTOR_NID", "0", 1) == 0);
    assert(setenv("PORTALS4_COLLECTOR_PID", countstr, 1) == 0);

    for (long c = 0; c < count; ++c) {
	snprintf(procstr, 10, "%li", c);
	assert(setenv("PORTALS4_PROC_ID", procstr, 1) == 0);
	if ((pids[c] = fork()) == 0) {
	    /* child */
	    execv(argv[optind], argv + optind);
	    perror("yod-> child execv failed!");
	    exit(EXIT_SUCCESS);
	} else if (pids[c] == -1) {
	    perror("yod-> could not launch process!\n");
	    if (c > 0) {
		fprintf(stderr,
			"... I should probably kill any that have been spawned so far. Kick the lazy developer.\n");
	    }
	    exit(EXIT_FAILURE);
	}
    }

    /* Wait for all children to exit */
    for (long c = 0; c < count; ++c) {
	int status;
	if (waitpid(pids[c], &status, 0) != pids[c]) {
	    perror("yod-> waitpid failed");
	}
	if (!WIFEXITED(status)) {
	    ++err;
	    fprintf(stderr, "yod-> child pid %i died unexpectedly\n",
		    (int)pids[c]);
	} else if (WEXITSTATUS(status) > 0) {
	    ++err;
	    fprintf(stderr, "yod-> child pid %i exited %i\n", (int)pids[c],
		    WEXITSTATUS(status));
	}
    }

    /* Cleanup */
    if (shm_unlink(shmname) != 0) {
	perror("yod-> shm_unlink failed");
	exit(EXIT_FAILURE);
    }
    return err;
}

void print_usage(
    int ex)
{
    printf("yod -c [num_procs] executable\n");
    fflush(stdout);
    if (ex) {
	exit(EXIT_FAILURE);
    } else {
	exit(EXIT_SUCCESS);
    }
}
