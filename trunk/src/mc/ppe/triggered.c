#include "config.h"

#include "ppe/ppe.h"
#include "ppe/dispatch.h"
#include "ppe/triggered.h"

int 
triggered_ct_impl( struct ptl_ppe_ni_t *ppe_ni, int type,
                            ptl_triggered_args_t *cmd, ptl_ctop_args_t *op )
{
    ptl_ppe_ct_t      *ppe_ct;

    PPE_DBG("ct_index=%d type=%d\n", cmd->ct_handle.s.code, type );

    ppe_ct =  ppe_ni->ppe_ct + cmd->ct_handle.s.code;

    ptl_triggered_op_t *t_op = (ptl_triggered_op_t*) malloc( sizeof(*t_op) );

    t_op->type      = type;
    t_op->index     = cmd->index;
    t_op->threshold = cmd->threshold;
    t_op->u.ct_op   = *op;

    ptl_double_list_insert_back( &ppe_ct->triggered_op_list, &t_op->list );

    return 0;
}

int 
triggered_data_movement_impl( struct ptl_ppe_ni_t *ppe_ni, int type,
                            ptl_triggered_args_t *cmd,
                            ptl_data_movement_args_t *data_movement_args,
                            ptl_atomic_args_t *atomic_args )
{
    ptl_ppe_ct_t      *ppe_ct;

    PPE_DBG("ct_index=%d type=%d\n", cmd->ct_handle.s.code, type );

    ppe_ct =  ppe_ni->ppe_ct + cmd->ct_handle.s.code;

    ptl_triggered_op_t *t_op = (ptl_triggered_op_t*) malloc( sizeof(*t_op) );

    t_op->type      = type;
    t_op->index     = cmd->index;
    t_op->threshold = cmd->threshold;
    t_op->u.data_movement.args = *data_movement_args;
    t_op->u.data_movement.atomic_args = *atomic_args;

    ptl_double_list_insert_back( &ppe_ct->triggered_op_list, &t_op->list );

    return 0;
}


void 
PtlInternalCTPullTriggers( ptl_ppe_ni_t *ppe_ni, ptl_handle_ct_t ct_handle)
{
    const ptl_internal_handle_converter_t ct_hc = { ct_handle };
    ptl_ppe_ct_t       *ppe_ct;
    ptl_internal_ct_t  *client_ct;

    PPE_DBG("ct_index=%d\n", ct_hc.s.code );

    ppe_ct    = ppe_ni->ppe_ct + ct_hc.s.code;

    // MJL: we need the count for this ct and it's stored in client 
    // memory and shared with the ppe, should we keep our own copy in the ppe_ct?
    client_ct = ppe_ni->client_ct + ct_hc.s.code;

    struct ptl_double_list_item_t *cur = ppe_ct->triggered_op_list.head;
    while ( cur ) {
        ptl_triggered_op_t *op = (ptl_triggered_op_t*) cur;

        // update cur now because we might remove this item from the list
        cur = cur->next;
        PPE_DBG("type=%d index=%d threshold=%lu success=%lu\n",
                op->type, op->index, op->threshold, client_ct->ct_event.success );

        if ( client_ct->ct_event.success >= op->threshold) {
            ptl_shared_triggered_t *shared_triggered;
            shared_triggered = ppe_ni->client_triggered + op->index;

            ptl_double_list_remove_item( &ppe_ct->triggered_op_list, 
                                    (struct ptl_double_list_item_t *)op );

            shared_triggered->in_use = 0;

            switch ( op->type ) {
                case PTLTRIGCTINC:
                case PTLTRIGCTSET:
                    ct_op_impl( ppe_ni, op->type, &op->u.ct_op ); 
                    break;
                case PTLTRIGPUT:
                    data_movement_impl( ppe_ni, PTLPUT, 
                                    &op->u.data_movement.args,
                                    &op->u.data_movement.atomic_args); 
                    break;
                case PTLTRIGGET:
                    data_movement_impl( ppe_ni, PTLGET, 
                                    &op->u.data_movement.args,
                                    &op->u.data_movement.atomic_args); 
                    break;
                case PTLTRIGATOMIC:
                    data_movement_impl( ppe_ni, PTLATOMIC, 
                                    &op->u.data_movement.args,
                                    &op->u.data_movement.atomic_args); 
                    break;
                case PTLTRIGFETCHATOMIC:
                    data_movement_impl( ppe_ni, PTLFETCHATOMIC, 
                                    &op->u.data_movement.args,
                                    &op->u.data_movement.atomic_args); 
                    break;
                case PTLTRIGSWAP:
                    data_movement_impl( ppe_ni, PTLSWAP, 
                                    &op->u.data_movement.args,
                                    &op->u.data_movement.atomic_args); 
                    break;
            }
            free( op );
        }
    } 
}
