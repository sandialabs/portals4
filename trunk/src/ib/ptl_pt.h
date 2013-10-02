/**
 * @file ptl_pt.h
 *
 * This file contains the interface for the
 * pt (portals table) class which contains
 * information about portals tables entries
 */

#ifndef PTL_PT_H
#define PTL_PT_H

#include "ptl_locks.h"

struct eq;

/**
 * pt state variables.
 *
 */
enum pt_state {
    PT_ENABLED = 0,             /* ie. no disabled flag */
    PT_DISABLED = 1 << 0,
    PT_AUTO_DISABLED = 1 << 1,
};

/**
 * pt class into.
 */
struct pt {
        /** pt options */
    unsigned int options;

        /** Its index in the NI. */
    unsigned int index;

        /** pt entry is is use */
    int in_use;

        /** pt entry is enabled/disabled */
    enum pt_state state;

        /** number of currently active target transactions on pt */
    int num_tgt_active;

        /** event queue */
    struct eq *eq;

        /** size of priority list */
    unsigned int priority_size;

        /** list of priority me/le's */
    struct list_head priority_list;

        /** size of overflow list */
    unsigned int overflow_size;

        /** list of overflow me/le's */
    struct list_head overflow_list;

        /** size of unexpected list */
    atomic_t unexpected_size;

        /** list of unexpected xt's */
    struct list_head unexpected_list;

        /** to attach on the EQ flow control list if this PT does it. **/
    struct list_head flowctrl_list;

        /** spin lock to protect pt lists */
    PTL_FASTLOCK_TYPE lock;
};

typedef struct pt pt_t;

#endif /* PTL_PT_H */
