/*
 * ptl_main.c
 */

/* this is the main for the portals4 control service process.
 * it is responsible for maintaining any global state
 * and creating xrc receive qp's
 */

#include "ctl.h"

int verbose;
int ptl_log_level;
static char *progname;
static char *nid_table_fname;
static char *rank_table_fname;
static unsigned int jobid;
static char lock_filename [1024];

struct p4oibd_config conf;

static void rpc_callback(struct session *session)
{
	struct rpc_msg msg;
	struct net_intf *net_intf;
	struct ib_intf *ib_intf;

	switch(session->rpc_msg.type) {
	case QUERY_INIT_DATA:
		msg.type = REPLY_INIT_DATA;
		strcpy(msg.reply_init_data.shmem_filename, conf.shmem.filename);
		msg.reply_init_data.shmem_filesize = conf.shmem.filesize;
		rpc_send(session, &msg);
		break;

	case QUERY_XRC_DOMAIN:
#if 0
		printf("FZ- got query for intf %s\n",
			session->rpc_msg.query_xrc_domain.net_name);
#endif
		net_intf = find_net_intf(
				session->rpc_msg.query_xrc_domain.net_name);
		msg.type = REPLY_XRC_DOMAIN;
		if (net_intf) {
			ib_intf = net_intf->ib_intf;
			strcpy(msg.reply_xrc_domain.xrc_domain_fname,
				ib_intf->xrc_domain_fname);
			strcpy(msg.reply_xrc_domain.ib_name, ib_intf->name);
		} else {
			strcpy(msg.reply_xrc_domain.xrc_domain_fname, "");
			strcpy(msg.reply_xrc_domain.ib_name, "");
		}
		rpc_send(session, &msg);
		break;

	default:
		fprintf(stderr, "Got unhandled RPC message id %d\n",
			session->rpc_msg.type);
		break;
	}
}

/* Each process in a job will try to start the control process. Only
 * one must succeed. */
static int run_once(void)
{
	int fd;
	int err;

	/* Create a file and try to lock it. */
	sprintf(lock_filename, "/tmp/p4oibd-JID-%d.lck", jobid);
	fd = open(lock_filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	if (fd == -1)
		return 1;

	err = flock(fd, LOCK_EX | LOCK_NB);
	if (err)
		return 1;

	/* We won ! */
	return 0;
}

static void usage(char *argv[])
{
	printf("usage:\n");
	printf("	%s OPTIONS\n", argv[0]);
	printf("\n");
	printf("OPTIONS: (default)\n");
	printf("    -h | --help         print this message\n");
	printf("    -v | --verbose      increase quantity of output\n");
	printf("    -p | --port         control port (default = %d)\n",
		PTL_CTL_PORT);
	printf("    -x | --xrc          XRC port (default = %d)\n", XRC_PORT);
	printf("    -n | --nid-table    NID table\n");
	printf("    -r | --rank-table   rank table\n");
	printf("    -l | --log level    rank table\n");
}

static int arg_process(int argc, char *argv[])
{
	int c;
	int opt_index = 0;
	char *opt_string = "hvp:n:r:j:l:";
	static const struct option opt_long[] = {
		{"help", 0, 0, 'h'},
		{"verbose", 0, 0, 'v'},
		{"port", 1, 0, 'p'},
		{"nid-table", 1, 0, 'n'},
		{"rank-table", 1, 0, 'r'},
		{"jid", 1, 0, 'j'},
		{"log", 1, 0, 'l'},
		{0, 0, 0, 0}
	};

	progname = rindex(argv[0], '/') + 1;
	if (!progname)
		progname = argv[0];

	conf.ctl_port = PTL_CTL_PORT;

	while (1) {
		c = getopt_long(argc, argv, opt_string, opt_long, &opt_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			//TODO handle opt_long[opt_index]
			//		optarg
			break;

		case 'h':
			goto err1;

		case 'v':
			verbose++;
			break;

		case 'p':
			conf.ctl_port = strtol(optarg, NULL, 0);
			break;

		case 'n':
			nid_table_fname = optarg;
			break;

		case 'r':
			rank_table_fname = optarg;
			break;

		case 'j':
			jobid = strtol(optarg, NULL, 0);
			break;

		case 'l':
			ptl_log_level = strtol(optarg, NULL, 0);
			break;

		default:
			fprintf(stderr, "unexpected option\n");
			goto err1;
		}
	}

	if (!nid_table_fname) {
		fprintf(stderr, "NID table filename not set\n");
		return 1;
	}

	if (!rank_table_fname) {
		fprintf(stderr, "RANK table filename not set\n");
		return 1;
	}

	if (!jobid) {
		fprintf(stderr, "Missing Job ID.\n");
		return 1;
	}

	return 0;

err1:
	usage(argv);
	return 1;
}

int main(int argc, char *argv[])
{
	int err;

	err = arg_process(argc, argv);
	if (err)
		return 1;

	if (verbose) {
		printf("%s starting with:\n", progname);
		printf("	port = %d\n", conf.ctl_port);
	}

	err = run_once();
	if (err) {
		/* Another process is already running. */
		return 1;
	}

	err = load_nid_table(nid_table_fname);
	if (err) {
		fprintf(stderr, "Couldn't load the NID table at %s\n",
			nid_table_fname);
		return 1;
	}

	err = load_rank_table(rank_table_fname);
	if (err) {
		fprintf(stderr, "Couldn't load the RANK table at %s\n",
			rank_table_fname);
		return 1;
	}

	err = create_shared_memory();
	if (err) {
		fprintf(stderr, "Couldn't create shared data\n");
		return 1;
	}
	
	err = create_ib_resources();
	if (err) {
		fprintf(stderr, "Couldn't create some IB resources\n");
		return 1;
	}

	err = rpc_init(rpc_type_server, conf.ctl_port, &conf.rpc, rpc_callback);
	if (err) {
		switch (err) {
		case EADDRINUSE:
			fprintf(stderr,
				"it appears that %s is already running\n",
				progname);
		default:
			fprintf(stderr, "unable to start %s\n", progname);
		}
		goto err1;
	}

	printf("%s started\n", progname);

#if 0
	daemon(0, 0);
#endif

	sleep(99999);

	rpc_fini(conf.rpc);

	destroy_ib_resources();

	printf("%s stopped\n", progname);

	remove(lock_filename);

	return 0;

err1:
	return 1;
}
