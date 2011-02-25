/*
 * ctl_ib - IB related stuff for the control process p4oibd.
 */

#include "ctl.h"

static struct list_head net_interfaces;	
static struct list_head ib_interfaces;	
static struct rdma_event_channel *cm_channel;
static pthread_t cm_thread;
static int cm_thread_stop;
static int cm_thread_running;


/* Find the network interface. */
struct net_intf *find_net_intf(const char *name)
{
	struct list_head *l;
	struct net_intf *intf;

	list_for_each(l, &net_interfaces) {
		intf = list_entry(l, struct net_intf, list);

		if (strcmp(intf->name, name) == 0)
			return intf;
	};

	return NULL;
}

static int process_connect_request(struct rdma_cm_event *event)
{
	const struct cm_priv_request *priv;
	struct rdma_conn_param conn_param;
	struct ibv_qp_init_attr init_attr;
	struct rank_entry *entry;
	struct ib_intf *ib_intf;
	struct ibv_pd pd;
	struct cm_priv_response resp;

	if (!event->param.conn.private_data ||
		(event->param.conn.private_data_len <
		sizeof(struct cm_priv_request)))
		return 1;

	priv = event->param.conn.private_data;

	if (priv->src_rank >= conf.nranks)
		return 1;

	entry = &conf.master_rank_table->elem[priv->src_rank];
	ib_intf = event->listen_id->context;

	memset(&init_attr, 0, sizeof(struct ibv_qp_init_attr));
	init_attr.qp_type = IBV_QPT_XRC;
	init_attr.xrc_domain = ib_intf->xrc_domain;

	/* FZ: In the case of an XRC QP, PD is unused by rdma_create_qp, but
	 * some sanity checking is done anyway. This is a hack, and I need
	 * to fix rdma cm. */
	pd.context = event->id->verbs;

	if (rdma_create_qp(event->id, &pd, &init_attr)) {
		return 1;
	}

	entry->cm_id = event->id;

	/* todo: fill resp. */

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;
	conn_param.private_data = &resp;
	conn_param.private_data_len = sizeof(resp);

	if (rdma_accept(event->id, &conn_param)) {
	}	

	return 0;
}

/* Called when an event happenned on the event channel. 
 * Not used yet, incomplete. */
static void process_cm_event(void)
{
	struct rdma_cm_event *event;

	if (rdma_get_cm_event(cm_channel, &event)) 
		return;

	switch(event->event) {
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		process_connect_request(event);
		break;

	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_ESTABLISHED:
	default:
		break;
	};

	rdma_ack_cm_event(event);

	return;
}

static void *cm_task(void *arg)
{
	int nfd;
	fd_set readfds;
	struct timeval timeout;

	FD_ZERO(&readfds);
	FD_SET(cm_channel->fd, &readfds);

	printf("cm event thread running\n");

	while (!cm_thread_stop) {
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000000;

		nfd = select(cm_channel->fd + 1, &readfds, NULL, NULL,
				&timeout);
		if (nfd < 0) {
			perror("select");
			break;
		}

		if (nfd)
			process_cm_event();
	}

	pthread_exit(0);
}

static int net_dir_filter(const struct dirent *entry)
{
	return strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..");
}

static char *net_to_ibdev(const char *netdev, char *name_buf, int name_buf_size)
{
	char dirname[IF_NAMESIZE + 24];
	struct dirent **namelist = NULL;
	int num_entries;
	int i;

	snprintf(dirname, sizeof(dirname), "/sys/class/net/%s/device/infiniband", netdev);

	num_entries = scandir(dirname, &namelist, net_dir_filter, alphasort);

	if (num_entries > 0)
		strncpy(name_buf, namelist[0]->d_name, name_buf_size);
	
	for (i=0; i < num_entries; i++)
		if (namelist && namelist[i])
			free(namelist[i]);

	if (namelist)
		free(namelist);

	return (num_entries > 0) ? name_buf : NULL;
}

static char *ib_to_netdev(char *ibdev_path, char *name_buf, int name_buf_size)
{
	char filename[64];
	FILE *fp;
	size_t nread;

	snprintf(filename, sizeof(filename), "%s/parent", ibdev_path);
	filename[63] = 0;

	fp = fopen(filename, "r");
	if (!fp)
		return NULL;

	nread =  fread(name_buf, 1, name_buf_size, fp);
	fclose(fp);
	if (nread <= 1)
		return NULL;

	/* convert \n to 0 */
	name_buf[nread - 1] = 0;

	return name_buf;
}

static struct ib_intf *find_ib_intf(const char *ib_dev_name)
{
	struct list_head *l;
	struct ib_intf *intf;

	list_for_each(l, &ib_interfaces) {
		intf = list_entry(l, struct ib_intf, list);

		if (strcmp(intf->name, ib_dev_name) == 0)
			return intf;
	};

	return NULL;
}

/* Retrieve the list of IB interfaces available on this machine. */
int create_ib_resources(void)
{
	struct ib_intf *ib_intf;
	struct net_intf *net_intf;
	char name_buf[64];
	char *dev_name;
	struct sockaddr_in sin;
	struct list_head *l;
	int i, j;
	int err;
	int num_ib_device;
	struct ibv_device **ib_device_list;
	struct if_nameindex *net_device_list;

	ptl_info("entering create_ib_resources\n");

	INIT_LIST_HEAD(&net_interfaces);
	INIT_LIST_HEAD(&ib_interfaces);

	/* get list of ib devices */
	ib_device_list = ibv_get_device_list(&num_ib_device);
	if (!ib_device_list || !num_ib_device) {
		ptl_warn("Unable to find any ib devices\n");
		return 1;
	}

	/* get list of network devices */
	net_device_list = if_nameindex();
	if (!net_device_list) {
		ptl_warn("Unable to find any network devices\n");
		return 1;
	}

	/* match ib and network devices to see if they match */
	for (i = 0; i < num_ib_device; i++) {
		for(j = 0; net_device_list[j].if_index != 0 &&
			net_device_list[j].if_name != NULL; j++) {

			/* infiniband transport (e.g. ib0) */
			dev_name = net_to_ibdev(net_device_list[j].if_name,
					name_buf, sizeof(name_buf));

			if (dev_name &&
				!strcmp(dev_name, ib_device_list[i]->name)) {
				ptl_info("%s -> %s\n", ib_device_list[i]->name,
					 net_device_list[j].if_name);
				goto found_one;
			}

			/* ethernet transport (e.g. rxe0) */
			dev_name = ib_to_netdev(ib_device_list[i]->ibdev_path,
					name_buf, sizeof(name_buf));
			if (dev_name &&
				!strcmp(dev_name, net_device_list[j].if_name)) {
				ptl_info("%s -> %s\n", ib_device_list[i]->name,
					 net_device_list[j].if_name);
				goto found_one;
			}

			continue;
found_one:
			/* build net_intf struct */
			net_intf = calloc(1, sizeof(*net_intf));
			if (!net_intf) {
				ptl_fatal("Unable to allocate memory for "
					"net_intf\n");
				goto err;
			}

			strcpy(net_intf->name, net_device_list[j].if_name);
			net_intf->index = net_device_list[j].if_index;

			/* find/build ib_intf struct */
			ib_intf = find_ib_intf(ib_device_list[i]->name);
			if (ib_intf) {
				net_intf->ib_intf = ib_intf;
				goto found_ib_intf;
			}

			ib_intf = calloc(1, sizeof(*ib_intf));
			if (!ib_intf) {
				ptl_fatal("Unable to allocate memory for "
					"ib_intf\n");
				free(net_intf);
				goto err;
			}
			net_intf->ib_intf = ib_intf;

			strcpy(ib_intf->name, ib_device_list[i]->name);
			ib_intf->xrc_domain_fd = -1;

			ib_intf->ibv_context =
					ibv_open_device(ib_device_list[i]);
			if (!ib_intf->ibv_context) {
				ptl_fatal("Unable to open %s\n",
					ib_device_list[i]->name);
				free(ib_intf);
				free(net_intf);
				goto err;
			}

			sprintf(ib_intf->xrc_domain_fname,
				"/tmp/p4oibd-%d-XXXXXX", getpid());
			ib_intf->xrc_domain_fd =
				mkstemp(ib_intf->xrc_domain_fname);
			if (ib_intf->xrc_domain_fd < 0) {
				ptl_fatal("Unable to create xrc fd for %s\n",
					ib_intf->xrc_domain_fname);
				ibv_close_device(ib_intf->ibv_context);
				free(ib_intf);
				free(net_intf);
				goto err;
			}

			ib_intf->xrc_domain =
				ibv_open_xrc_domain(ib_intf->ibv_context,
					ib_intf->xrc_domain_fd,
					O_CREAT | O_EXCL);
			if (!ib_intf->xrc_domain) {
				ptl_fatal("Unable to open xrc domain\n");
				close(ib_intf->xrc_domain_fd);
				ibv_close_device(ib_intf->ibv_context);
				free(ib_intf);
				free(net_intf);
				goto err;
			}

			list_add(&ib_intf->list, &ib_interfaces);
found_ib_intf:
			list_add(&net_intf->list, &net_interfaces);
		}
	}

	if (list_empty(&net_interfaces)) {
		ptl_warn("No suitable ib interfaces found\n");
		return 1;
	}

	/* Create the RDMA CM listen endpoint. */
	cm_channel = rdma_create_event_channel();
	if (!cm_channel) {
		ptl_fatal("unable to create rdma event channel\n");
		return 1;
	}

	/* Listen on each interface. */
	list_for_each(l, &ib_interfaces) {
		ib_intf = list_entry(l, struct ib_intf, list);

		/* Listen on that interface. */
		if (rdma_create_id(cm_channel, &ib_intf->listen_id,
						   ib_intf, RDMA_PS_TCP)) {
			continue;
		}

		/* TODO: bind on the right address. */ 
		sin.sin_addr.s_addr = 0;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(conf.ctl_port);
		if (rdma_bind_addr(ib_intf->listen_id,
			(struct sockaddr *)&sin)) {
			rdma_destroy_id(ib_intf->listen_id);
			continue;
		}

		if (rdma_listen(ib_intf->listen_id, 0)) {
			rdma_destroy_id(ib_intf->listen_id);
			continue;
		}

		ptl_info("listening on %s\n", ib_intf->name);
	}

	err = pthread_create(&cm_thread, NULL, cm_task, NULL);
	if (err) {
		ptl_fatal("unable to create CM thread\n");
		goto err;
	}
	cm_thread_running = 1;

	ibv_free_device_list(ib_device_list);
	ptl_info("ok\n");
	return 0;

 err:
	destroy_ib_resources();
	ibv_free_device_list(ib_device_list);
	return 1;
}

void destroy_ib_resources(void)
{
	struct list_head *l;
	struct ib_intf *ib_intf;
	struct net_intf *net_intf;

	if (cm_thread_running) {
		cm_thread_stop = 1;
		pthread_join(cm_thread, NULL);
		cm_thread_running = 0;
	}

	/* TODO: free and unlink intf. list_for_each_safe ? */

	list_for_each(l, &ib_interfaces) {
		ib_intf = list_entry(l, struct ib_intf, list);

		if (ib_intf->listen_id) {
			rdma_destroy_id(ib_intf->listen_id);
			ib_intf->listen_id = NULL;
		}

		if (ib_intf->xrc_domain_fd != -1) {
			close(ib_intf->xrc_domain_fd);
			unlink(ib_intf->xrc_domain_fname);
		}
	}

	list_for_each(l, &net_interfaces) {
		net_intf = list_entry(l, struct net_intf, list);
	}

	if (cm_channel) {
		rdma_destroy_event_channel(cm_channel);
		cm_channel = NULL;
	}
}
