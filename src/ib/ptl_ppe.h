/* Included by the light library and the PPE. */

#ifdef WITH_PPE

/* XPMEM mapping. Maybe this structure should be split into 2
 * differents ones: one for the client and one for the PPE. */
struct xpmem_map {
	/* From source process. */
	const void *source_addr;
	size_t size;

	off_t offset;				/* from start of segid to source_addr */
	xpmem_segid_t segid;

	/* On dest process. */
	void *ptr_attach;			/* registered address with xpmem_attach */

	/* Both. */
    struct xpmem_addr addr;
};

#define PPE_SOCKET_NAME "/tmp/portals4-ppe"

enum ppe_op {
	OP_PtlAtomic = 1,
	OP_PtlAtomicSync,
	OP_PtlCTAlloc,
	OP_PtlCTCancelTriggered,
	OP_PtlCTFree,
	OP_PtlCTInc,
	OP_PtlCTSet,
	OP_PtlEQAlloc,
	OP_PtlEQFree,
	OP_PtlFetchAtomic,
	OP_PtlFini,
	OP_PtlGet,
	OP_PtlGetId,
	OP_PtlGetMap,
	OP_PtlGetPhysId,
	OP_PtlGetUid,
	OP_PtlInit,
	OP_PtlLEAppend,
	OP_PtlLESearch,
	OP_PtlLEUnlink,
	OP_PtlMDBind,
	OP_PtlMDRelease,
	OP_PtlMEAppend,
	OP_PtlMESearch,
	OP_PtlMEUnlink,
	OP_PtlNIFini,
	OP_PtlNIHandle,
	OP_PtlNIInit,
	OP_PtlNIStatus,
	OP_PtlPTAlloc,
	OP_PtlPTDisable,
	OP_PtlPTEnable,
	OP_PtlPTFree,
	OP_PtlPut,
	OP_PtlSetMap,
	OP_PtlSwap,
	OP_PtlTriggeredAtomic,
	OP_PtlTriggeredCTInc,
	OP_PtlTriggeredCTSet,
	OP_PtlTriggeredFetchAtomic,
	OP_PtlTriggeredGet,
	OP_PtlTriggeredPut,
	OP_PtlTriggeredSwap,
};

/* Messages exchanged between the PPE and the clients. */
struct ppe_msg {
	int ret;

	union {
		struct {
			ptl_interface_t        iface;
			unsigned int           options;
			ptl_pid_t              pid;
			int with_desired;
			ptl_ni_limits_t desired;
			ptl_ni_limits_t actual;
			ptl_handle_ni_t ni_handle;
		} PtlNIInit;

		struct {
			ptl_handle_ni_t ni_handle;
		} PtlNIFini;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_sr_index_t  status_register;
			ptl_sr_value_t status;
		} PtlNIStatus;

		struct {
			ptl_handle_any_t handle;
			ptl_handle_ni_t ni_handle;
		} PtlNIHandle;

		struct {
			ptl_handle_ni_t      ni_handle;
			ptl_size_t           map_size;
			const ptl_process_t *mapping;
		} PtlSetMap;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_size_t      map_size;
			ptl_process_t *mapping;
			ptl_size_t     actual_map_size;
		} PtlGetMap;

		struct {
			ptl_handle_ni_t ni_handle;
			unsigned int    options;
			ptl_handle_eq_t eq_handle;
			ptl_pt_index_t  pt_index_req;
			ptl_pt_index_t  pt_index;
		} PtlPTAlloc;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_pt_index_t  pt_index;
		} PtlPTFree;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_pt_index_t  pt_index;
		}  PtlPTDisable;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_pt_index_t  pt_index;
		} PtlPTEnable;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_uid_t       uid;
		} PtlGetUid;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_process_t  id;
		} PtlGetId;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_process_t  id;
		} PtlGetPhysId;

		struct {
			ptl_handle_ni_t  ni_handle;
			ptl_md_t   md;
			ptl_handle_md_t  md_handle;
		} PtlMDBind;

		struct {
			ptl_handle_md_t md_handle;
		} PtlMDRelease;

		struct {
			ptl_handle_ni_t  ni_handle;
			ptl_pt_index_t   pt_index;
			ptl_le_t  le;
			ptl_list_t       ptl_list;
			void            *user_ptr;
			ptl_handle_le_t  le_handle;
		} PtlLEAppend;

		struct {
			ptl_handle_le_t le_handle;
		} PtlLEUnlink;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_pt_index_t  pt_index;
			ptl_le_t  le;
			ptl_search_op_t ptl_search_op;
			void           *user_ptr;
		} PtlLESearch;

		struct {
			ptl_handle_ni_t  ni_handle;
			ptl_pt_index_t   pt_index;
			ptl_me_t   me;
			ptl_list_t       ptl_list;
			void            *user_ptr;
			ptl_handle_me_t  me_handle;
		} PtlMEAppend;

		struct {
			ptl_handle_me_t me_handle;
		} PtlMEUnlink;

		struct {
			ptl_handle_ni_t ni_handle;
			ptl_pt_index_t  pt_index;
			ptl_me_t me;
			ptl_search_op_t ptl_search_op;
			void           *user_ptr;
		} PtlMESearch;

		struct {
			ptl_handle_ni_t  ni_handle;
			ptl_handle_ct_t  ct_handle;
			struct xpmem_map ct_mapping;
		} PtlCTAlloc;

		struct {
			ptl_handle_ct_t ct_handle;
		} PtlCTFree;

		struct {
			ptl_handle_ct_t ct_handle;
		} PtlCTCancelTriggered;

		struct {
			ptl_handle_ct_t ct_handle;
			ptl_ct_event_t  new_ct;
		} PtlCTSet;

		struct {
			ptl_handle_ct_t ct_handle;
			ptl_ct_event_t  increment;
		} PtlCTInc;

		struct {
			ptl_handle_md_t  md_handle;
			ptl_size_t       local_offset;
			ptl_size_t       length;
			ptl_ack_req_t    ack_req;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
			ptl_hdr_data_t   hdr_data;
		} PtlPut;

		struct {
			ptl_handle_md_t  md_handle;
			ptl_size_t       local_offset;
			ptl_size_t       length;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
		} PtlGet;

		struct {
			ptl_handle_md_t  md_handle;
			ptl_size_t       local_offset;
			ptl_size_t       length;
			ptl_ack_req_t    ack_req;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
			ptl_hdr_data_t   hdr_data;
			ptl_op_t         operation;
			ptl_datatype_t   datatype;
		} PtlAtomic;

		struct {
			ptl_handle_md_t  get_md_handle;
			ptl_size_t       local_get_offset;
			ptl_handle_md_t  put_md_handle;
			ptl_size_t       local_put_offset;
			ptl_size_t       length;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
			ptl_hdr_data_t   hdr_data;
			ptl_op_t         operation;
			ptl_datatype_t   datatype;
		} PtlFetchAtomic;

		struct {
			ptl_handle_md_t  get_md_handle;
            ptl_size_t       local_get_offset;
            ptl_handle_md_t  put_md_handle;
            ptl_size_t       local_put_offset;
            ptl_size_t       length;
            ptl_process_t    target_id;
            ptl_pt_index_t   pt_index;
            ptl_match_bits_t match_bits;
            ptl_size_t       remote_offset;
            void            *user_ptr;
            ptl_hdr_data_t   hdr_data;
            const void      *operand;
            ptl_op_t         operation;
            ptl_datatype_t   datatype;
		} PtlSwap;

		struct {
			ptl_handle_ni_t  ni_handle;
			ptl_size_t       count;
			ptl_handle_eq_t  eq_handle;
			struct xpmem_map eqe_list;
		} PtlEQAlloc;

		struct {
			ptl_handle_eq_t eq_handle;
		} PtlEQFree;

		struct {
			ptl_handle_md_t  md_handle;
			ptl_size_t       local_offset;
			ptl_size_t       length;
			ptl_ack_req_t    ack_req;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
			ptl_hdr_data_t   hdr_data;
			ptl_handle_ct_t  trig_ct_handle;
			ptl_size_t       threshold;
		} PtlTriggeredPut;

		struct {
			ptl_handle_md_t  md_handle;
			ptl_size_t       local_offset;
			ptl_size_t       length;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
			ptl_handle_ct_t  trig_ct_handle;
			ptl_size_t       threshold;
		} PtlTriggeredGet;

		struct {
			ptl_handle_md_t  md_handle;
			ptl_size_t       local_offset;
			ptl_size_t       length;
			ptl_ack_req_t    ack_req;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
			ptl_hdr_data_t   hdr_data;
			ptl_op_t         operation;
			ptl_datatype_t   datatype;
			ptl_handle_ct_t  trig_ct_handle;
			ptl_size_t       threshold;
		} PtlTriggeredAtomic;

		struct {
			ptl_handle_md_t  get_md_handle;
			ptl_size_t       local_get_offset;
			ptl_handle_md_t  put_md_handle;
			ptl_size_t       local_put_offset;
			ptl_size_t       length;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
			ptl_hdr_data_t   hdr_data;
			ptl_op_t         operation;
			ptl_datatype_t   datatype;
			ptl_handle_ct_t  trig_ct_handle;
			ptl_size_t       threshold;
		} PtlTriggeredFetchAtomic;

		struct {
			ptl_handle_md_t  get_md_handle;
			ptl_size_t       local_get_offset;
			ptl_handle_md_t  put_md_handle;
			ptl_size_t       local_put_offset;
			ptl_size_t       length;
			ptl_process_t    target_id;
			ptl_pt_index_t   pt_index;
			ptl_match_bits_t match_bits;
			ptl_size_t       remote_offset;
			void            *user_ptr;
			ptl_hdr_data_t   hdr_data;
			const void      *operand;
			ptl_op_t         operation;
			ptl_datatype_t   datatype;
			ptl_handle_ct_t  trig_ct_handle;
			ptl_size_t       threshold;
		} PtlTriggeredSwap;

		struct {
			ptl_handle_ct_t ct_handle;
			ptl_ct_event_t increment;
			ptl_handle_ct_t trig_ct_handle;
			ptl_size_t threshold;
		} PtlTriggeredCTInc;

		struct {
			ptl_handle_ct_t ct_handle;
			ptl_ct_event_t new_ct;
			ptl_handle_ct_t trig_ct_handle;
			ptl_size_t threshold;
		} PtlTriggeredCTSet;
	};
};

/**
 * A ppebuf is used to communicate between the client and the PPE. It
 * resides in shared memory,
 */
typedef struct ppebuf {
	/** base object */
	obj_t			obj;

	/** Cookie given by the PPE to that client and used for almost any
	 * communication. */
	void *cookie;

	/** Type of operation */
	enum ppe_op op;

	/** Set to 1 when the PPE has completed the request in the
	 * buffer. */
	unsigned int completed;

	/** Message from client to PPE, with response from PPE. */
	struct ppe_msg msg;
} ppebuf_t;

/* Maximum number of progress threads on the PPE. */
#define MAX_PROGRESS_THREADS 10

/* Communication PAD. Created by the PPE and shared with the clients. */
struct ppe_comm_pad {
	/* Clients enqueue ppebufs here, and PPE consummes. */
	struct {
		queue_t queue __attribute__ ((aligned (64)));
	} q[MAX_PROGRESS_THREADS];

	/* Pool of ppebufs, for clients to use. The slab itself has been
	 * mapped through XPMEM by the PPE. */
	pool_t ppebuf_pool __attribute__ ((aligned (64)));

	/* The ppebufs slab. */
	ppebuf_t ppebuf_slab[0]  __attribute__ ((aligned (4096)));
};

/* Message sent by a new client to the PPE, through the socket. */
union msg_ppe_client {
	/* Request, from client to PPE. */
	struct {
		/* Client's process ID. */
		pid_t pid;

		/* Client's whole memory space. */
		xpmem_segid_t segid;
	} req;

	/* Reply, from PPE to client. */
	struct {
		int ret;
		void *cookie;
		struct xpmem_map ppebufs_mapping;
		void *ppebufs_ppeaddr;
		int queue_index;
	} rep;
};

#endif
