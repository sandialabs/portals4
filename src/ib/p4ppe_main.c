/**
 * @file p4ppe.c
 *
 * Portals Process Engine main file.
 */

#include "ptl_loc.h"
#include "p4ppe.h"

#include <getopt.h>
#include <sys/un.h>

static int num_threads = 1;
static int num_bufs = 1000;

static void usage(const char *name)
{
	printf("  Portals 4 Process Engine\n\n");
	printf("  Usage:\n");
	printf("  %s [OPTIONS]\n\n", name);
	printf("  Options are:\n");
	printf("    -n, --nppebufs=NUM     number of PPE buffers to create (default=%d)\n",
               num_bufs);
	printf("    -p, --nprogthreads=NUM number of processing threads to create (default=%d)\n",
               num_threads);
	printf("    -h, --help             displays this message\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	int c;

	while (1) {
		int option_index;
		static struct option long_options[] = {
			{"nppebufs", 1, 0, 'n'},
			{"nprogthreads", 1, 0, 'p'},
			{"help", 0, 0, 'h' },
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "n:p:h",
                        long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 'n':
			num_bufs = atoi(optarg);
			if (num_bufs < 1) {
				ptl_warn("Invalid argument value for nppebufs\n");
				return 1;
			}
			break;

		case 'p':
			num_threads = atoi(optarg);
			if (num_threads < 1 || num_threads>MAX_PROGRESS_THREADS) {
				ptl_warn("Invalid argument value for nprogthreads\n");
				return 1;
			}
			break;

		case 'h':
			usage(argv[0]);
			return 1;
			break;

		default:
			ptl_warn("Invalid option %s\n", argv[option_index]);
			return 1;
		}
	}

	return ppe_run(num_bufs, num_threads, 0);
}

