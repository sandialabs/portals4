/*
 * Portals4 Reference Implementation
 *
 * Copyright (c) 2010 Sandia Corporation
 */

#ifndef PORTALS4_H
#define PORTALS4_H

/*****************
 * Return Values *
 *****************/
#define PTL_OK	    0 /* Indicates Success */
#define PTL_FAIL    1 /* Indicates a non-specific error */

/********************
 * Portal Datatypes *
 ********************/
typedef size_t	ptl_size_t;
typedef pid_t	ptl_pid_t;
typedef int	ptl_interface_t;
typedef pid_t	ptl_process_id_t;
typedef int	ptl_handle_ni_t;

/******************************
 * Initialization and Cleanup *
 ******************************/
/**
 * The PtlInit() function initializes the portals library.
 * PtlInit must be called at least once by a process before any thread makes a
 * portals function call but may be safely called more than once. Each call to
 * PtlInit() increments a reference count.
 * @see PtlFini()
 * @return OK or FAIL
 */
int PtlInit(void);
/**
 * The PtlFini() function allows an application to clean up after the portals library is no longer needed by a process.
 * Each call to PtlFini() decrements the reference count that was incremented
 * by PtlInit(). When the reference count reaches zero, all portals resources
 * are freed. Once the portals resources are freed, calls to any of the
 * functions defined by the portals API or use the structures set up by the
 * portals API will result in undefined behavior. Each call to PtlInit() should
 * be matched by a corresponding PtlFini().
 * @see PtlInit()
 */
void PtlFini(void);

/**********************
 * Network Interfaces *
 **********************/
/*! The Network interface Limits Type */
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

/**
int PtlNIInit(ptl_interface_t	iface,
	      unsigned int	options,
	      ptl_pid_t		pid,
	      ptl_ni_limits_t	*desired,
	      ptl_ni_limits_t	*actual,
	      ptl_size_t	map_size,
	      ptl_process_id_t	*desired_mapping,
	      ptl_process_id_t	*actual_mapping,
	      ptl_handle_ni_t	*ni_handle);

#endif
