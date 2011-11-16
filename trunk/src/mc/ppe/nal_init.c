
#include <limits.h>

#include "portals4.h"

#include "nal/p3.3/user/p3utils.h"

#include "nal/p3.3/include/p3api/types.h"
#include "nal/p3.3/include/p3api/debug.h"

#include "nal/p3.3/include/p3/lock.h"
#include "nal/p3.3/include/p3/handle.h"
#include "nal/p3.3/include/p3/process.h"
#include "nal/p3.3/include/p3/nal_types.h"

#include "nal/p3.3/include/p3nal_utcp.h"

#include "nal/p3.3/include/p3lib/types.h"
#include "nal/p3.3/include/p3lib/nal.h"
#include "nal/p3.3/include/p3lib/p3lib_support.h"

#include "ppe/nal.h"

int nal_init( ptl_ppe_t *ppe_ctx )
{
    PPE_DBG("\n");
    p3utils_init();

    p3_process_t *pp = p3lib_new_process();
    assert( pp );

    pp->init = 0;
    p3lib_nal_setup();

    int status;

    ppe_ctx->ni.pid = 0;
    ppe_ctx->ni.debug = // PTL_DBG_NI_00 |
                        PTL_DBG_NI_01 |
                        PTL_DBG_NI_02 |
                        PTL_DBG_NI_03 |
                        PTL_DBG_NI_04 |
                        PTL_DBG_NI_05 |
                        PTL_DBG_NI_06 |
                        PTL_DBG_NI_07;

    ppe_ctx->ni.nal = lib_new_nal( PTL_IFACE_UTCP, NULL, 0, 
                        &ppe_ctx->ni, &ppe_ctx->ni.nid,
                     &ppe_ctx->ni.limits, &status );
    assert( ppe_ctx->ni.nal );
    ppe_ctx->ni.nal->ni = &ppe_ctx->ni;

    pp->ni[0] = &ppe_ctx->ni;
    PPE_DBG("nid=%#lx pid=%d\n",(unsigned long) ppe_ctx->ni.nid, 
                                                        ppe_ctx->ni.pid);
    return 0;
}
