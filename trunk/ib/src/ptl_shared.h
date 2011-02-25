
/* RANK */
struct rank_entry {
	ptl_rank_t rank;
	ptl_nid_t nid;
	ptl_pid_t pid;
	uint32_t xrc_srq_num;

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
};
