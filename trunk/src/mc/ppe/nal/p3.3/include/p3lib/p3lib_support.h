
#ifndef _PTL3_LIB_P3LIB_SUPPORT_H_
#define _PTL3_LIB_P3LIB_SUPPORT_H_

#include <limits.h>
#include <stdio.h>
#include <p3/lock.h>
#include <p3/handle.h>
#include <p3api/types.h>
#include <p3lib/types.h>
#include <p3lib/nal.h>

extern lib_ni_t *p3lib_get_ni_pid(ptl_interface_t type, ptl_pid_t pid);


/* The library will use this to get a new instance of a particular NAL type.
 * If a NAL type needs unique data to create an instance, *data and data_sz
 * can be used to supply it.  *nid  and *limits should be set to values
 * appropriate to the new instance. *status should be PTL_OK on success,
 * or give an appropriate error indication.
 */
extern lib_nal_t *lib_new_nal(ptl_interface_t type, void *data, size_t data_sz,
                  const lib_ni_t *ni, ptl_nid_t *nid,
                  ptl_ni_limits_t *limits, int *status);

/* The library will use this to cause a particular NAL type to stop
 * processing messages, prior to shutting down.  The library expects 
 * that when lib_stop_nal() returns, all references to library objects 
 * will have been released, but the NAL memory validation service is 
 * still operational.
 */
extern void lib_stop_nal(lib_nal_t *nal);


/* The library will use this to remove an existing instance of a particular 
 * NAL type.
 */
extern void lib_free_nal(lib_nal_t *nal);


/* The library will use this to do any setup or teardown for NALs.  It is
 * not an error to call these multiple times from the same context.
 */
extern void p3lib_nal_setup(void);
extern void p3lib_nal_teardown(void);

#endif
