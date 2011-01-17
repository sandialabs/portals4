#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "ptl_test.h"
#include "cio.h"

/* todo
	- get version from configure
 */

int verbose = 1;
int debug;

char *filename;

void usage()
{
	printf("usage:\n");
	printf("ptl_test OPTIONS\n");
	printf("\n");
	printf("OPTIONS\n");
	printf("	-h, --help		print this message\n");
	printf("	-V, --version		print version\n");
	printf("	-v, --verbose		increase output\n");
	printf("	-q, --quiet		reduce output\n");
	printf("	-d, --debug		increase debugging output\n");
	printf("	-f, --file FILENAME	test file location\n");
	printf("	-l, --log level		log level\n");
}

int process_args(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int c;
	int option_index = 0;
	char *options = "hVvqdf:l:";
	static struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
		{"verbose", 0, 0, 'v'},
		{"quiet", 0, 0, 'q'},
		{"debug", 0, 0, 'd'},
		{"file", 1, 0, 'f'},
		{"log", 1, 0, 'l'},
		{0, 0, 0, 0}
	};

	while (1) {
		c = getopt_long(argc, argv, options,
				long_options, &option_index);
		if (c == -1)
		break;

		switch (c) {
		case 'h':
			return PTL_TEST_RETURN_HELP;

		case 'V':
			printf("ptl_test-%s\n", PTL_TEST_VERSION);
			return PTL_TEST_RETURN_EXIT;

		case 'v':
			verbose++;
			break;

		case 'q':
			verbose = 0;
			break;

		case 'd':
			debug++;
			break;

		case 'f':
			filename = optarg;
			break;

		case 'l':
			ptl_log_level = strtol(optarg, NULL, 0);
			break;

		case '?':
			break;

		default:
			printf("?? getopt returned character code 0%o ??\n", c);
		}
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
	}

	if (!filename) {
		printf("missing filename\n");
		return -1;
	}

	return 0;
}

int init()
{
	init_dict();
	return 0;
}

int fini()
{
	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	xmlDocPtr doc;

	ret = process_args(argc, argv);
	if (ret) {
		if (ret == PTL_TEST_RETURN_HELP) {
			usage();
			exit(0);
		} else if (ret == PTL_TEST_RETURN_EXIT)
			exit(0);
		else
			exit(ret);
	}

	ret = init();
	if (ret)
		exit(ret);

	ret = cio_init();
	if (ret)
		exit(ret);

	doc = cio_get_input(filename);
	if (!doc)
		exit(-1);

	run_doc(doc);

	cio_free_input(doc);

	cio_cleanup();

	ret = fini();

	return 0;
}
