/*
 * ptl_handle.h - handle related types and methods
 */

#ifndef PTL_HANDLE_H
#define PTL_HANDLE_H

/*
   Handles

	handles are encoded as:

	+-------+-------+-------------------- ... ------+
	|  ni	| type	|		obj_index	|
	+-------+-------+-------------------- ... ------+

	obj_index is unique per ni for all objects
	type is an enum obj_type
	ni is the index of the network interface (1, 2, ...)
	(ni == 0 is reserved for non NI handles)
	the obj_index for an ni is always 1
 */

#endif /* PTL_HANDLE_H */
