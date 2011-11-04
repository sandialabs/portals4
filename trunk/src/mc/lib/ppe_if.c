#include "ppe_if.h"

struct ppe_if ppe_if_data; 

void ppe_if_init()
{
    ppe_if_data.sharedBase =  malloc( sizeof( ptl_md_t ) * ppe_if_data.limits.max_mds );
    
    ppe_if_data.mdBase = ppe_if_data.sharedBase;  
    ppe_if_data.mdFreeHint = 0;
}
