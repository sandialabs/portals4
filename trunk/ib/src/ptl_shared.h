/*
 * Shared structures between the control process and the clients.
 */

/* RANK */
struct rank_entry {
	ptl_rank_t rank;
	ptl_nid_t nid;
	ptl_pid_t pid;
	uint32_t xrc_srq_num;
	in_addr_t addr;				/* IPV4 address, in network order */
};

struct rank_table {
	unsigned int num_entries;
	struct rank_entry elem[0];
};

/* Configuration shared between the control process and the portals
 * applications. Offsets are relative to the
 * start of the shared memory. */
struct shared_config {
	off_t rank_table_offset;	/* offset in bytes */
	size_t rank_table_size;		/* table size in bytes */
};

/* RDMA CM private data */
struct cm_priv_request {
	ptl_rank_t src_rank;		/* rank requesting that connection */
};

/* Event loop */
struct ev_loop *my_event_loop;

void session_list_is_empty(void);
