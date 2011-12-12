#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"

#include "ppe/ct.h"

int
ct_alloc_impl( ptl_ppe_ni_t *ppe_ni, ptl_cqe_ctalloc_t *cmd )
{
    PPE_DBG("ct_index=%d\n", cmd->ct_handle );

    ppe_ni->client_ct[ cmd->ct_handle ].in_use = True;

    return 0;
}

int
ct_free_impl( ptl_ppe_ni_t *ppe_ni, ptl_cqe_ctfree_t *cmd )
{
    ptl_internal_ct_t  *client_ct;
    ptl_ppe_ct_t       *ppe_ct;

    PPE_DBG("ct_index=%d\n", cmd->ct_handle );

    client_ct = ppe_ni->client_ct + cmd->ct_handle;
    ppe_ct    = ppe_ni->ppe_ct + cmd->ct_handle;

    cancel_triggered( ppe_ni, ppe_ct ); 

    client_ct->in_use = 0;
    ppe_ct->in_use    = False;
    return 0;
}

int
ct_op_impl( ptl_ppe_ni_t *ppe_ni, int type, ptl_ctop_args_t *cmd )
{
    PPE_DBG("ct_index=%d type=%d\n", cmd->ct_handle, type );

    if ( type == PTLCTSET ) {
        ct_set( ppe_ni, cmd->ct_handle, cmd->ct_event );
    } else {
        ct_inc( ppe_ni, cmd->ct_handle, cmd->ct_event.success );
    }

    return 0;
}
