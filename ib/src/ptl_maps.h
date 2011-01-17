/*
 * ptl_maps.h
 */

#ifndef PTL_MAPS_H
#define PTL_MAPS_H

int get_maps(void);

int check_range(void *addr, unsigned long length);

#ifdef PTL_CHECK_POINTER

#define CHECK_POINTER(addr, type) check_range(addr, sizeof(type))

#define CHECK_RANGE(addr, type, count) check_range(addr, count*sizeof(type))

#else

#define CHECK_POINTER(addr, type)	((addr) == NULL)

#define CHECK_RANGE(addr, type, count)	((addr) == NULL)

#endif

#endif /* PTL_MAPS_H */
