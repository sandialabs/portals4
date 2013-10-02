#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef LIBEV_INC_PREFIXED
# include <libev/ev.h>
#else
# include <ev.h>
#endif

struct evl {
    struct ev_loop *loop;
    pthread_mutex_t lock;       /* global loop lock */
    ev_async async_w;
};

extern struct evl evl;

void evl_init(struct evl *evl);
void evl_run(struct evl *evl);

/* Add the proper locking around an ev_TYPE_start/stop, and signal the
 * change to the event loop. */
#define EVL_WATCH(func) do { \
		pthread_mutex_lock(&evl.lock); \
		func; \
		ev_async_send(evl.loop, &evl.async_w); \
		pthread_mutex_unlock(&evl.lock); \
	} while(0)
