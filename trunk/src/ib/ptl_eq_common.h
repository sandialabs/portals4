/**
 * @file ptl_eq.h
 *
 * Event queue interface declarations.
 * @see ptl_eq.c
 */

#ifndef PTL_EQ_COMMON_H
#define PTL_EQ_COMMON_H

#include "ptl_locks.h"

/**
 * Event queue entry.
 */
typedef struct {
	unsigned int		generation;	/**< increments each time the producer
									   pointer wraps */
	ptl_event_t		event;		/**< portals event */
} eqe_t;

// todo: reorder fields for better performance. use align.
struct eqe_list {
	unsigned int		producer;	/**< producer index */
	unsigned int		consumer;	/**< consumer index */
	unsigned int		prod_gen;	/**< producer generation */
	unsigned int		cons_gen;	/**< consumer generation */
	int					interrupt;	/**< if set eq is being
									   freed or destroyed */
	unsigned int		used;		/**< number of slots used */
	unsigned int		count;		/**< size of event queue */

	PTL_FASTLOCK_TYPE	lock;	/**< mutex for eq condition */

	eqe_t eqe[0];
};

int PtlEQGet_work(struct eqe_list *eqe_list, ptl_event_t *event_p);
int PtlEQWait_work(struct eqe_list *eqe_list, ptl_event_t *event_p);
int PtlEQPoll_work(struct eqe_list *eqe_list_in[], unsigned int size,
				   ptl_time_t timeout, ptl_event_t *event_p,
				   unsigned int *which_p);

#endif /* PTL_EQ_COMMON_H */
