/*
 * Copyright (c) 2009 Cisco Systems.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _LINUX_UMMUNOTIFY_H
#define _LINUX_UMMUNOTIFY_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_LINUX_IOCTL_H
# include <linux/ioctl.h>
#endif

/*
 * Ummunotify relays MMU notifier events to userspace.  A userspace
 * process uses it by opening /dev/ummunotify, which returns a file
 * descriptor.  Interest in address ranges is registered using ioctl()
 * and MMU notifier events are retrieved using read(), as described in
 * more detail below.
 *
 * Userspace can also mmap() a single read-only page at offset 0 on
 * this file descriptor.  This page contains (at offest 0) a single
 * 64-bit generation counter that the kernel increments each time an
 * MMU notifier event occurs.  Userspace can use this to very quickly
 * check if there are any events to retrieve without needing to do a
 * system call.
 */

/*
 * struct ummunotify_register_ioctl describes an address range from
 * start to end (including start but not including end) to be
 * monitored.  user_cookie is an opaque handle that userspace assigns,
 * and which is used to unregister.  flags and reserved are currently
 * unused and should be set to 0 for forward compatibility.
 */
struct ummunotify_register_ioctl {
	uint64_t	start;
	uint64_t	end;
	uint64_t	user_cookie;
	uint32_t	flags;
	uint32_t	reserved;
};

#define UMMUNOTIFY_MAGIC		'U'

/*
 * Forward compatibility: Userspace passes in a 32-bit feature mask
 * with feature flags set indicating which extensions it wishes to
 * use.  The kernel will return a feature mask with the bits of
 * userspace's mask that the kernel implements; from that point on
 * both userspace and the kernel should behave as described by the
 * kernel's feature mask.
 *
 * If userspace does not perform a UMMUNOTIFY_EXCHANGE_FEATURES ioctl,
 * then the kernel will use a feature mask of 0.
 *
 * No feature flags are currently defined, so the kernel will always
 * return a feature mask of 0 at present.
 */
#define UMMUNOTIFY_EXCHANGE_FEATURES	_IOWR(UMMUNOTIFY_MAGIC, 1, uint32_t)

/*
 * Register interest in an address range; userspace should pass in a
 * struct ummunotify_register_ioctl describing the region.
 */
#define UMMUNOTIFY_REGISTER_REGION	_IOW(UMMUNOTIFY_MAGIC, 2, \
					     struct ummunotify_register_ioctl)
/*
 * Unregister interest in an address range; userspace should pass in
 * the user_cookie value that was used to register the address range.
 * No events for the address range will be reported once it is
 * unregistered.
 */
#define UMMUNOTIFY_UNREGISTER_REGION	_IOW(UMMUNOTIFY_MAGIC, 3, uint64_t)

/*
 * Invalidation events are returned whenever the kernel changes the
 * mapping for a monitored address.  These events are retrieved by
 * read() on the ummunotify file descriptor, which will fill the
 * read() buffer with struct ummunotify_event.
 *
 * If type field is INVAL, then user_cookie_counter holds the
 * user_cookie for the region being reported; if the HINT flag is set
 * then hint_start/hint_end hold the start and end of the mapping that
 * was invalidated.  (If HINT is not set, then multiple events
 * invalidated parts of the registered range and hint_start/hint_end
 * and set to the start/end of the whole registered range)
 *
 * If type is LAST, then the read operation has emptied the list of
 * invalidated regions, and user_cookie_counter holds the value of the
 * kernel's generation counter when the empty list occurred.  The
 * other fields are not filled in for this event.
 */
enum {
	UMMUNOTIFY_EVENT_TYPE_INVAL	= 0,
	UMMUNOTIFY_EVENT_TYPE_LAST	= 1,
};

enum {
	UMMUNOTIFY_EVENT_FLAG_HINT	= 1 << 0,
};

struct ummunotify_event {
	uint32_t	type;
	uint32_t	flags;
	uint64_t	hint_start;
	uint64_t	hint_end;
	uint64_t	user_cookie_counter;
};

#endif /* _LINUX_UMMUNOTIFY_H */
