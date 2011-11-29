
#ifndef MC_PPE_PT_H
#define MC_PPE_PT_H

#if 0
static inline int
pt_append_le( ptl_ppe_pt_t* pt, ptl_list_t list, ptl_ppe_le_t* ppe_le )
{
    PPE_DBG("ppe_me=%p\n",ppe_le);
    ptl_double_list_insert_back( &pt->list[list], &ppe_le->base );  
    return 0;
}

static inline int
pt_unlink_le( ptl_ppe_pt_t* pt, ptl_list_t list, ptl_ppe_le_t* ppe_le )
{
    PPE_DBG("ppe_me=%p\n",ppe_le);
    ptl_double_list_remove_item( &pt->list[list], &ppe_le->base );  
    return 0;
}
#endif

#endif
