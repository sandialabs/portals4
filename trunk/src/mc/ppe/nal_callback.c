

#include "ppe.h"

#include "nal/p3.3/include/p3api/types.h"
#include "nal/p3.3/include/p3lib/types.h"
#include "nal/p3.3/include/p3lib/p3lib.h"

int lib_parse(ptl_hdr_t *hdr, unsigned long nal_msg_data,
          ptl_interface_t type, ptl_size_t *drop_len)
{
    PPE_DBG("\n");
    return 0;
}

int lib_finalize(lib_ni_t *ni, void *lib_msg_data, ptl_ni_fail_t fail_type)
{
    PPE_DBG("\n");
    return 0;
}
