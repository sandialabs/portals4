
#include "ppe/ppe.h"
#include "nal/p3.3/include/p3lib/p3lib_support.h"
#include "nal/p3.3/user/p3utils.h"

#include "nal/p3.3/include/p3/nal_types.h"
#include "nal/p3.3/include/p3nal_utcp.h"

lib_ni_t  p3_ni;

void nal_init(void);
void nal_send(void);

void nal_init(void)
{
    PPE_DBG("\n");
    p3utils_init();
    p3lib_nal_setup();

    int status;

    p3_ni.pid = 0;
    p3_ni.debug = -1;

    p3_ni.nal = lib_new_nal( PTL_IFACE_UTCP, NULL, 0, &p3_ni, &p3_ni.nid,
                     &p3_ni.limits, &status );
    assert( p3_ni.nal );
    p3_ni.nal->ni = &p3_ni;

    PPE_DBG("nid=%#lx pid=%d\n",(unsigned long) p3_ni.nid, p3_ni.pid);

}


void nal_send( void )
{
    ptl_process_id_t dst;
    dst.pid = 0;
    dst.nid = p3_ni.nid;
    unsigned long ptr;
    int retval; 
    retval = p3_ni.nal->send( &p3_ni, &ptr, NULL, dst, 
                                    NULL, 0, NULL, 0, 0, 0, NULL );
    printf("%d\n",retval);
}
