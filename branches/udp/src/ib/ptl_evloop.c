#include <stdio.h>
#include <pthread.h>

#include "ptl_evloop.h"

static void evl_release(EV_P)
{
	struct evl *evl = ev_userdata (EV_A);
	pthread_mutex_unlock(&evl->lock);
}

static void evl_acquire(EV_P)
{
	struct evl *evl = ev_userdata (EV_A);
	pthread_mutex_lock(&evl->lock);
}

/* This keep the loop active when there is no other event, and is used
 * to signal the loop that events have been added/removed. */
static void async_cb(EV_P_ ev_async *w, int revents)
{
	/* Nothing to do */
}

void evl_init(struct evl *evl)
{
	evl->loop = EV_DEFAULT;

	ev_async_init(&evl->async_w, async_cb);
	ev_async_start(evl->loop, &evl->async_w);

	pthread_mutex_init(&evl->lock, NULL);

	/* now associate this with the loop */
	ev_set_userdata(evl->loop, evl);
	ev_set_loop_release_cb(evl->loop, evl_release, evl_acquire);
}

void evl_run(struct evl *evl)
{
	evl_acquire(evl->loop);
	ev_run (evl->loop, 0);
	evl_release(evl->loop);
}
