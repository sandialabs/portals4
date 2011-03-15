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
static char lock_filename [1024];

/* Used by master only. Rank table is complete. Send it to local ranks
 * and remote control processes. */
void broadcast_rank_table(struct p4oibd_config *conf)
{
	ptl_rank_t local_rank;
	struct rpc_msg msg;

	/* Give rank table to local nodes. */
	msg.type = REPLY_RANK_TABLE;
	strcpy(msg.reply_rank_table.shmem_filename, conf->shmem.filename);
	msg.reply_rank_table.shmem_filesize = conf->shmem.filesize;
	for (local_rank=0; local_rank<conf->local_nranks; local_rank++) {
		//??		rpc_send(session, &msg);

		rpc_send(conf->sessions[local_rank], &msg);
	}

	/* Give rank table to remote control nodes. */
	// todo
}

static void rpc_callback(struct session *session, void *data)
{
	struct rpc_msg msg;
	struct net_intf *net_intf;
	struct ib_intf *ib_intf;
	struct rpc_msg *m = &session->rpc_msg;
	ptl_rank_t local_rank;
	struct p4oibd_config *conf = data;

	switch(session->rpc_msg.type) {
	case QUERY_RANK_TABLE:
		if (verbose)
			printf("got QUERY_RANK_TABLE from rank %d / %d\n", session->rpc_msg.query_rank_table.rank, conf->local_nranks);
		local_rank = m->query_rank_table.local_rank;

		if (conf->local_rank_table->size <= m->query_rank_table.local_rank) {
			/* Bad rank. Should not happen */
			assert(0);
		}

		/* We may already have it. In that case, reply, but don't
		 * modify the table. */
		if (conf->local_rank_table->elem[local_rank].nid != conf->nid) {
			conf->local_rank_table->elem[local_rank].rank = m->query_rank_table.rank;
			conf->local_rank_table->elem[local_rank].xrc_srq_num = m->query_rank_table.xrc_srq_num;
			conf->local_rank_table->elem[local_rank].addr = m->query_rank_table.addr;
			conf->local_rank_table->elem[local_rank].nid = conf->nid;

			conf->sessions[local_rank] = session;
			conf->num_sessions ++;
		}

		if (verbose)
			printf("got another session - %d %d\n", conf->num_sessions, conf->local_nranks);
		if (conf->num_sessions == conf->local_nranks) {
			/* All local ranks have reported. Sent the local rank table to the master control. */
			//	msg.type = LOCAL_RANK_TABLE;

			if (verbose)
				printf("got all session\n");

			if (conf->nid == conf->master_nid) {
				/* Hey, I'm the boss. Copy the local ranks to the
				 * global rank table if we don't already have it. */
				if (conf->master_rank_table->elem[conf->local_rank_table->elem[local_rank].rank].nid != conf->local_rank_table->elem[local_rank].nid) {
					for (local_rank=0; local_rank < conf->local_rank_table->size; local_rank++) {
						ptl_rank_t rank = conf->local_rank_table->elem[local_rank].rank;
						conf->master_rank_table->elem[rank].rank = rank;
						conf->master_rank_table->elem[rank].xrc_srq_num = conf->local_rank_table->elem[local_rank].xrc_srq_num;
						conf->master_rank_table->elem[rank].addr = conf->local_rank_table->elem[local_rank].addr;
						conf->master_rank_table->elem[rank].pid = 0; /* ??? todo */
						conf->master_rank_table->elem[rank].nid = conf->local_rank_table->elem[local_rank].nid;

						conf->recv_nranks ++;
					}
				}

				if (conf->recv_nranks == conf->nranks) {
					broadcast_rank_table(conf);
				}

			} else {
				/* Not the master control process. */
				// TODO
				assert(0);
			}
		}
		break;

	case QUERY_XRC_DOMAIN:
		net_intf = find_net_intf(conf,
				session->rpc_msg.query_xrc_domain.net_name);
		msg.type = REPLY_XRC_DOMAIN;
		if (net_intf) {
			ib_intf = net_intf->ib_intf;
			strcpy(msg.reply_xrc_domain.xrc_domain_fname,
				ib_intf->xrc_domain_fname);
		} else {
			strcpy(msg.reply_xrc_domain.xrc_domain_fname, "");
		}
		rpc_send(session, &msg);
		break;

	default:
		fprintf(stderr, "Got unhandled RPC message id %x\n",
			session->rpc_msg.type);
		break;
	}
}

/* Each process in a job will try to start the control process. Only
 * one must succeed. */
static int run_once(struct p4oibd_config *conf)
{
	int fd;
	int err;

	/* Create a file and try to lock it. */
	sprintf(lock_filename, "/tmp/p4oibd-JID-%d.lck", conf->jobid);
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
	printf("    -x | --xrc          XRC port (default = %d)\n", PTL_XRC_PORT);
	printf("    -n | --nid          NID\n");
	printf("    -l | --log level    rank table\n");
	printf("    -j | --jid          job ID\n");
	printf("    -t | --local_nranks number of local ranks\n");
	printf("    -s | --nranks       number of ranks\n");
	printf("    -m | --master-nid   NID of master control daemon\n");
	printf("    -u | --num_nids     number of nids (nodes)\n");
}

static int arg_process(struct p4oibd_config *conf, int argc, char *argv[])
{
	int c;
	int opt_index = 0;
	const char *opt_string = "hvp:n:j:l:t:s:m:x:u:";
	static const struct option opt_long[] = {
		{"help", 0, 0, 'h'},
		{"verbose", 0, 0, 'v'},
		{"port", 1, 0, 'p'},
		{"nid", 1, 0, 'n'},
		{"master-nid", 1, 0, 'm'},
		{"jid", 1, 0, 'j'},
		{"log", 1, 0, 'l'},
		{"local_nranks", 1, 0, 't'},
		{"nranks", 1, 0, 's'},
		{"num_nids", 1, 0, 'u'},
		{0, 0, 0, 0}
	};

	progname = rindex(argv[0], '/') + 1;
	if (!progname)
		progname = argv[0];

	conf->ctl_port = PTL_CTL_PORT;
	conf->xrc_port = PTL_XRC_PORT;
	conf->shmem.fd = -1;

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
			conf->ctl_port = strtol(optarg, NULL, 0);
			break;

		case 'n':
			conf->nid = strtol(optarg, NULL, 0);
			break;

		case 'm':
			conf->master_nid = strtol(optarg, NULL, 0);
			break;

		case 'j':
			conf->jobid = strtol(optarg, NULL, 0);
			break;

		case 'l':
			ptl_log_level = strtol(optarg, NULL, 0);
			break;

		case 't':
			conf->local_nranks = strtol(optarg, NULL, 0);
			break;

		case 's':
			conf->nranks = strtol(optarg, NULL, 0);
			break;

		case 'x':
			conf->xrc_port = strtol(optarg, NULL, 0);
			break;

		case 'u':
			conf->num_nids = strtol(optarg, NULL, 0);
			break;

		default:
			fprintf(stderr, "unexpected option %s\n", optarg);
			goto err1;
		}
	}

	if (!conf->nid) {
		fprintf(stderr, "NID not set\n");
		return 1;
	}

	if (!conf->jobid) {
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
	struct p4oibd_config conf;

	memset(&conf, 0, sizeof(struct p4oibd_config));

	err = arg_process(&conf, argc, argv);
	if (err)
		return 1;

	if (verbose) {
		printf("%s starting with:\n", progname);
		printf("	ctl port = %d\n", conf.ctl_port);
		printf("	XRC port = %d\n", conf.xrc_port);
	}

	err = run_once(&conf);
	if (err) {
		/* Another process is already running. */
		return 1;
	}

	/* Detach the process before continuing. */
	err = daemon(0,0);
	if (err) {
		perror("Couldn't daemonize\n");
		return 1;
	}

	/* Create sessions table */
	conf.sessions = calloc(conf.local_nranks, sizeof(struct session *));
	if (!conf.sessions) {
		fprintf(stderr, "Couldn't allocate sessions\n");
		return 1;
	}

	/* Create local rank table. */
	conf.local_rank_table = calloc(1, sizeof(struct rank_table) +
								   conf.local_nranks * sizeof(struct rank_entry));
	if (!conf.local_rank_table) {
		fprintf(stderr, "Couldn't allocate local rank table\n");
		return 1;
	}
	conf.local_rank_table->size = conf.local_nranks;

	/* Create connection table. */
	conf.connect = calloc(conf.nranks,
						  sizeof (*conf.connect));
	if (!conf.connect) {
		fprintf(stderr, "Couldn't allocate connection table\n");
		return 1;
	}
	conf.local_rank_table->size = conf.local_nranks;

	err = create_shared_memory(&conf);
	if (err) {
		fprintf(stderr, "Couldn't create shared data\n");
		return 1;
	}

	my_event_loop = EV_DEFAULT;

	err = create_ib_resources(&conf);
	if (err) {
		fprintf(stderr, "Couldn't create some IB resources\n");
		return 1;
	}

	err = rpc_init(rpc_type_server, -1, conf.ctl_port, &conf.rpc, rpc_callback, &conf);
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

	/* Run the event loop. */
	ev_run(my_event_loop, 0);

	rpc_fini(conf.rpc);

	destroy_ib_resources(&conf);

	printf("%s stopped\n", progname);

	remove(lock_filename);

	return 0;

err1:
	return 1;
}
