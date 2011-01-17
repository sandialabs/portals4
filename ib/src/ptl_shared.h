/* NID */
struct nid_entry {
	ptl_nid_t nid;
	union {
		struct sockaddr addr;
		struct sockaddr_in addr_in;
		struct sockaddr_in6 addr_in6;
	};
};

struct nid_table {
	unsigned int size;
	struct nid_entry elem[0];
};

/* RANK */
struct rank_entry {
	ptl_rank_t rank;
	ptl_nid_t nid;
	ptl_pid_t pid;

	/* Connection from remote rank. */
	struct rdma_cm_id *cm_id;
};

struct rank_table {
	unsigned int size;			/* number of elements */
	struct rank_entry elem[0];
};

/* Configuration shared between the control process and the portals
 * applications. Offsets are relative to the
 * start of the shared memory. */
struct shared_config {
	off_t rank_table_offset;
	size_t rank_table_size;
	
	off_t nid_table_offset;
	size_t nid_table_size;
};
