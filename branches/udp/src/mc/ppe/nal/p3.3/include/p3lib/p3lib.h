
#ifndef _PTL3_LIB_P3LIB_H_
#define _PTL3_LIB_P3LIB_H_

#include <p3lib/debug.h>

/* When the NAL detects an incoming message, it should call lib_parse() to
 * decode it and begin library processing.  If the NAL requires saved state
 * to process the remainder of the transaction it should use <nal_msg_data>
 * to give the library something that uniquely identifies this transaction.
 * The NAL callbacks will be handed the <nal_msg_data> value any time they
 * require processing for that particular transaction.  The <nal_msg_data>
 * value must be unique for the life of that transaction.  
 *
 * type is the interface type which received the header, and is used to 
 * look up the destination p3_process_t based on PID, since the library
 * guarantees that PID values are unique per interface type.  If the return
 * value is not PTL_OK the NAL should take *drop_len bytes off the wire and
 * throw them away.
 */
extern
int lib_parse(ptl_nid_t src_nid, ptl_hdr_t *hdr, unsigned long nal_msg_data,
          ptl_interface_t type, ptl_size_t *drop_len);

/* When the NAL callbacks send or receive have finished the transaction
 * requested by the library, they should cal lib_finalize() to allow
 * the transaction to be closed.  The lib_msg_data parameter should be the
 * one given to the NAL by the library in the callback.  If the transaction
 * cannot be completed successfully, the NAL should use fail_type to
 * notify that the transaction should be completed with an error condition.
 */
extern
int lib_finalize(lib_ni_t *ni, void *lib_msg_data, ptl_ni_fail_t fail_type);

#endif
