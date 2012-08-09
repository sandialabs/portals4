#include "ptl_loc.h"

#include <knem_io.h>

/* Initializes KNEM. */
int knem_init(ni_t * ni)
{
	assert(ni->shmem.knem_fd == -1);

	/* Open KNEM device */
	ni->shmem.knem_fd = open(KNEM_DEVICE_FILENAME, O_RDWR);
	if (ni->shmem.knem_fd == -1) {
		WARN();
		return PTL_FAIL;
	}

	return PTL_OK;
}

/* Closes KNEM. */
void knem_fini(ni_t * ni)
{
	if (ni->shmem.knem_fd != -1) {
		close(ni->shmem.knem_fd);
		ni->shmem.knem_fd = -1;
	}
}

/* Registers a region. returns the cookie on success, 0 on error. */
uint64_t knem_register(ni_t * ni, void *data, ptl_size_t len, int prot)
{
	struct knem_cmd_create_region create;
	struct knem_cmd_param_iovec iov;
	int err;

	iov.base = (uintptr_t) data;
	iov.len = len;
	create.iovec_array = (uintptr_t) & iov;
	create.iovec_nr = 1;
	create.flags = 0;
	create.protection = prot;

	err = ioctl(ni->shmem.knem_fd, KNEM_CMD_CREATE_REGION, &create);
	if (err < 0) {
		WARN();
		ptl_warn("PORTALS4-> KNEM create region failed, err = %d, errno = %d\n", err, errno);
		abort();
		return 0;
	}

	assert(create.cookie != 0);
	return create.cookie;
}

/* Unregisters a region. */
void knem_unregister(ni_t * ni, uint64_t cookie)
{
	int err;

	assert(cookie != 0);

	err = ioctl(ni->shmem.knem_fd, KNEM_CMD_DESTROY_REGION, &cookie);

	if (err < 0) {
		WARN();
		ptl_warn("PORTALS4-> KNEM destroy region failed, err = %d\n", err);
	}
}

/* copy data between 2 regions. */
size_t knem_copy(ni_t * ni,
				 uint64_t scookie, uint64_t soffset,
				 uint64_t dcookie, uint64_t doffset,
				 size_t length)
{
	struct knem_cmd_copy_bounded copy;
	int err;

	copy.src_cookie = scookie;
	copy.src_offset = soffset;
	copy.dst_cookie = dcookie;
	copy.dst_offset = doffset;
	copy.length = length;
	copy.flags = 0;				/* synchronous */

	err = ioctl(ni->shmem.knem_fd, KNEM_CMD_COPY_BOUNDED, &copy);
	if (err < 0) {
		fprintf(stderr, "PORTALS4-> KNEM inline copy failed, err = %d\n",
				err);
		abort();
	}
	if (copy.current_status != KNEM_STATUS_SUCCESS) {
		fprintf(stderr, "PORTALS4-> KNEM inline copy status "
				"(%u) != KNEM_STATUS_SUCCESS\n", copy.current_status);
		abort();
	}

	return length;
}
