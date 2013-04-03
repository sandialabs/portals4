int gbl_init(gbl_t *gbl);
extern int ptl_log_level;
extern unsigned long pagesize;
extern unsigned int linesize;

#ifdef IS_PPE
int ppe_misc_init_once(void);
#else
int misc_init_once(void);
#endif
