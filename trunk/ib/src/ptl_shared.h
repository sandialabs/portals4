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
	ptl_process_t src_id;		/* rank or NID/PID requesting that connection */
};

/* In current implementation a NID is just an IPv4 address in host order. */
static inline in_addr_t nid_to_addr(ptl_nid_t nid)
{
	return htonl(nid);
}

static inline ptl_nid_t addr_to_nid(in_addr_t addr)
{
	return ntohl(addr);
}

/* A PID is a port in host order. */
static inline uint16_t pid_to_port(ptl_pid_t pid)
{
	if (pid == PTL_PID_ANY) {
		return 0;
	} else {
		return htons(pid);
	}
}

void session_list_is_empty(void);
