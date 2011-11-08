#ifndef PTL_INTERNAL_STARTUP_H
#define PTL_INTERNAL_STARTUP_H

#include "ptl_internal_iface.h"

int ptl_ppe_global_init(ptl_iface_t *ptl_iface);

int ptl_ppe_global_setup(ptl_iface_t *ptl_iface);

int ptl_ppe_global_fini(ptl_iface_t *ptl_iface);

int ptl_ppe_connect(ptl_iface_t *ptl_iface);

int ptl_ppe_disconnect(ptl_iface_t *ptl_iface);

#endif
