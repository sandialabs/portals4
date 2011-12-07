#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/ct.h"

int
ct_alloc_impl( ptl_ppe_ni_t *ppe_ni, ptl_cqe_ctalloc_t *cmd )
{
    PPE_DBG("ct_index=%d\n", cmd->ct_handle.s.code );

    return 0;
}

int
ct_free_impl( ptl_ppe_ni_t *ppe_ni, ptl_cqe_ctfree_t *cmd )
{
    ptl_internal_ct_t *ct;
    PPE_DBG("ct_index=%d\n", cmd->ct_handle.s.code );

    ct =  ppe_ni->client_ct + cmd->ct_handle.s.code;

    // MJL: how do we coordiate with pending data xfers that will want to
    // use this CT?
    // what about triggered ops for this ct?

    ct->in_use = 0;
    return 0;
}

int
ct_op_impl( ptl_ppe_ni_t *ppe_ni, int type, ptl_ctop_args_t *cmd )
{
    PPE_DBG("ct_index=%d type=%d\n", cmd->ct_handle.s.code, type );

    if ( type == PTLCTSET ) {
        ct_set( ppe_ni, cmd->ct_handle.s.code, cmd->ct_event );
    } else {
        PtlInternalCTSuccessInc( ppe_ni, cmd->ct_handle.a, cmd->ct_event.success );
    }

    return 0;
}
