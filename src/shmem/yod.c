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
#include <sys/wait.h> /* for waitpid() */
#include <string.h> /* for memset() */

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

static void print_usage(int ex);

int main(int argc, char *argv[])
{
    char shmname[PSHMNAMLEN + 1];
    char countstr[10];
    char procstr[10];
    int opt;
    long count = 0;
    pid_t *pids;
    int shm_fd;

    while ((opt = getopt(argc, argv, "hc:")) != -1) {
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
	    }
		break;
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
	memset(shmname, 0, PSHMNAMLEN+1);
	snprintf(shmname, PSHMNAMLEN, "ptl4_%lx%lx%lx", r1, r2, r3);
    }
    assert(setenv("PORTALS4_SHM_NAME", shmname, 0) == 0);

    /* Establish the communication pad */
    shm_fd = shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (shm_fd >= 0) {
	/* pre-allocate the shared memory page... necessary on BSD */
	if (ftruncate(shm_fd, getpagesize()) != 0) {
	    perror("ftruncate failed");
	    if (shm_unlink(shmname) == -1) {
		perror("shm_unlink failed");
	    }
	    exit(EXIT_FAILURE);
	}
    } else {
	perror("shm_open failed");
	exit(EXIT_FAILURE);
    }

    snprintf(countstr, 10, "%li", count);
    assert(setenv("PORTALS4_NUM_PROCS", countstr, 1) == 0);
    pids = malloc(sizeof(pid_t) * count);

    for (long c = 0; c < count; ++c) {
	snprintf(procstr, 10, "%li", c);
	assert(setenv("PORTALS4_MYPROC_ID", procstr, 1) == 0);
	if ((pids[c] = fork()) == 0) {
	    int i;
	    /* child */
	    execv(argv[optind], argv + optind);
	    perror("execv failed!");
	    exit(EXIT_SUCCESS);
	} else if (pids[c] == -1) {
	    perror("could not launch process!\n");
	    if (c > 0) {
		fprintf(stderr,
			"... I should probably kill any that have been spawned so far. Kick the lazy developer.\n");
	    }
	    exit(EXIT_FAILURE);
	}
    }

    /* Wait for all children to exit */
    for (long c = 0; c < count; ++c) {
	int junk;
	if (waitpid(pids[c], &junk, 0) != pids[c]) {
	    perror("waitpid failed");
	}
    }

    /* Cleanup */
    if (shm_unlink(shmname) != 0) {
	perror("shm_unlink failed");
	exit(EXIT_FAILURE);
    }
    return 0;
}

void print_usage(int ex)
{
    printf("yod -c [num_procs] executable\n");
    fflush(stdout);
    if (ex) {
	exit(EXIT_FAILURE);
    } else {
	exit(EXIT_SUCCESS);
    }
}
