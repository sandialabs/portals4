/* -*- C -*-
 *
 * Copyright 2011 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S.  Government
 * retains certain rights in this software.
 * 
 * This file is part of the Portals SHMEM software package. For license
 * information, see the LICENSE file in the top level directory of the
 * distribution.
 *
 *
 * Run-time support for the built-in runtime that is part of the
 * shared memory implementation Portals
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <portals4.h>

#include "support.h"

static ptl_process_t *mapping;
static ptl_handle_ni_t phys_ni_h;

int
libtest_init(void)
{
    int ret;
    ptl_process_t my_id;

    ret = PtlInit();
    if (PTL_OK != ret) { return ret; }

    ret = PtlNIInit(PTL_IFACE_DEFAULT,
                    PTL_NI_NO_MATCHING | PTL_NI_PHYSICAL,
                    PTL_PID_ANY,
                    NULL,
                    NULL,
                    &phys_ni_h);
    if (PTL_OK != ret) { return ret;}

    ret = PtlGetId(phys_ni_h, &my_id);
    if (PTL_OK != ret) { return ret;}

    mapping = malloc(sizeof(ptl_process_t) * 1);
    if (NULL == mapping) return 1;

    mapping[0] = my_id;

    return 0;
}


int
libtest_fini(void)
{
    if (NULL != mapping) free(mapping);

    PtlNIFini(phys_ni_h);
    PtlFini();

    return 0;
}


ptl_process_t*
libtest_get_mapping(void)
{
    return mapping;
}


int
libtest_get_rank(void)
{
    return 0;
}


int
libtest_get_size(void)
{
    return 1;
}


void
libtest_barrier(void)
{
}
