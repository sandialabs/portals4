/*!
 * @file portals4.h
 * @brief Portals4 Reference Implementation
 *
 * Copyright (c) 2010 Sandia Corporation
 */

#ifndef PORTALS4_H
#define PORTALS4_H

#include <stdint.h> // assume C99, for uint64_t

/*****************
 * Return Values *
 *****************/
/*! The set of all possible return codes. */
enum ptl_retvals {
    PTL_OK=0,		/*!< Indicates success */
    PTL_FAIL,		/*!< Indicates a non-specific error */
    PTL_NO_INIT,	/*!< Init has not yet completed successfully. */
    PTL_ARG_INVALID,	/*!< One of the arguments is invalid. */
    PTL_SEGV,		/*!< Caught a segfault. */
    PTL_IN_USE,		/*!< The specified resource is currently in use. */
    PTL_NO_SPACE,	/*!< Sufficient memory for specified action was not available. */
    PTL_LIST_TOO_LONG,	/*!< The resulting list is too long (maximum is interface-dependent). */
};

/**************
 * Base Types *
 **************/
typedef uint64_t	ptl_size_t; /*!< Unsigned 64-bit integral type used for representing sizes. */
typedef unsigned int	ptl_pt_index_t; /*!< Integral type used for representing portal table indices. */
typedef uint64_t	ptl_match_bits_t; /*!< Capable of holding unsigned 64-bit integer values. */
typedef unsigned int	ptl_interface_t; /*!< Integral type used for identifying different network interfaces. */
typedef unsigned int	ptl_nid_t; /*!< Integral type used for representing node identifiers. */
typedef unsigned int	ptl_pid_t; /*!< Integral type used for representing
				     process identifiers when physical
				     addressing is used in the network
				     interface (PTL_NI_PHYSICAL is set). */
typedef unsigned int	ptl_rank_t; /*!< Integral type used for representing
				      process identifiers when logical
				      addressing is used in the network
				      interface (PTL_NI_LOGICAL is set). */
typedef unsigned int	ptl_uid_t; /*!< Integral type for representing user identifiers. */
typedef unsigned int	ptl_jid_t; /*!< Integral type for representing job identifiers. */
typedef unsigned int	ptl_sr_index_t; /*!< Defines the types of indexes that can be used to access the status registers. */
typedef int		ptl_sr_value_t; /*!< Signed integral type that defines the types of values held in status registers. */
/* Handles */
typedef int		ptl_handle_ni_t; /*!< A network interface handle */
typedef int		ptl_handle_eq_t; /*!< An event queue handle */
typedef int		ptl_handle_ct_t; /*!< A counting type event handle */
typedef int		ptl_handle_md_t; /*!< A memory descriptor handle */
typedef int		ptl_handle_le_t; /*!< A list entry handle */
typedef int		ptl_handle_me_t; /*!< A match list entry handle */
/*!
 * @union ptl_handle_any_t
 * @brief The generic handle type.
 * This union can represent any type of handle.
 */
typedef union {
    ptl_handle_ni_t ni; /*!< A network interface handle */
    ptl_handle_eq_t eq; /*!< An event queue handle */
    ptl_handle_ct_t ct; /*!< A counting type event handle */
    ptl_handle_md_t md; /*!< A memory descriptor handle */
    ptl_handle_le_t le; /*!< A list entry handle */
    ptl_handle_me_t me; /*!< A match list entry handle */
}			ptl_handle_any_t;
/*!
 * @union ptl_process_id_t
 * @brief A union for representing processes either physically or logically.
 * The physical address uses two identifiers to represent the process
 * identifier: a node identifier \a nid and a process identifier \a pid. In
 * turn, a logical address uses a logical index within a translation table
 * specified by the application (the \a rank) to identify another process.
 * @ingroup PI
 */
typedef union {
    struct {
	ptl_nid_t nid;	/*!< The node identifier. */
	ptl_pid_t pid;	/*!< The process identifier. */
    } phys;		/*!< The physical representation of a node. */
    ptl_rank_t rank;	/*!< The logical representation of a node. */
}			ptl_process_id_t;
/*!
 * @struct ptl_md_t
 * @brief Defines the visible parts of a memory descriptor. Values of this type
 *	are used to initialize the memory descriptors.
 *
 *	A memory descriptor contains information about a region of a process'
 *	memory and optionally points to an event queue where information about
 *	the operations performed on the memory descriptor are recorded. Memory
 *	descriptors are initiator side resources that are used to encapsulate
 *	an association with a network interface (NI) with a description of a
 *	memory region. They provide an interface to register memory (for
 *	operating systems that require it) and to carry that information across
 *	multiple operations (an MD is persistent until released). PtlMDBind()
 *	is used to create a memory descriptor and PtlMDRelease() is used to
 *	unlink and release the resources associated wiht a memory descriptor.
 * @ingroup MD
 */
typedef struct {
    void *	    start; /*!< Specify the starting address for the memory
			     region associated with the memory descriptor.
			     There are no alignment restrictions on the
			     starting address; although unaligned messages may
			     be slower (i.e. lower bandwidth and/or longer
			     latency) on some implementations. */
    ptl_size_t	    length; /*!< Specifies the length of the memory region
			      associated with the memory descriptor. */
    unsigned int    options; /*!<
    Specifies the behavior of the memory descriptor. Options include the use of
    scatter/gather vectors and disabling of end events associated with this
    memory descriptor. Values for this argument can be constructed using a
    bitwise OR of the following values:
    - \c PTL_MD_EVENT_DISABLE
    - \c PTL_MD_EVENT_SUCCESS_DISABLE
    - \c PTL_MD_EVENT_CT_SEND
    - \c PTL_MD_EVENT_CT_REPLY
    - \c PTL_MD_EVENT_CT_ACK
    - \c PTL_MD_UNORDERED
    - \c PTL_MD_REMOTE_FAILURE_DISABLE
    - \c PTL_IOVEC
			       */
    ptl_handle_eq_t eq_handle; /*!< The event queue handle used to log the
				 operations performed on the memory region. if
				 this member is \c PTL_EQ_NONE, operations
				 performed on this memory descriptor are not
				 logged. */
    ptl_handle_ct_t ct_handle; /*!< A handle for counting type events
				 associated with the memory region. If this
				 argument is \c PTL_CT_NONE, operations
				 performed on this memory descriptor are not
				 counted. */
}			ptl_md_t;
/*!
 * @struct ptl_iovec_t
 * @brief Used to describe scatter/gather buffers of a match list entry or
 *	memory descriptor in conjunction with the \c PTL_IOVEC option. The
 *	ptl_iovec_t type is intended to be a type definition of the \c struct
 *	\c iovec type on systems that already support this type.
 * @ingroup MD
 * @note Performance conscious users should not mix offsets (local or remote)
 *	with ptl_iovec_t. While this is a supported operation, it is unlikely
 *	to perform well in most implementations.
 * @implnote The implementation is required to support the mixing of the
 *	ptl_iovec_t type with offsets (local and remote); howeve,r it will be
 *	difficult to make this perform well in the general case. The correct
 *	behavior in this scenario is to treat the region described by the
 *	ptl_iovec_t type as if it were a single contiguous region. In some
 *	cases, this may require walking the entire scatter/gather list to find
 *	the correct location for depositing the data.
 */
typedef struct {
    void* iov_base; /*!< The byte aligned start address of the vector element. */
    ptl_size_t iov_len; /*!< The length (in bytes) of the vector element. */
} ptl_iovec_t;
/*! @struct ptl_ac_id_t
 * @brief To facilitate access control to both list entries and match list
 *	entries, the ptl_ac_id_t is defined as a union of a job ID and a user
 *	ID. A ptl_ac_id_t is attached to each list entry or match list entry to
 *	control which user (or which job, as selected by an option) can access
 *	the entry. Either field can specify a wildcard.
 * @ingroup LEL
 */
typedef union {
    ptl_jid_t	jid; /*!< The user identifier of the \a initiator that may
		       access the associated list entry or match list entry.
		       This may be set to \c PTL_UID_ANY to allow access by any
		       user. */
    ptl_uid_t	uid; /*!< The job identifier of the \a initiator that may
		       access the associated list entry or match list entry.
		       This may be set to \c PTL_JID_ANY to allow access by any
		       job. */
} ptl_ac_id_t;
/*!
 * @struct ptl_le_t
 * @brief Defines the visible parts of a list entry. Values of this type are
 *	used to initialize the list entries.
 *
 * @ingroup LEL
 * @note The list entry (LE) has a number of fields in common with the memory
 *	descriptor (MD). The overlapping fields have the same meaning in the LE
 *	as in the MD; however, since initiator and target resources are
 *	decoupled, the MD is not a proper subset of the LE, and the options
 *	field has different meaning based on whether it is used at an initiator
 *	or target, it was deemed undesirable and cumbersome to include a
 *	"target MD" structure that would be included as an entry in the LE.
 * @note The default behavior from Portals 3.3 (no truncation and locally
 *	managed offsets) has been changed to match the default semantics of the
 *	list entry, which does not provide matching.
 */
typedef struct {
    void*	    start; /*!< Specify the starting address of the memory
			     region associated with the match list entry. Can
			     be \c NULL provided that \a length is zero.
			     Zero-length buffers (NULL LE) are useful to record
			     events. There are no alignment restrictions on
			     buffer alignment, the starting address, or the
			     length of the region; although messages that are
			     not natively aligned (e.g. to a four byte or eight
			     byte boundary) may be slower (i.e. lower bandwidth
			     and/or longer latency) on some implementations. */
    ptl_size_t	    length; /*!< Specify the length of the memory region
			      associated with the match list entry. */
    ptl_handle_ct_t ct_handle; /*!< A handle for counting type events
				 associated with the memory region. If this
				 argument is \c PTL_CT_NONE, operations
				 performed on this list entry are not counted.
				 */
    ptl_ac_id_t	    ac_id; /*!< Specifies either the user ID or job ID (as
			     selected by the \a options) that may access this
			     list entry. Either the user ID or job ID may be
			     set to a wildcard (\c PTL_UID_ANY or \c
			     PTL_JID_ANY). If the access control check fails,
			     then the message is dropped without modifying
			     Portals state. This is treated as a permissions
			     failure and the PtlNIStatus() register indexed by
			     \c PTL_SR_PERMISSIONS_VIOLATIONS is incremented.
			     This failure is also indicated to the initiator
			     through the \a ni_fail_type in the \c
			     PTL_EVENT_SEND event, unles the \c
			     PTL_MD_REMOTE_FAILURE_DISABLE option is set. */
    unsigned int    options; /*!< Specifies the behavior of the list entry. The
			       following options can be selected: enable put
			       operations (yes or no), enable get operations
			       (yes or no), offset management (local or
			       remote), message truncation (yes or no),
			       acknowledgement (yes or no), use scatter/gather
			       vectors and disable events. Values for this
			       argument can be constructed using a bitwise OR
			       of the following values:
 - \c PTL_LE_OP_PUT
 - \c PTL_LE_OP_GET
 - \c PTL_LE_USE_ONCE
 - \c PTL_LE_ACK_DISABLE
 - \c PTL_IOVEC
 - \c PTL_LE_EVENT_DISABLE
 - \c PTL_LE_EVENT_SUCCESS_DISABLE
 - \c PTL_LE_EVENT_OVER_DISABLE
 - \c PTL_LE_EVENT_UNLINK_DISABLE
 - \c PTL_LE_EVENT_CT_GET
 - \c PTL_LE_EVENT_CT_PUT
 - \c PTL_LE_EVENT_CT_PUT_OVERFLOW
 - \c PTL_LE_EVENT_CT_ATOMIC
 - \c PTL_LE_EVENT_CT_ATOMIC_OVERFLOW
 - \c PTL_LE_AUTH_USE_JID
 */
} ptl_le_t;

/****************************
 * Generic Option Constants *
 ****************************/
/*! Specifies that the \a start member of the relevant structure is a pointer
 * to an array of type ptl_iovec_t and the \a length member is the length of
 * the array of ptl_iovec_t elements. This allows for a scatter/gather
 * capability for memory descriptors. A scatter/gather memory descriptor
 * behaves exactly as a memory descriptor that describes a single virtually
 * contiguous region of memory. This value is re-used by several structures. */
#define PTL_IOVEC (1<<7)

/*************
 * Constants *
 *************/
extern const ptl_handle_eq_t PTL_EQ_NONE;	/*!< Indicates the absence of an event queue. */
extern const ptl_handle_ct_t PTL_CT_NONE;	/*!< Indicates the absence of a counting type event. */
extern const ptl_handle_any_t PTL_INVALID_HANDLE;	/*!< Represents an invalid handle. */
extern const ptl_interface_t PTL_IFACE_DEFAULT;	/*!< Identifies the default interface. */
extern const ptl_pid_t PTL_PID_ANY;	/*!< Matches any process identifier. */
extern const ptl_nid_t PTL_NID_ANY;	/*!< Matches any node identifier. */
extern const ptl_jid_t PTL_JID_ANY;	/*!< Matches any job identifier. */
extern const ptl_rank_t PTL_RANK_ANY;	/*!< Matches any rank. */
extern const ptl_sr_index_t PTL_SR_DROP_COUNT;	/*!< Identifies the status register that counts the dropped requests for the interface. */
extern const ptl_sr_index_t PTL_SR_PERMISSIONS_VIOLATIONS;	/*!< Identifies the status register that counts the number of attempted permission violations. */
extern const unsigned int PTL_NI_MATCHING;	/*!< Request that the interface specified in \a iface be opened with matching enabled. */
extern const unsigned int PTL_NI_NO_MATCHING;	/*!< Request that the interface specified in \a iface be opened with matching
						 * disabled. \c PTL_NI_MATCHING and \c PTL_NI_NO_MATCHING are mutually
						 * exclusive. */
extern const unsigned int PTL_NI_LOGICAL;	/*!< Request that the interface specified in \a iface be opened with logical
						 * end-point addressing (e.g.\ MPI communicator and rank or SHMEM PE). */
extern const unsigned int PTL_NI_PHYSICAL;	/*!< Request that the interface specified in \a iface be opened with physical
						 * end-point addressing (e.g.\ NID/PID). \c PTL_NI_LOGICAL and \c
						 * PTL_NI_PHYSICAL are mutually exclusive */

/******************************
 * Initialization and Cleanup *
 ******************************/
/*!
 * @addtogroup INC Initialization and Cleanup
 * @{
 * @fn PtlInit(void)
 * @brief Initializes the portals library.
 *	PtlInit must be called at least once by a process before any thread
 *	makes a portals function call but may be safely called more than once.
 *	Each call to PtlInit() increments a reference count.
 * @see PtlFini()
 * @retval PTL_OK   Indicates success.
 * @retval PTL_FAIL Indicates some sort of failure in initialization.
 */
int PtlInit(void);
/*!
 * @fn PtlFini(void)
 * @brief Allows an application to clean up after the portals library is no
 *	longer needed by a process. Each call to PtlFini() decrements the
 *	reference count that was incremented by PtlInit(). When the reference
 *	count reaches zero, all portals resources are freed. Once the portals
 *	resources are freed, calls to any of the functions defined by the
 *	portals API or use the structures set up by the portals API will result
 *	in undefined behavior. Each call to PtlInit() should be matched by a
 *	corresponding PtlFini().
 * @see PtlInit()
 */
void PtlFini(void);
/*! @} */

/**********************
 * Network Interfaces *
 **********************/
/*!
 * @addtogroup NI Network Interfaces
 * @{
 * @implnote A logical interface is very similar to a physical interface. Like
 * a physical interface, a logical interface is a "well known" interface --
 * i.e. it is a specific physical interface with a specific set of properties.
 * One additional burden placed on the implementation is the need for the
 * initiator to place 2 bits in the message header to identify to the target
 * the logical interface on which this message was sent. In addition, all
 * logical interfaces associated with a single physical interface must share a
 * single node ID and Portals process ID.
 *
 * @struct ptl_ni_limits_t
 * @brief The network interface (NI) limits type */
typedef struct {
    int max_mes; /*!< Maximum number of match list entries that can be allocated at any one time. */
    int max_mds; /*!< Maximum number of memory descriptors that can be allocated at any one time. */
    int max_cts; /*!< Maximum number of counting events that can be allocated at any one time. */
    int max_eqs; /*!< Maximum number of event queues that can be allocated at any one time. */
    int max_pt_index; /*!< Largest portal table index for this interface, valid indexes range from 0 to max_pt_index, inclusive. An interface must have a max_pt_index of at least 63. */
    int max_iovecs; /*!< Maximum number of I/O vectors for a single memory descriptor for this interface. */
    int max_me_list; /*!< Maximum number of match list entries that can be attached to any portal table index. */
    ptl_size_t max_msg_size; /*!< Maximum size (in bytes) of a message (put, get, or reply). */
    ptl_size_t max_atomic_size; /*!< Maximum size (in bytes) of an atomic operation. */
} ptl_ni_limits_t;

/*!
 * @fn PtlNIInit(ptl_interface_t iface,
 *		 unsigned int options,
 *		 ptl_pid_t pid,
 *		 ptl_ni_limits_t *desired,
 *		 ptl_ni_limits_t *actual,
 *		 ptl_size_t map_size,
 *		 ptl_process_id_t *desired_mapping,
 *		 ptl_process_id_t *actual_mapping,
 *		 ptl_handle_ni_t *ni_handle)
 * @brief Initializes the portals API for a network interface (NI). A process
 *	using portals must call this function at least once before any other
 *	functions that apply to that interface. For subsequent calls to
 *	PtlNIInit() from within the same process (either by different threads
 *	or the same thread), the desired limits will be ignored and the call
 *	will return the existing network interface handle and the actual
 *	limits. Calls to PtlNIInit() increment a reference count on the network
 *	interface and must be matched by a call to PtlNIFini().
 * @param[in] iface	    Identifies the network interface to be initialized.
 * @param[in] options	    This field contains options that are requested for
 *			    the network interface. Values for this argument can
 *			    be constructed using a bitwise OR of the values
 *			    defined below. Either \c PTL_NI_MATCHING or \c
 *			    PTL_NI_NO_MATCHING must be set, but not both.
 *			    Either \c PTL_NI_LOGICAL or \c PTL_NI_PHYSICAL must
 *			    be set, but not both.
 * @param[in] pid	    Identifies the desired process identifier (for well
 *			    known process identifiers). The value \c
 *			    PTL_PID_ANY may be used to let the portals library
 *			    select a process identifier.
 * @param[in] desired	    If not \c NULL, points to a structure that holds
 *			    the desired limits.
 * @param[out] actual	    If not \c NULL, on successful return, the location
 *			    pointed to by \a actual will hold the actual
 *			    limits.
 * @param[in] map_size	    Contains the size of the map being passed in (zero
 *			    for \c NULL). This field is ignored if the \c
 *			    PTL_NI_LOGICAL option is set.
 * @param[in] desired_mapping	If not \c NULL, points to an array of
 *				structures that holds the desired mapping of
 *				logical identifiers to NID/PID pairs. This
 *				field is ignored if the PTL_NI_LOGICAL option
 *				is \b not set.
 * @param[out] actual_mapping   If the \c PTL_NI_LOGICAL option is set,
 *				on successful return, the location pointed to
 *				by \a actual_mapping will hold the actual
 *				mapping of logical identifiers to NID/PID
 *				pairs.
 * @param[out] ni_handle	On successful return, this location
 *				will hold the interface handle.
 * @retval PTL_OK		Indicates success.
 * @retval PTL_NO_INIT		Indicates that the portals API has not been
 *				successfully initialized.
 * @retval PTL_ARG_INVALID	Indicates that either \a iface is not a valid network
 *				interface or \a pid is not a valid process
 *				identifier.
 * @retval PTL_IN_USE		Indicates that \a pid is currently in use.
 * @retval PTL_NO_SPACE		Indicates that PtlNIInit() was not able to
 *				allocate the memory required to initialize this
 *				interface.
 * @retval PTL_SEGV		Indicates that \a actual or \a ni_handle is not
 *				\c NULL or a legal address, or that \a desired
 *				is not \c NULL and does not point to a valid
 *				address.
 * @see PtlNIFini(), PtlNIStatus()
 */
int PtlNIInit(ptl_interface_t	iface,
	      unsigned int	options,
	      ptl_pid_t		pid,
	      ptl_ni_limits_t	*desired,
	      ptl_ni_limits_t	*actual,
	      ptl_size_t	map_size,
	      ptl_process_id_t	*desired_mapping,
	      ptl_process_id_t	*actual_mapping,
	      ptl_handle_ni_t	*ni_handle);
/*!
 * @fn PtlNIFini(ptl_handle_ni_t ni_handle)
 * @brief Used to release the resources allocated for a network interface.
 *	The release of network interface resources is based on a reference
 *	count that is incremented by PtlNIInit() and decremented by
 *	PtlNIFini(). Resources can only be released when the reference count
 *	reaches zero. Once the release of resources has begun, the results of
 *	pending API operations (e.g. operation initiated by another thread) for
 *	this interface are undefined. Similarly the efects of incoming
 *	operations (put, get, atomic) or return values (acknowledgement and
 *	reply) for this interface are undefined.
 * @param[in] ni_handle	    An interface handle to shut down.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a ni_handle is not a valid network
 *			    interface handle.
 * @see PtlNIInit(), PtlNIStatus()
 * @implnote If PtlNIInit() gets called more than once per logical interface,
 * then the implementation should fill in \a actual, \a actual_mapping, and \a
 * ni_handle. It should ignore \a pid. PtlGetId() can be used to retrieve the
 * \a pid.
 */
int PtlNIFini(ptl_handle_ni_t ni_handle);
/*!
 * @fn PtlNIStatus(ptl_handle_ni_t ni_handle,
 *		   ptl_sr_index_t status_register,
 *		   ptl_sr_value_t *status)
 * @brief Returns the value of a status register for the specified interface.
 * @param[in]	ni_handle	An interface handle.
 * @param[in]	status_register The index of the status register.
 * @param[out]	status		On successful return, this location will hold
 *				the current value of the status register.
 * @retval PTL_OK		Indicates success.
 * @retval PTL_NO_INIT		Indicates that the portals API has not been successfully initialized.
 * @retval PTL_ARG_INVALID	Indicates that either \a ni_handle is not a
 *				valid network interface handle or \a
 *				status_register is not a valid status register.
 * @retval PTL_SEGV		Indicates that \a status is not to a valid address.
 * @see PtlNIInit(), PtlNIFini()
 */
int PtlNIStatus(ptl_handle_ni_t ni_handle,
	        ptl_sr_index_t	status_register,
		ptl_sr_value_t	*status);
/*!
 * @fn PtlNIHandle(ptl_handle_any_t handle,
 *		   ptl_handle_ni_t *ni_handle)
 * @brief Returns the network interface handle with which the object identified
 *	by \a handle is associated. If the object identified by \a handle is a
 *	network interface, this function returns the same value it is passed.
 * @param[in]  handle	    The object handle.
 * @param[out] ni_handle    On successful return, this location will hold the
 *			    network interface handle associated with \a handle.
 * @retval PTL_OK		Indicates success.
 * @retval PTL_NO_INIT		Indicates that the portals API has not been successfully initialized.
 * @retval PTL_ARG_INVALID	Indicates that \a handle is not a valid handle.
 * @retval PTL_SEGV		Indicates that \a ni_handle is not to a valid address.
 * @implnote Every handle should encode the network interface and the object
 * identifier relative to this handle.
 */
int PtlNIHandle(ptl_handle_any_t    handle,
		ptl_handle_ni_t*    ni_handle);
/*! @} */

/************************
 * Portal Table Entries *
 ************************/
/*!
 * @addtogroup PT Portal Table Entries
 * @{ */
/*! Hint to the underlying implementation that all entries attached to this
 * portal table entry will have the \c PTL_ME_USE_ONCE or \c PTL_LE_USE_ONCE
 * option set. */
#define PTL_PT_ONLY_USE_ONCE	(1)

/*! Enable flow control on this portal table entry. */
#define PTL_PT_FLOW_CONTROL	(1<<1)
/*!
 * @fn PtlPTAlloc(ptl_handle_ni_t   ni_handle,
 *		  unsigned int	    options,
 *		  ptl_handle_eq_t   eq_handle,
 *		  ptl_pt_index_t    pt_index_req,
 *		  ptl_pt_index_t*   pt_index)
 * @brief Allocates a portal table entry and sets flags that pass options to
 *	the implementation.
 * @param[in] ni_handle	    The interface handle to use.
 * @param[in] options	    This field contains options that are requested for
 *			    the portal index. Values for this argument can be
 *			    constructed using a bitwise OR of the values \c
 *			    PTL_PT_ONLY_USE_ONCE and \c PTL_PT_FLOW_CONTROL.
 * @param[in] eq_handle	    The event queue handle used to log the operations
 *			    performed on match list entries attached to the
 *			    portal table entry. The \a eq_handle attached to a
 *			    portal table entry must refer to an event queue
 *			    containing ptl_target_event_t type events. If this
 *			    argument is \c PTL_EQ_NONE, operations performed on
 *			    this portal table entry are not logged.
 * @param[in] pt_index_req  The value of the portal index that is requested. if
 *			    the value is set to \c PTL_PT_ANY, the
 *			    implementation can return any portal index.
 * @param[out] pt_index	    On successful return, this location will hold the
 *			    portal index that has been allocated.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a ni_handle is not a valid network
 *			    interface handle.
 * @see PtlPTFree()
 */
int PtlPTAlloc(ptl_handle_ni_t  ni_handle,
	       unsigned int	options,
	       ptl_handle_eq_t  eq_handle,
	       ptl_pt_index_t   pt_index_req,
	       ptl_pt_index_t*  pt_index);
/*!
 * @fn PtlPTFree(ptl_handle_ni_t    ni_handle,
 *		 ptl_pt_index_t	    pt_index)
 * @brief Releases the resources associated with a portal table entry.
 * @param[in] ni_handle	The interface handle on which the \a pt_index should be
 *			freed.
 * @param[in] pt_index	The index of the portal table entry that is to be freed.
 * @retval PTL_OK		Indicates success.
 * @retval PTL_NO_INIT		Indicates that the portals API has not been
 *				successfully initialized.
 * @retval PTL_ARG_INVALID	Indicates that either \a pt_index is not a valid
 *				portal table index or \a ni_handle is not a
 *				valid network interface handle.
 * @retval PTL_IN_USE		Indicates that \a pt_index is currently in use
 *				(e.g. a match list entry is still attached).
 * @see PtlPTAlloc()
 */
int PtlPTFree(ptl_handle_ni_t	ni_handle,
	      ptl_pt_index_t	pt_index);
/*!
 * @fn PtlPTDisable(ptl_handle_ni_t ni_handle,
 *		    ptl_pt_index_t  pt_index)
 * @brief Indicates to an implementation that no new messages should be
 *	accepted on that portal table entry. The function blocks until the
 *	portal table entry status has been updated, all messages being actively
 *	processed are completed, and all events are posted.
 * @param[in] ni_handle	The interface handle to use.
 * @param[in] pt_index	The portal index that is to be disabled.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a ni_handle is not a valid network
 *			    interface handle.
 * @see PtlPTEnable()
 * @note After successful completion, no other messages will be accepted on
 * this portal table entry and no more events associated with this portal table
 * entry will be delivered. Replies arriving at this initiator will continue to
 * succeed.
 */
int PtlPTDisable(ptl_handle_ni_t    ni_handle,
		 ptl_pt_index_t	    pt_index);
/*!
 * @fn PtlPTEnable(ptl_handle_ni_t  ni_handle,
 *		   ptl_pt_index_t   pt_index)
 * @brief Indicates to an implementation that a previously disabled portal
 *	table entry should be re-enabled. This is used to enable portal table
 *	entries that were automatically or manually disabled. The function
 *	blocks until the portal table entry status has been updated.
 * @param[in] ni_handle	The interface handle to use.
 * @param[in] pt_index	The portal index that is to be enabled.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a ni_handle is not a valid network
 *			    interface handle.
 * @see PtlPTDisable()
 */
int PtlPTEnable(ptl_handle_ni_t	ni_handle,
		ptl_pt_index_t	pt_index);
/*! @} */
/***********************
 * User Identification *
 ***********************/
/*!
 * @addtogroup UI User Identification
 * @{
 * @fn PtlGetUid(ptl_handle_ni_t    ni_handle,
 *		 ptl_uid_t*	    uid)
 * @brief Retrieves the user identifier of a process.
 *	Every process runs on behalf of a user. User identifiers travel in the
 *	trusted portion of the header of a portals message. They can be used at
 *	the \a target to limit access via access controls.
 * @param[in] ni_handle	A network interface handle.
 * @param[out] uid	On successful return, this location will hold the user
 *			identifier for the calling process.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a ni_handle is not a valid network
 *			    interface handle.
 * @retval PTL_SEGV	    Indicates that \a uid is not a legal address.
 */
int PtlGetUid(ptl_handle_ni_t	ni_handle,
	      ptl_uid_t*	uid);
/*! @} */
/**************************
 * Process Identification *
 **************************/
/*!
 * @addtogroup PI Process Identification
 * @{
 * @fn PtlGetId(ptl_handle_ni_t	    ni_handle,
 *		ptl_process_id_t*   id)
 * @brief Retrieves the process identifier of the calling process.
 * @param[in] ni_handle	A network interface handle.
 * @param[out] id	On successful return, this location will hold the
 *			identifier for the calling process.
 * @note Note that process identifiers and ranks are dependent on the network
 *	interface(s). In particular, if a node has multiple interfaces, it may
 *	have multiple process identifiers and multiple ranks.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a ni_handle is not a valid network interface handle
 * @retval PTL_SEGV	    Indicates that \a id is not a legal address.
 */
int PtlGetId(ptl_handle_ni_t	ni_handle,
	     ptl_process_id_t*	id);
/*! @} */
/***********************
 * Process Aggregation *
 ***********************/
/*!
 * @addtogroup PA Process Aggregation
 * @{
 * @fn PtlGetJid(ptl_handle_ni_t    ni_handle,
 *		 ptl_jid_t*	    jid)
 * @brief Retrieves the job identifier of the calling process.
 *	It is useful in the context of a parallel machine to represent all of
 *	the processes in a parallel job through an aggregate identifier. The
 *	portals API provides a mechanism for supporting such job identifiers
 *	for these systems. In order to be fully supported, job identifiers must
 *	be included as a trusted part of a message header.
 *
 *	The job identifier is an opaque identifier shared between all of the
 *	distributed processes of an application running on a parallel machine.
 *	All application processes and job-specific support programs, such as
 *	the parallel job launcher, share the same job identifier. This
 *	identifier is assigned by the runtime system upon job launch and is
 *	guaranteed to be unique among application jobs currently runing on the
 *	entire distributed system. An individual serial process may be assigned
 *	a job identifier that is not shared with any other processes in the
 *	system or can be assigned the constant \c PTL_JID_NONE.
 * @param[in] ni_handle	A network interface handle.
 * @param[out] jid	On successful return, this location will hold the
 *			job identifier for the calling process. \c PTL_JID_NONE
 *			may be returned for a serial job, if a job identifier
 *			is not assigned.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a ni_handle is not a valid network interface handle
 * @retval PTL_SEGV	    Indicates that \a jid is not a legal address.
 */
int PtlGetJid(ptl_handle_ni_t	ni_handle,
	     ptl_jid_t*		jid);
/*! @} */
/**********************
 * Memory Descriptors *
 **********************/
/*!
 * @addtogroup MD Memory Descriptors
 * @{ */
/*! Specifies that this memory descriptor should not generate events. */
#define PTL_MD_EVENT_DISABLE	     (1)

/*! Specifies that this memory descriptor should not generate events that
 * indicate success. This is useful in scenarios where the application does not
 * need normal events, but does require failure information to enhance
 * reliability. */
#define PTL_MD_EVENT_SUCCESS_DISABLE (1<<1)

/*! Enable the counting of \c PTL_EVENT_SEND events. */
#define PTL_MD_EVENT_CT_SEND	     (1<<2)

/*! Enable the counting of \c PTL_EVENT_REPLY events. */
#define PTL_MD_EVENT_CT_REPLY	     (1<<3)

/*! Enable the counting of \c PTL_EVENT_ACK events. */
#define PTL_MD_EVENT_CT_ACK	     (1<<4)

/*! Indicate to the portals implementation that messages sent from this memory
 * descriptor do not have to arrive at the target in order. */
#define PTL_MD_UNORDERED	     (1<<5)

/*! Indicate to the portals implementation that faiilures requiring
 * notification from the target should not be delivered to the local
 * application. This prevents the local events (e.g. \c PTL_EVENT_SEND) from
 * having to wait for a round-trip notification before delivery. */
#define PTL_MD_REMOTE_FAILURE_DISABLE (1<<6)
/*!
 * @fn PtlMDBind(ptl_handle_ni_t    ni_handle,
 *		 ptl_md_t*	    md,
 *		 ptl_handle_md_t*   md_handle)
 * @brief Used to create a memory descriptor to be used by the \a initiator. On
 *	systems that require memory registration, the PtlMDBind() operation
 *	would invoke the approprioate memory registration functions.
 * @param[in] ni_handle	    The network interface handle with which the memory descriptor will be associated.
 * @param[in] md	    Provides initial values for the user-visible parts
 *			    of a memory descriptor. Other than its use for
 *			    initialization, there is no linkage between this
 *			    structure and the memory descriptor maintained by
 *			    the API.
 * @param[out] md_handle    On successful return, this location will hold the
 *			    newly created memory descriptor handle. The \a
 *			    md_handle argument must be a valid address and
 *			    cannot be \c NULL.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that either \a ni_handle is not a valid
 *			    network interface handle, \a md is not a legal
 *			    memory descriptor (this may happen because the
 *			    memory region defined in \a md is invalid or
 *			    because the network interface associated with the
 *			    \a eq_handle or the \a ct_handle in \a md is not
 *			    the same as the network interface, \a ni_handle),
 *			    the event queue associated with \a md is not valid,
 *			    or the counting event associated with \a md is not
 *			    valid.
 * @retval PTL_NO_SPACE	    Indicates that there is insufficient memory to
 *			    allocate the memory descriptor.
 * @retval PTL_SEGV	    Indicates that \a md_handle is not a legal address.
 * @see PtlMDRelease()
 */
int PtlMDBind(ptl_handle_ni_t	ni_handle,
	      ptl_md_t*		md,
	      ptl_handle_md_t*	md_handle);
/*!
 * @fn PtlMDRelease(ptl_handle_md_t md_handle)
 * @brief Releases the internal resources associated with a memory descriptor.
 *	(This function does not free the memory region associated with the
 *	memory descriptor; i.e., the memory the user allocated for this memory
 *	descriptor.) Only memory descriptors with no pending operations may be
 *	unlinked.
 * @implnote An implementation will be greatly simplified if the encoding of
 *	memory descriptor handles does not get reused. This makes debugging
 *	easier and it avoids race conditions between threads calling
 *	PtlMDRelease() and PtlMDBind().
 * @param[in] md_handle The memory descriptor handle to be released
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a md_handle is not a valid memory
 *			    descriptor hadle.
 * @retval PTL_IN_USE	    Indicates that \a md_handle has pending operations
 *			    and cannot be released.
 * @see PtlMDBind()
 */
int PtlMDRelease(ptl_handle_md_t md_handle);
/*! @} */

/**************************
 * List Entries and Lists *
 **************************/
/*!
 * @addtogroup LEL List Entries and Lists
 * @{
 * @enum ptl_list_t
 * @brief A behavior for list appending.
 * @see PtlLEAppend(), PtlMEAppend()
 * @ingroup LEL
 * @ingroup MLEML
 */
typedef enum {
    PTL_PRIORITY_LIST, /*!< The priority list associated with a portal table entry. */
    PTL_OVERFLOW, /*!< The overflow list associated with a portal table entry. */
    PTL_PROBE_ONLY, /*!< Do not attach to a list. Use the LE to proble the
		      overflow list, without consuming an item in the list and
		      without being attached anywhere. */
} ptl_list_t;

/*! Specifies that the list entry will respond to \p put operations. By
 * default, list entries reject \p put operations. If a \p put operation
 * targets a list entry where \c PTL_LE_OP_PUT is not set, it is treated as a
 * permissions failure. */
#define PTL_LE_OP_PUT			(1)

/*! Specifies that the list entry will respond to \p get operations. By
 * default, list entries reject \p get operations. If a \p get operations
 * targets a list entry where \c PTL_LE_OP_GET is not set, it is treated as a
 * permissions failure. @note It is not considered an error to have a list
 * entry that does not respond to either \p put or \p get operations: Every
 * list entry responds to \p reply operations. Nor is it considered an error to
 * have a list entry that responds to both \p put and \p get operations. In
 * fact, it is often desirable for a list entry used in an \p atomic operation
 * to be configured to respond to both \p put and \p get operations. */
#define PTL_LE_OP_GET			(1<<1)

/*! Specifies that the list entry will only be used once and then unlinked. If
 * this option is not set, the list entry persists until it is explicitly
 * unlinked. */
#define PTL_LE_USE_ONCE			(1<<2)

/*! Specifies that an acknowledgment should not be sent for incoming \p put
 * operations, even if requested. By default, acknowledgments are sent for \p
 * put operations that request an acknowledgment. This applies to both standard
 * and counting type events. Acknowledgments are never sent for \p get
 * operations. The data sent in the \p reply serves as an implicit
 * acknowledgment. */
#define PTL_LE_ACK_DISABLE		(1<<3)

/*! Specifies that this list entry should not generate events. */
#define PTL_LE_EVENT_DISABLE		(1<<4)

/*! Specifies that this list entry should not generate events that indicate
 * success. This is useful in scenarios where the application does not need
 * normal events, but does require failure information to enhance reliability.
 */
#define PTL_LE_EVENT_SUCCESS_DISABLE	(1<<5)

/*! Specifies that this list entry should not generate overflow list events.
 */
#define PTL_LE_EVENT_OVER_DISABLE	(1<<6)

/*! Specifies that this list entry should not gnerate unlink (\c
 * PTL_EVENT_UNLINK) or free (\c PTL_EVENT_FREE) events. */
#define PTL_LE_EVENT_UNLINK_DISABLE	(1<<8)

/*! Enable the counting of \c PTL_EVENT_GET events */
#define PTL_LE_EVENT_CT_GET		(1<<9)

/*! Enable the counting of \c PTL_EVENT_PUT events */
#define PTL_LE_EVENT_CT_PUT		(1<<10)

/*! Enable the counting of \c PTL_EVENT_PUT_OVERFLOW events */
#define PTL_LE_EVENT_CT_PUT_OVERFLOW	(1<<11)

/*! Enable the counting of \c PTL_EVENT_ATOMIC events */
#define PTL_LE_EVENT_CT_ATOMIC		(1<<12)

/*! Enable the counting of \c PTL_EVENT_ATOMIC_OVERFLOW events */
#define PTL_LE_EVENT_CT_ATOMIC_OVERFLOW (1<<13)

/*! Use job ID for authentication instead of user ID. By default, the user ID
 * must match to allow a message to access a list entry. */
#define PTL_LE_AUTH_USE_JID		(1<<14)
/*!
 * @fn PtlLEAppend(ptl_handle_ni_t  ni_handle,
 *		   ptl_pt_index_t   pt_index,
 *		   ptl_le_t	    le,
 *		   ptl_list_t	    ptl_list,
 *		   void*	    user_ptr,
 *		   ptl_handle_le_t* le_handle)
 * @brief Creates a single list entry and appends this entry to the end of the
 *	list specified by \a ptl_list associated with the portal table entry
 *	specified by \a pt_index for the portal table for \a ni_handle. if the
 *	list is currently uninitialized, the PtlLEAAppend() function creates
 *	the first entry in the list.
 *
 *	When a list entry is posted to a list, the overflow list is checked to
 *	see if a message has arrived prior to posting the list entry. if so, a
 *	\c PTL_EVENT_PUT_OVERFLOW event is generated. No searching is performed
 *	when a list entry is posted to the overflow list.
 * @param[in] ni_handle	    The interface handle to use.
 * @param[in] pt_index	    The portal table index where the list entry should
 *			    be appended.
 * @param[in] le	    Provides initial values for the user-visible parts
 *			    of a list entry. Other than its use for
 *			    initialization, there is no linkage between this
 *			    structure and the list entry maintained by the API.
 * @param[in] ptl_list	    Determines whether the list entry is appended to
 *			    the priority list, appended to the overflow list,
 *			    or simply queries the overflow list.
 * @param[in] user_ptr	    A user-specified value that is associated with each
 *			    command that can generate an event. The value does
 *			    not need to be a pointer, but must fit in the space
 *			    used by a pointer. This value (along with other
 *			    values) is recorded in events associated with
 *			    operations on this list entry.
 * @param[out] le_handle    On successful return, this location will hold the
 *			    newly created list entry handle.
 * @retval PTL_OK		Indicates success.
 * @retval PTL_NO_INIT		Indicates that the portals API has not been
 *				successfully initialized.
 * @retval PTL_ARG_INVALID	Indicates that either \a ni_handle is not a
 *				valid network interface handle or \a pt_index
 *				is not a valid portal table index.
 * @retval PTL_NO_SPACE		Indicates that there is insufficient memory to
 *				allocate the match list entry.
 * @retval PTL_LE_LIST_TOO_LONG	Indicates that the resulting list is too long.
 *				The maximum length for a list is defined by the
 *				interface.
 * @see PtlLEUnlink()
 */
int PtlLEAppend(ptl_handle_ni_t	    ni_handle,
		ptl_pt_index_t	    pt_index,
		ptl_le_t	    le,
		ptl_list_t	    ptl_list,
		void*		    user_ptr,
		ptl_handle_le_t*    le_handle);
/*!
 * @fn PtlLEUnlink(ptl_handle_le_t le_handle)
 * @brief Used to unlink a list entry from a list. This operation also releases
 *	any resources associated with the list entry. It is an error to use the
 *	list entry handle after calling PtlLEUnlink().
 * @param[in] le_handle	The list entry handle to be unlinked.
 * @note If this list entry has pending operations; e.g., an unfinished reply
 *	operation, then PtlLEUnlink() will return \c PTL_IN_USE, and the
 *	list entry will not be unlinked. This essentially creates a race
 *	between the application retrying the unlink operation and a new
 *	operation arriving. This is believed to be reasonable as the
 *	application rarely wants to unlink an LE while new operations are
 *	arriving to it.
 * @retval PTL_OK	    Indicates success.
 * @retval PTL_NO_INIT	    Indicates that the portals API has not been
 *			    successfully initialized.
 * @retval PTL_ARG_INVALID  Indicates that \a le_handle is not a valid
 *			    list entry handle.
 * @retval PTL_IN_USE	    Indicates that the list entry has pending
 *			    operations and cannot be unlinked.
 * @see PtlLEAppend()
 */
int PtlLEUnlink(ptl_handle_le_t le_handle);
/*! @} */

/********************************************
 * Matching List Entries and Matching Lists *
 ********************************************/
/*!
 * @addtogroup MLEML Matching List Entries and Matching Lists
 * @{
 */
/*! Specifies that the match list entry will respond to \p put operations. By
 * default, match list entries reject \p put operations. If a \p put operation
 * targets a list entry where \c PTL_ME_OP_PUT is not set, it is treated as a
 * permissions failure. */
#define PTL_ME_OP_PUT			(1)

/*! Specifies that the match list entry will respond to \p get operations. by
 * default, match list entries reject \p get operations. If a \p get operation
 * targets a list entry where \c PTL_ME_OP_GET is not set, it is treated as a
 * permissions failure.
 * @note It is not considered an error to have a match list entry that does not
 *	respond to either \p put or \p get operations: Every match list entry
 *	responds to \p reply operations. Nor is it considered an error to have a
 *	match list entry that responds to both \p put and \p get operations. In fact,
 *	it is often desirable for a match list entry used in an \p atomic
 *	operation to be configured to respond to both \p put and \p get operations. */
#define PTL_ME_OP_GET			(1<<1)

/*! Specifies that the offset used in accessing the memory region is managed
 * locally. By default, the offset is in the incoming message. When the offset
 * is maintained locally, the offset is incremented by the length of the
 * request so that the next operation (\p put and/or \p get) will access the next
 * part of the memory region.
 * @note Only one offset variable exists per match list entry. If both \p put and
 *	\p get operations are performed on a match list entry, the value of that
 *	single variable is updated each time. */
#define PTL_ME_MANAGE_LOCAL		(1<<2)

/*! Specifies that the length provided in the incoming request cannot be
 * reduced to match the memory available in the region. This can cause the
 * match to fail. (The memory available in a memory region is determined by
 * subtracting the offset from the length of the memory region.) By default, if
 * the length in the incoming operation is greater than the amount of memory
 * available, the operation is truncated. */
#define PTL_ME_NO_TRUNCATE		(1<<3)

/*! Specifies that the match list entry will only be used once and then
 * unlinked. If this option is not set, the match list entry persists until
 * another unlink condition is triggered. */
#define PTL_ME_USE_ONCE			(1<<4)

/*! Indicates that messages deposited into this match list entry may be
 * aligned by the implementation to a performance optimizing boundary.
 * Essentially, this is a performance hint to the implementation to indicate
 * that the application does not care about the specific placement of the data.
 * This option is only relevant when the \c PTL_ME_MANAGE_LOCAL option is set.
 * */
#define PTL_ME_MAY_ALIGN		(1<<5)

/*! Specifies that an \p acknowledgment should \e not be sent for incoming \p put
 * operations, even if requested. By default, acknowledgments are sent for put
 * operations that request an acknowledgment. This applies to both standard and
 * counting type events. Acknowledgments are never sent for \p get operations.
 * The data sent in the \p reply serves as an implicit acknowledgment. */
#define PTL_ME_ACK_DISABLE		(1<<6)

/*! Specifies that the \a min_free field in the match list entry is to be used.
 * This option is only used if \c PTL_ME_MANAGE_LOCAL is set. */
#define PTL_ME_MIN_FREE			(1<<8)

/*! Specifies that this match list entry should not generate events. */
#define PTL_ME_EVENT_DISABLE		(1<<9)

/*! Specifies that this match list entry should not generate events that
 * indicate success. This is useful in scenarios where the application does not
 * need normal events, but does require failure information to enhance
 * reliability. */
#define PTL_ME_EVENT_SUCCESS_DISABLE	(1<<10)

/*! Specifies that this match list entry should not generate overflow list
 * events (\c PTL_EVENT_PUT_OVERFLOW events). */
#define PTL_ME_EVENT_OVER_DISABLE	(1<<11)

/*! Specifies that this match list entry should not generate unlink (\c
 * PTL_EVENT_UNLINK) or free (\c PTL_EVENT_FREE) events. */
#define PTL_ME_EVENT_UNLINK_DISABLE	(1<<12)

/*! Enable the counting of \c PTL_EVENT_GET events. */
#define PTL_ME_EVENT_CT_GET		(1<<13)

/*! Enable the counting of \c PTL_EVENT_PUT events. */
#define PTL_ME_EVENT_CT_PUT		(1<<14)

/*! Enable the counting of \c PTL_EVENT_PUT_OVERFLOW events. */
#define PTL_ME_EVENT_CT_PUT_OVERFLOW	(1<<15)

/*! Enable the counting of \c PTL_EVENT_ATOMIC events. */
#define PTL_ME_EVENT_CT_ATOMIC		(1<<16)

/*! Enable the counting of \c PTL_EVENT_ATOMIC_OVERFLOW events. */
#define PTL_ME_EVENT_CT_ATOMIC_OVERFLOW	(1<<17)

/*! Use job ID for authentication instead of user ID. By default, the user ID
 * must match to allow a message to access a match list entry. */
#define PTL_ME_AUTH_USE_JID		(1<<18)
/*!
 * @struct ptl_me_t
 * @brief Defines the visible parts of a match list entry. Values of this type
 *	are used to initialize and update the match list entries.
 * @note The match list entry (ME) has a number of fields in common with the
 *	memory descriptor (MD). The overlapping fields have the same meaning in
 *	the ME as in the MD; however, since initiator and target resources are
 *	decoupled, the MD is not a proper subset of the ME, and the options
 *	field has different meaning based on whether it is used at an initiator
 *	or target, it was deemed undesirable and cumbersome to include a
 *	"target MD" structure that would be included as an entry in the ME.
 * @note Incoming match bits are compared to the match bits stored in the match
 *	list entry using the ignore bits as a mask. An optimized version of
 *	this is shown in the following code fragment:
 * @code
 * ((incoming_bits ^ match_bits) & ~ignore_bits) == 0
 * @endcode
 */
typedef struct {
    void *		start; /*!< Specify the starting address of the memory
				 region associated with the match list entry.
				 The \a start member can be \c NULL provided
				 that the \a length member is zero. Zero-length
				 buffers (NULL ME) are useful to record events.
				 There are no alignment restrictions on buffer
				 alignment, the starting address or the length
				 of the region; although unaligned messages may
				 be slower (i.e., lower bandwidth and/or longer
				 latency) on some implementations. */
    ptl_size_t		length; /*!< Specify the length of the memory region associated with the match list entry. */
    ptl_handle_ct_t	ct_handle; /*!< A handle for counting type events
				     associated with the memory region. If this
				     argument is \c PTL_CT_NONE, operations
				     performed on this match list entry are not
				     counted. */
    ptl_size_t		min_free; /*!< When the unused portion of a match list
				    entry (length - local offset) falls below
				    this value, the match list entry
				    automatically unlinks. This value is only
				    used if the \c PTL_ME_MIN_FREE option is
				    specified and \c PTL_ME_MANAGE_LOCAL is
				    set. */
    ptl_ac_id_t		ac_id; /*!< Specifies either the user ID or job ID (as
				 selected by the options) that may access this
				 match list entry. Either the user ID or job ID
				 may be set to a wildcard (\c PTL_UID_ANY or \c
				 PTL_JID_ANY). If the access control check
				 fails, then the message is dropped without
				 modifying Portals state. This is treated as a
				 permissions failure and the PtlNIStatus()
				 register indexed by \c
				 PTL_SR_PERMISSIONS_VIOLATIONS is incremented.
				 This failure is also indicated to the
				 initiator through the \a ni_fail_type in the
				 \c PTL_EVENT_SEND event, unless the \c
				 PTL_MD_REMOTE_FAILURE_DISABLE option is set.
				 */
    unsigned int	options; /*!< Specifies the behavior of the match list
				   entry. The following options can be
				   selected: enable put opterations (yes or
				   no), enable get operations (yes or no),
				   offset management (local or remote), message
				   truncation (yes or no), acknowledgement (yes
				   or no), use scatter/gater vectors and
				   disable events. Values for this argument can
				   be constructed using a bitwise OR of the
				   following values:
 - \c PTL_ME_OP_PUT
 - \c PTL_ME_OP_GET
 - \c PTL_ME_MANAGE_LOCAL
 - \c PTL_ME_NO_TRUNCATE
 - \c PTL_ME_USE_ONCE
 - \c PTL_ME_MAY_ALIGN
 - \c PTL_ME_ACK_DISABLE
 - \c PTL_IOVEC
 - \c PTL_ME_MIN_FREE
 - \c PTL_ME_EVENT_DISABLE
 - \c PTL_ME_EVENT_SUCCESS_DISABLE
 - \c PTL_ME_EVENT_OVER_DISABLE
 - \c PTL_ME_EVENT_UNLINK_DISABLE
 - \c PTL_ME_EVENT_CT_GET
 - \c PTL_ME_EVENT_CT_PUT
 - \c PTL_ME_EVENT_CT_PUT_OVERFLOW
 - \c PTL_ME_EVENT_CT_ATOMIC
 - \c PTL_ME_EVENT_CT_ATOMIC_OVERFLOW
 - \c PTL_ME_AUTH_USE_JID
 */
    ptl_process_id_t	match_id; /*!< Specifies the match criteria for the
				    process identifier of the requester. The
				    constants \c PTL_PID_ANY and \c PTL_NID_ANY
				    can be used to wildcard either of the
				    physical identifiers in the
				    ptl_process_id_t structure, or \c
				    PTL_RANK_ANY can be used to wildcard the
				    rank for logical addressing. */
    ptl_match_bits_t	match_bits; /*!< Specify the match criteria to apply to
				      the match bits in the incoming request.
				      The \a ignore_bits are used to mask out
				      insignificant bits in the incoming match
				      bits. The resulting bits are then
				      compared to the match list entry's match
				      bits to determine if the incoming request
				      meets the match criteria. */
    ptl_match_bits_t	ignore_bits; /*!< The \a ignore_bits are used to mask out
				      insignificant bits in the incoming match
				      bits. */
} ptl_me_t;
/*!
 * @fn PtlMEAppend(ptl_handle_ni_t	ni_handle,
 *		   ptl_pt_index_t	pt_index,
 *		   ptl_me_t		me,
 *		   ptl_list_t		ptl_list,
 *		   void *		user_ptr,
 *		   ptl_handle_me_t *	me_handle)
 * @brief Creates a single match list entry. If \c PTL_PRIORITY_LIST or \c
 *	PTL_OVERFLOW is specified by \a ptl_list, this entry is appended to the
 *	end of the appropriate list specified by \a ptl_list associated with
 *	the portal table entry specified by \a pt_index for the portal table
 *	for \a ni_handle. If the list is currently uninitialized, the
 *	PtlMEAppend() function creates the first entry in the list.
 *
 *	When a match list entry is posted to the priority list, the overflow
 *	list is searched to see if a matching message has arrived prior to
 *	posting the match list entry. If so, a \c PTL_EVENT_PUT_OVERFLOW event
 *	is generated. No searching is performed when a match list entry is
 *	posted to the overflow list.
 *
 *	If \a ptl_list is set to \c PTL_PROBE_ONLY, the overflow list is probed
 *	to support the MPI_Probe functionality. A probe of the overflow list
 *	will \e always generate a \c PTL_EVENT_PROBE event. If a matching
 *	message was found in the overflow list, \c PTL_NI_OK is returned in the
 *	event. Otherwise, the even tindicates that the probe operation failed.
 * @param[in] ni_handle	    The interface handle to use.
 * @param[in] pt_index	    The portal table index where the match list entry
 *			    should be appended.
 * @param[in] me	    Provides initial values for the user-visible parts
 *			    of a match list entry. Other than its use for
 *			    initialization, there is no linkage between this
 *			    structure and the match list entry maintained by
 *			    the API.
 * @param[in] ptl_list	    Determines whether the match list entry is appended
 *			    to the priority list, appended ot the overflow
 *			    list, or simply queries the overflow list.
 * @param[in] user_ptr	    A user-specified value that is associated with each
 *			    command that can generate an event. The value does
 *			    not need to be a pointer, but must fit in the space
 *			    used by a pointer. This value (along with other
 *			    values) is recorded in events associated with
 *			    operations on this match list entry.
 * @param[out] me_handle    On successful return, this location will hold the
 *			    newly created match list entry handle.
 * @retval PTL_OK		Indicates success
 * @retval PTL_NO_INIT		Indicates that the portals API has not been
 *				successfully initialized.
 * @retval PTL_ARG_INVALID	Indicates that either \a ni_handle is not a
 *				valid network interface handle, \a pt_index is
 *				not a valid portal table index, or \a match_id
 *				in the match list entry is not a valid process
 *				identifier.
 * @retval PTL_NO_SPACE		Indicates that there is insufficient memory to
 *				allocate the match list entry.
 * @retval PTL_LIST_TOO_LONG	Indicates that the resulting list is too long.
 *				The maximum length for a list is defined by the
 *				interface.
 * @implnote Checking whether a \a match_id is a valid process identifier may
 *	require global knowledge. However, PtlMEAppend() is not meant to cause
 *	any communication with other nodes in the system. Therefore, \c
 *	PTL_PROCESS_INVALID may not be returned in some cases where it would
 *	seem appropriate.
 */
int PtlMEAppend(ptl_handle_ni_t	    ni_handle,
		ptl_pt_index_t	    pt_index,
		ptl_me_t	    me,
		ptl_list_t	    ptl_list,
		void *		    user_ptr,
		ptl_handle_me_t *   me_handle);
/*!
 * @fn PtlMEUnlink(ptl_handle_me_t me_handle)
 * @brief Used to unlink a match list entry from a list. This operation also
 *	releases any resources associated with the match list entry. It is an
 *	error to use the match list entry handle after calling PtlMEUnlink().
 * @param[in] me_handle	The match list entry handle to be unlinked.
 * @note If this match list entry has pending operations; e.g., an unfinished
 *	\p reply operation, then PtlMEUnlink() will return \c PTL_IN_USE,
 *	and the match list entry will not be unlinked. This essentially creates
 *	a race between the application retrying the unlink operation and a new
 *	operation arriving. This is believed to be reasonable as the
 *	application rarely wants to unlink an ME while new operation are
 *	arriving to it.
 * @retval PTL_OK		Indicates success
 * @retval PTL_NO_INIT		Indicates that the portals API has not been
 *				successfully initialized.
 * @retval PTL_ARG_INVALID	Indicates that either \a me_handle is not a
 *				valid match list entry handle.
 * @retval PTL_IN_USE		Indicates that the match list entry has pending
 *				operations and cannot be unlinked.
 */
int PtlMEUnlink(ptl_handle_me_t me_handle);
/*! @} */

/***************************
 * Events and Event Queues *
 ***************************/
/*!
 * @addtogroup EEQ Events and Event Queues
 * @{
 */
/*! @} */

/*********************************
 * Lightweight "Counting" Events *
 *********************************/
/*!
 * @addtogroup LCE Lightweight "Counting" Events
 * @{
 */
/*! @} */

/****************************
 * Data Movement Operations *
 ****************************/
/*!
 * @addtogroup DMO Data Movement Operations
 * @{
 */
/*! @} */

/************************
 * Triggered Operations *
 ************************/
/*!
 * @addtogroup TO Triggered Operations
 * @{
 */
/*! @} */

/*************************
 * Operations on Handles *
 *************************/
/*!
 * @addtogroup OH Operations on Handles
 * @{
 */
/*! @} */


#endif
