
#include <assert.h>

#include "ppe/nal.h"

#include "nal/p3.3/include/p3/process.h"

#include "nal/p3.3/include/p3api/types.h"
#include "nal/p3.3/include/p3lib/types.h"
#include "nal/p3.3/include/p3lib/p3lib.h"
#include "nal/p3.3/include/p3lib/p3lib_support.h"

int lib_parse(ptl_hdr_t *hdr, unsigned long nal_msg_data,
          ptl_interface_t type, ptl_size_t *drop_len)
{
    PPE_DBG("nal_msg_data %lu\n",nal_msg_data);

    ptl_process_id_t dst;

    dst.nid = hdr->target_id.phys.nid;
    dst.pid = hdr->target_id.phys.pid;

    lib_ni_t *ni = p3lib_get_ni_pid(type, dst.pid); 
    assert( ni );

    dm_ctx_t *dm_ctx = malloc( sizeof( *dm_ctx ) );
    assert( dm_ctx );
    dm_ctx->nal_msg_data = nal_msg_data;

    dm_ctx->hdr = *hdr;
    dm_ctx->user_ptr = 0; // get from me or le

    // for testing lets send out of some PPE memory
    dm_ctx->iovec.iov_base = malloc ( hdr->length );
    dm_ctx->iovec.iov_len = hdr->length;

    PPE_DBG("dm_ctx=%p\n",dm_ctx);

    ni->nal->recv( ni, 
                        nal_msg_data,
                        dm_ctx,         // lib_data
                        &dm_ctx->iovec, // dst_iov
                        1,              // iovlen
                        0,              // offset
                        hdr->length,    // mlen
                        hdr->length,    // rlen
                        NULL            // addrkey
                    ); 
    
    return PTL_OK;
}

int lib_finalize(lib_ni_t *ni, void *lib_msg_data, ptl_ni_fail_t fail_type)
{
    PPE_DBG("%p\n",lib_msg_data);
    // do event stuff 
    dm_ctx_t *dm_ctx = lib_msg_data;
    free( dm_ctx->iovec.iov_base );
    free( lib_msg_data );
    return PTL_OK;
}
