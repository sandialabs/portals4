
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

// need to expose the ni to lib_parse
lib_ni_t *_p3_ni;

int nal_init( ptl_ppe_t *ppe_ctx )
{
    PPE_DBG("\n");
    p3utils_init();

    p3lib_nal_setup();

    int status;

    _p3_ni = &ppe_ctx->ni;
    // pid is not used when one nal services multiple apps 
    ppe_ctx->ni.pid = PTL_PID_ANY;
#if 1 
    ppe_ctx->ni.debug = 0;
#else
// PTL_DBG_NI_00 |
    ppe_ctx->ni.debug = PTL_DBG_NI_01 |
                        PTL_DBG_NI_02 |
                        PTL_DBG_NI_03 |
                        PTL_DBG_NI_04 |
                        PTL_DBG_NI_05 |
                        PTL_DBG_NI_06 |
                        PTL_DBG_NI_07;
#endif

    ppe_ctx->ni.data = ppe_ctx; 
    PPE_DBG("%p\n",&ppe_ctx->ni);
    ppe_ctx->ni.nal = lib_new_nal( PTL_IFACE_UTCP, NULL, 0, 
                        &ppe_ctx->ni, &ppe_ctx->ni.nid,
                     &ppe_ctx->ni.limits, &status );
    assert( ppe_ctx->ni.nal );
    ppe_ctx->ni.nal->ni = &ppe_ctx->ni;
    ppe_ctx->nid = ppe_ctx->ni.nid;

    PPE_DBG( "nid=%#lx\n", (unsigned long) ppe_ctx->ni.nid );
    return 0;
}
