/*
 * api.c - API wrappers
 */

#include "ptl_test.h"

int test_ptl_init(struct node_info *info)
{
	return info->ret != PtlInit();
}

int test_ptl_fini(struct node_info *info)
{
	PtlFini();
	return info->ret != ptl_test_return;
}

int test_ptl_ni_init(struct node_info *info)
{
	int ret;

	ret = PtlNIInit(info->iface, info->ni_opt, info->pid,
				      info->desired_ptr, info->actual_ptr,
				      info->map_size, info->desired_map_ptr,
				      info->actual_map_ptr, info->ptr);

	if (ret == PTL_OK) {
		if (info->next_ni >= STACK_SIZE) {
			printf("NI stack overflow\n");
			return PTL_FAIL;
		}
		info->ni_stack[info->next_ni++] = info->ni_handle;
	}

	return info->ret != ret;
}

int test_ptl_ni_fini(struct node_info *info)
{
	int ret;

	ret = PtlNIFini(info->ni_handle);

	if (ret == PTL_OK) {
		if (info->next_ni == 0) {
			printf("NI stack underflow\n");
			return PTL_FAIL;
		}
		info->next_ni--;
	}

	return info->ret != ret;
}

int test_ptl_ni_status(struct node_info *info)
{
	ptl_sr_index_t status_register;
	ptl_sr_value_t status;

	status_register = 0;
	status = 0;

	return info->ret != PtlNIStatus(info->ni_handle,
					status_register, &status);
}

int test_ptl_ni_handle(struct node_info *info)
{
	ptl_handle_any_t handle;
	ptl_handle_ni_t ni_handle;

	handle = 0;
	ni_handle = 0;

	return info->ret != PtlNIHandle(handle, &ni_handle);
}

int test_ptl_handle_is_eq(struct node_info *info)
{
	return info->ret != PtlHandleIsEqual(info->handle1, info->handle2);
}

int test_ptl_get_uid(struct node_info *info)
{
	return info->ret != PtlGetUid(info->ni_handle, info->ptr);
}

int test_ptl_get_id(struct node_info *info)
{
	return info->ret != PtlGetId(info->ni_handle, info->ptr);
}

int test_ptl_get_jid(struct node_info *info)
{
	return info->ret != PtlGetJid(info->ni_handle, info->ptr);
}

int test_ptl_pt_alloc(struct node_info *info)
{
	int ret;

	ret = PtlPTAlloc(info->ni_handle, info->pt_opt,
		         info->eq_handle, info->pt_index, info->ptr);

	if (ret == PTL_OK) {
		if (info->next_pt >= STACK_SIZE) {
			printf("PT stack overflow\n");
			return PTL_FAIL;
		}
		info->pt_stack[info->next_pt++] = info->pt_index;
	}

	return info->ret != ret;
}

int test_ptl_pt_free(struct node_info *info)
{
	int ret;

	ret = PtlPTFree(info->ni_handle, info->pt_index);

	if (ret == PTL_OK) {
		if (info->next_pt == 0) {
			printf("PT stack underflow\n");
			return PTL_FAIL;
		}
		info->next_pt--;
	}

	return info->ret != ret;
}

int test_ptl_pt_disable(struct node_info *info)
{
	return info->ret != PtlPTDisable(info->ni_handle, info->pt_index);
}

int test_ptl_pt_enable(struct node_info *info)
{
	return info->ret != PtlPTEnable(info->ni_handle, info->pt_index);
}

int test_ptl_eq_alloc(struct node_info *info)
{
	int ret;

	ret = PtlEQAlloc(info->ni_handle, info->eq_count, info->ptr);

	if (ret == PTL_OK) {
		if (info->next_eq >= STACK_SIZE) {
			printf("EQ stack overflow\n");
			return PTL_FAIL;
		}
		info->eq_stack[info->next_eq++] = info->eq_handle;
	}

	return info->ret != ret;
}

int test_ptl_eq_free(struct node_info *info)
{
	int ret;

	ret = PtlEQFree(info->eq_handle);

	if (ret == PTL_OK) {
		if (info->next_eq == 0) {
			printf("EQ stack underflow\n");
			return PTL_FAIL;
		}
		info->next_eq--;
	}

	return info->ret != ret;
}

int test_ptl_eq_get(struct node_info *info)
{
	return info->ret != PtlEQGet(info->eq_handle, info->ptr);
}

int test_ptl_eq_wait(struct node_info *info)
{
	return info->ret != PtlEQWait(info->eq_handle, info->ptr);
}

int test_ptl_eq_poll(struct node_info *info)
{
	int ret;

	ret = PtlEQPoll(&info->eq_handle, info->eq_size,
				      info->timeout, info->ptr, info->which_ptr);

	return info->ret != ret;
}

int test_ptl_ct_alloc(struct node_info *info)
{
	int ret;

	ret = PtlCTAlloc(info->ni_handle, info->ptr);

	if (ret == PTL_OK) {
		if (info->next_ct >= STACK_SIZE) {
			printf("CT stack overflow\n");
			return PTL_FAIL;
		}
		info->ct_stack[info->next_ct++] = info->ct_handle;
	}

	return info->ret != ret;
}

int test_ptl_ct_free(struct node_info *info)
{
	int ret;

	ret = PtlCTFree(info->ct_handle);

	if (ret == PTL_OK) {
		if (info->next_ct == 0) {
			printf("CT stack underflow\n");
			return PTL_FAIL;
		}
		info->next_ct--;
	}

	return info->ret != ret;
}

int test_ptl_ct_get(struct node_info *info)
{
	return info->ret != PtlCTGet(info->ct_handle, info->ptr);
}

int test_ptl_ct_wait(struct node_info *info)
{
	return info->ret != PtlCTWait(info->ct_handle, info->ct_test, info->ptr);
}

//int test_ptl_ct_poll(struct node_info *info)
//{
	//return info->ret != PtlEQPoll(&info->ct_handle, info->ct_size,
				      //info->timeout, info->ptr, info->which_ptr);
//}

int test_ptl_ct_set(struct node_info *info)
{
	return info->ret != PtlCTSet(info->ct_handle, info->ct_event);
}

int test_ptl_ct_inc(struct node_info *info)
{
	return info->ret != PtlCTInc(info->ct_handle, info->ct_event);
}

int test_ptl_md_bind(struct node_info *info)
{
	int ret;

	info->md.ct_handle = info->ct_handle;
	info->md.eq_handle = info->eq_handle;

	if (debug) {
		printf("test_ptl_md_bind - start(%p), length(%d)\n",
			info->md.start, (int)info->md.length);
	}
	ret = PtlMDBind(info->ni_handle, &info->md, info->ptr);

	if (ret == PTL_OK) {
		if (info->next_md >= STACK_SIZE) {
			printf("MD stack overflow\n");
			return PTL_FAIL;
		}
		info->md_stack[info->next_md++] = info->md_handle;
	}

	return info->ret != ret;
}

int test_ptl_md_release(struct node_info *info)
{
	int ret;

	ret = PtlMDRelease(info->md_handle);

	if (ret == PTL_OK) {
		if (info->next_md == 0) {
			printf("MD stack underflow\n");
			return PTL_FAIL;
		}
		info->next_md--;
	}

	return info->ret != ret;
}

int test_ptl_le_append(struct node_info *info)
{
	int ret;

	info->le.ct_handle = info->ct_handle;

	ret = PtlLEAppend(info->ni_handle, info->pt_index,
					&info->le, info->list, info->user_ptr,
					info->ptr);

	if (ret == PTL_OK) {
		if (info->next_le >= STACK_SIZE) {
			printf("LE stack overflow\n");
			return PTL_FAIL;
		}
		info->le_stack[info->next_le++] = info->le_handle;
	}

	return info->ret != ret;
}

int test_ptl_le_unlink(struct node_info *info)
{
	int ret;

	ret = PtlLEUnlink(info->le_handle);

	if (ret == PTL_OK) {
		if (info->next_le == 0) {
			printf("LE stack underflow\n");
			return PTL_FAIL;
		}
		info->next_le--;
	}

	return info->ret != ret;
}

int test_ptl_me_append(struct node_info *info)
{
	int ret;

	info->me.ct_handle = info->ct_handle;

	if (info->me.options & PTL_LE_AUTH_USE_JID)
		info->me.ac_id.jid = info->jid;
	else
		info->me.ac_id.uid = info->uid;

	ret = PtlMEAppend(info->ni_handle, info->pt_index,
					&info->me, info->list, info->user_ptr,
					info->ptr);

	if (ret == PTL_OK) {
		if (info->next_me >= STACK_SIZE) {
			printf("ME stack overflow\n");
			return PTL_FAIL;
		}
		info->me_stack[info->next_me++] = info->me_handle;
	}

	return info->ret != ret;
}

int test_ptl_me_unlink(struct node_info *info)
{
	int ret;

	ret = PtlMEUnlink(info->me_handle);

	if (ret == PTL_OK) {
		if (info->next_me == 0) {
			printf("ME stack underflow\n");
			return PTL_FAIL;
		}
		info->next_me--;
	}

	return info->ret != ret;
}

int test_ptl_put(struct node_info *info)
{
	return info->ret != PtlPut(info->md_handle, info->loc_offset,
				   info->length, info->ack_req, info->target_id,
				   info->pt_index, info->match,
				   info->rem_offset, info->user_ptr,
				   info->hdr_data);

}

int test_ptl_get(struct node_info *info)
{
	return info->ret != PtlGet(info->md_handle, info->loc_offset,
				   info->length, info->target_id, info->pt_index,
				   info->match, info->user_ptr,
				   info->rem_offset);
}

int test_ptl_atomic(struct node_info *info)
{
	return info->ret != PtlAtomic(info->md_handle, info->loc_offset,
				      info->length, info->ack_req, info->target_id,
				      info->pt_index, info->match,
				      info->rem_offset, info->user_ptr,
				      info->hdr_data, info->atom_op, info->type);

}

int test_ptl_fetch_atomic(struct node_info *info)
{
	ptl_size_t		local_get_offset;
	ptl_size_t		local_put_offset;

	local_get_offset = 0;
	local_put_offset = 0;

	return info->ret != PtlFetchAtomic(info->get_md_handle,
					   local_get_offset,
					   info->put_md_handle,
					   local_put_offset,
					   info->length, info->target_id,
					   info->pt_index, info->match,
					   info->rem_offset, info->user_ptr,
					   info->hdr_data, info->atom_op,
					   info->type);
}

int test_ptl_swap(struct node_info *info)
{
	ptl_size_t		local_get_offset;
	ptl_size_t		local_put_offset;

	local_get_offset = 0;
	local_put_offset = 0;

	return info->ret != PtlSwap(info->get_md_handle, local_get_offset,
				    info->put_md_handle, local_put_offset,
				    info->length, info->target_id, info->pt_index,
				    info->match, info->rem_offset,
				    info->user_ptr, info->hdr_data, info->ptr,
				    info->atom_op, info->type); 
}

int test_ptl_trig_put(struct node_info *info)
{
	return info->ret != PtlTriggeredPut(info->md_handle, info->loc_offset,
					    info->length, info->ack_req,
					    info->target_id, info->pt_index,
					    info->match, info->rem_offset,
					    info->user_ptr, info->hdr_data,
					    info->ct_handle, info->threshold);
}

int test_ptl_trig_get(struct node_info *info)
{
	return info->ret != PtlTriggeredGet(info->md_handle, info->loc_offset,
					    info->length, info->target_id,
					    info->pt_index, info->match,
					    info->user_ptr, info->rem_offset,
					    info->ct_handle, info->threshold);
}

int test_ptl_trig_atomic(struct node_info *info)
{
	return info->ret != PtlTriggeredAtomic(info->md_handle, info->loc_offset,
					       info->length, info->ack_req,
					       info->target_id, info->pt_index, info->match,
					       info->rem_offset, info->user_ptr,
					       info->hdr_data, info->atom_op, info->type,
					       info->ct_handle, info->threshold);
}

int test_ptl_trig_fetch_atomic(struct node_info *info)
{
	ptl_size_t		local_get_offset;
	ptl_size_t		local_put_offset;

	local_get_offset = 0;
	local_put_offset = 0;

	return info->ret != PtlTriggeredFetchAtomic(info->get_md_handle, local_get_offset,
					 	    info->put_md_handle, local_put_offset,
						    info->length, info->target_id, info->pt_index,
						    info->match, info->rem_offset,
						    info->user_ptr, info->hdr_data, info->atom_op,
						    info->type, info->ct_handle,
						    info->threshold);
}

int test_ptl_trig_swap(struct node_info *info)
{
	ptl_size_t		local_get_offset;
	ptl_size_t		local_put_offset;

	local_get_offset = 0;
	local_put_offset = 0;

	return info->ret != PtlTriggeredSwap(info->get_md_handle, local_get_offset,
					     info->put_md_handle, local_put_offset,
					     info->length, info->target_id, info->pt_index,
					     info->match, info->rem_offset,
					     info->user_ptr, info->hdr_data, info->ptr,
					     info->atom_op, info->type,
					     info->ct_handle, info->threshold); 
}

int test_ptl_trig_ct_inc(struct node_info *info)
{
	ptl_ct_event_t		increment;
	ptl_handle_ct_t		trig_ct_handle;

	//increment = 0;
	trig_ct_handle = 0;

	return info->ret != PtlTriggeredCTInc(info->ct_handle, increment,
					      trig_ct_handle, info->threshold);
}

int test_ptl_trig_ct_set(struct node_info *info)
{
	ptl_ct_event_t		new_ct;
	ptl_handle_ct_t	   	trig_ct_handle;

	//new_ct = 0;
	trig_ct_handle = 0;

	return info->ret != PtlTriggeredCTSet(info->ct_handle, new_ct,
					      trig_ct_handle, info->threshold);
}
