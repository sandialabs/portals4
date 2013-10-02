struct ct_info {
    /* When PPE has been selected, the following fields will be shared
     * with the light library. The other fields are only used by the
     * PPE. */
    ptl_ct_event_t event;                       /**< counting event data */

    int interrupt;                              /**< flag indicating ct is
						     getting shut down */
};

int PtlCTPoll_work(struct ct_info *cts_info[], const ptl_size_t *thresholds,
                   unsigned int size, ptl_time_t timeout,
                   ptl_ct_event_t *event_p, unsigned int *which_p);
int PtlCTWait_work(struct ct_info *ct_info, uint64_t threshold,
                   ptl_ct_event_t *event_p);
