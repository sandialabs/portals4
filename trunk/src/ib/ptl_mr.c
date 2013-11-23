/**
 * @file ptl_buf.c
 *
 * This file contains the implementation of
 * mr (memory region) class methods.
 */

#include "ptl_loc.h"

#include "ummunotify.h"

/**
 * @brief Initialize mr each time when allocated from free list.
 *
 * @param[in] arg opaque mr address
 *
 * @return PTL_OK Indicates success.
 */
int mr_new(void *arg)
{

    mr_t *mr;
    mr = arg;

#if WITH_TRANSPORT_IB
    mr->ibmr = NULL;
#endif

#if WITH_TRANSPORT_SHMEM
    mr->knem_cookie = 0;
#endif

#if IS_PPE
    mr->ppe_addr = (void *)-1;
#endif

    return PTL_OK;
}

/**
 * Cleanup mr object.
 *
 * Called when the mr object is freed to the mr pool.
 *
 * @param[in] arg opaque reference to an mr object
 */
void mr_cleanup(void *arg)
{
    mr_t *mr = (mr_t *)arg;

#if !IS_PPE
    if (mr->obj.obj_ni->umn_fd != -1 && mr->umn_cookie != 0) {
        ioctl(mr->obj.obj_ni->umn_fd, UMMUNOTIFY_UNREGISTER_REGION,
              &mr->umn_cookie);
    }
#endif

#if WITH_TRANSPORT_IB
    if (mr->ibmr) {
        int err;

        err = ibv_dereg_mr(mr->ibmr);
        if (err) {
            ptl_error("ibv_dereg_mr failed, ret = %d\n", err);
        }
        mr->ibmr = NULL;
    }
#endif

#if WITH_TRANSPORT_SHMEM
    if (mr->knem_cookie) {
        knem_unregister(mr->obj.obj_ni, mr->knem_cookie);
        mr->knem_cookie = 0;
    }
#endif

#if IS_PPE
    if (mr->ppe_addr != (void *)-1) {
        xpmem_detach((void *)((uintptr_t) mr->ppe_addr & ~(pagesize - 1)));
        mr->ppe_addr = (void *)-1;
    }
#endif
}

/**
 * Compare two mrs.
 *
 * mrs are sorted by starting address.
 *
 * @param[in] m1 first mr
 * @param[in] m2 second mr
 *
 * @return -1, 0, or +1 as m1 address is <, == or > m2 address
 */
static int mr_compare(struct mr *m1, struct mr *m2)
{
    return (m1->addr < m2->addr ? -1 : m1->addr > m2->addr);
}

#if !IS_PPE
static atomic_t umn_cookie = {.val = 1 };

/* Last kernel generation counter seen. If it is different than the
 * current counter, then some messages are waiting. */
static uint64_t generation_counter;

static void umn_register(ni_t *ni, mr_t *mr, void *start, size_t size)
{
    struct ummunotify_register_ioctl r = {
        .start = (uintptr_t) start,
        .end = (uintptr_t) start + size,
        .user_cookie = atomic_inc(&umn_cookie)
    };

    if (ni->umn_fd == -1)
        return;

    if (ioctl(ni->umn_fd, UMMUNOTIFY_REGISTER_REGION, &r)) {
        perror("register ioctl");
        return;
    }

    mr->umn_cookie = r.user_cookie;
}
#endif

/**
 * Generate RB tree internal functions.
 */
RB_GENERATE_STATIC(the_root, mr, entry, mr_compare);

/**
 * Allocate and register a new memory region.
 *
 * For the new mr both an OFA verbs memory region and
 * a knem cookie are created.
 *
 * @param[in] ni from which to allocate mr
 * @param[in] start starting address of memory region
 * @param[in] length length of memory region
 * @param[out] mr_p address of return value
 *
 * @return status
 */
static int mr_create(ni_t *ni, void *start, ptl_size_t length, mr_t **mr_p)
{
    int err;
    mr_t *mr = NULL;
    void *end = start + length;
    void *ib_start;

    err = mr_alloc(ni, &mr);
    if (err) {
        WARN();
        err = ENOMEM;
        goto err1;
    }
#if WITH_QLOGIC
    //Fix for Qlogic InfiniPath, that does not accept a NULL value for an ibv_mr_reg
    //REG 2013
    if ((start == NULL) && ((int)length == 0)) {
        uint64_t junk_value;
        start = &junk_value;
        end = start;
    }
#endif

    start = (void *)((uintptr_t) start & ~((uintptr_t) pagesize - 1));
    end =
        (void *)(((uintptr_t) end + pagesize - 1) &
                 ~((uintptr_t) pagesize - 1));
    length = end - start;

    ib_start = start;

#if !IS_PPE
    /* Register the region with ummunotify to be notified when the
     * application frees the buffer, or parts of it. */
    umn_register(ni, mr, start, length);
#endif

#if IS_PPE
    if (length == 0) {
        /* Nothing to map. ppe_addr should never be used. Leave it at
         * -1. */
    } else {
        off_t offset;
        struct xpmem_addr addr;

        /* Hack. When addr.offset is not page aligned, xpmem_attach()
         * always fails. So fix the ptr afterwards. */
        offset = ((uintptr_t) start) & (pagesize - 1);
        addr.offset = (uintptr_t) start - offset;
        addr.apid = ni->mem.apid;

        mr->ppe_addr = xpmem_attach(addr, length + offset, NULL);
        if (mr->ppe_addr == (void *)-1) {
            err = EINVAL;
            WARN();
            goto err1;
        }

        mr->ppe_addr += offset;

        /* Adjust parameter for IB. */
        ib_start = mr->ppe_addr;
    }
#endif

#if WITH_TRANSPORT_IB
    /*
     * for now ask for everything
     * TODO get more particular later
     */
    mr->ibmr =
        ibv_reg_mr(ni->iface->pd, ib_start, length,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
                   IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC
#ifdef IS_PPE
                   | IBV_ACCESS_XPMEM
#endif
        );
    if (!mr->ibmr) {
        err = errno;
        WARN();
        /* Try again for a read only MR */
        mr->ibmr =
            ibv_reg_mr(ni->iface->pd, ib_start, length, IBV_ACCESS_REMOTE_READ
#ifdef IS_PPE
                       | IBV_ACCESS_XPMEM
#endif
            );
        if (!mr->ibmr) {
            err = errno;
            WARN();
            goto err1;
        } else {
            mr->readonly = 1;
        }
    } else {
        mr->readonly = 0;
    }

#endif

#if WITH_TRANSPORT_SHMEM
    if (get_param(PTL_ENABLE_MEM)) {
        mr->knem_cookie =
            knem_register(ni, start, length, PROT_READ | PROT_WRITE);
        if (!mr->knem_cookie) {
            err = EINVAL;
            WARN();
            goto err1;
        }
    }
#endif

    mr->addr = start;
    mr->length = length;
    *mr_p = mr;

    return 0;

  err1:
    if (mr)
        mr_put(mr);

    return err;
}

/**
 * Lookup an mr in the mr cache.
 *
 * Returns an mr satisfying the requested start/length. A new mr can
 * be allocated, or an existing one can be used. It is also possible that
 * one or more existing mrs will be merged into one.
 *
 * @param[in] ni in which to lookup range
 * @param[in] start starting address of memory range in application space
 * @param[in] length length of range
 * @param[out] mr_p address of return value
 *
 * @return status
 */
int mr_lookup(ni_t *ni, struct ni_mr_tree *tree, void *start,
              ptl_size_t length, mr_t **mr_p)
{
    /*
     * Search for an existing mr. The start address of the node must
     * be less than or equal to the start address of the requested
     * start. Find the closest start.
     */
    struct mr *link;
    struct mr *rb;
    struct mr *mr;
    struct mr *left_node;
    int ret;
    struct list_head mr_list;

#if !IS_PPE
  again:
#endif
    PTL_FASTLOCK_LOCK(&tree->tree_lock);

    link = RB_ROOT(&tree->tree);
    left_node = NULL;

    mr = NULL;

    while (link) {
        mr = link;

        if (start < mr->addr)
            link = RB_LEFT(mr, entry);
        else {
            if (mr->addr + mr->length >= start + length) {
                /* Requested mr fits in an existing region. */
                mr_get(mr);
                ret = 0;
                *mr_p = mr;
                goto done;
            }
            left_node = mr;
            link = RB_RIGHT(mr, entry);
        }
    }

    /* Not found. */
    INIT_LIST_HEAD(&mr_list);

    mr = NULL;

    /* Extend region to the left. */
    if (left_node && (start <= (left_node->addr + left_node->length))) {
        length += start - left_node->addr;
        start = left_node->addr;

        /* First merge node. Will be replaced later. */
        mr = left_node;
    }

    /* Extend the region to the right. */
    if (left_node)
        rb = RB_NEXT(the_root, &tree->tree, left_node);
    else
        rb = RB_MIN(the_root, &tree->tree);
    while (rb) {
        struct mr *next_rb = RB_NEXT(the_root, &tree->tree, rb);

        /* Check whether new region can be merged with this node. */
        if (start + length >= rb->addr) {
            /* Is it completely part of the new region ? */
            size_t new_length = rb->addr + rb->length - start;
            if (new_length > length)
                length = new_length;

            if (mr) {
                /* Mark the node for removal since it will be included
                 * in the new mr. */
                list_add_tail(&rb->list, &mr_list);
            } else {
                /* First merge node. Will be replaced later. */
                mr = rb;
            }
        } else {
            break;
        }

        rb = next_rb;
    }

    if (mr) {
        /* Mark for removal the included mr on the right. */
        list_add_tail(&mr->list, &mr_list);
        mr = NULL;
    }

    /* Insert the new node */
    ret = mr_create(ni, start, length, mr_p);
    if (ret) {
#if !IS_PPE
        if (ret == EFAULT && ni->umn_fd != -1) {
            /* Some pages cannot be registered. This happens when the
             * application has freed some regions, and we tried to
             * extend the requeted MR. In that case, we wait for all
             * the notification messages to be consummed by
             * process_ummunotify() then try again.
             *
             * This case should rarely happen as it is there only to
             * close that small race. */
            PTL_FASTLOCK_UNLOCK(&tree->tree_lock);
            while (generation_counter != *ni->umn_counter) {
                SPINLOCK_BODY();
            }
            goto again;
        }
#endif

        *mr_p = NULL;
        ret = PTL_FAIL;
    } else {
        void *res;

        /* Remove all the MRs that are included in the new MR. We must
         * create the new MR first before eliminating these. */
        list_for_each_entry(mr, &mr_list, list) {
            RB_REMOVE(the_root, &tree->tree, mr);
            mr_put(mr);
        }

        /* Finally we can insert the new MR in the tree. */
        mr = *mr_p;
        mr_get(mr);
        res = RB_INSERT(the_root, &tree->tree, mr);
//this can happen if using Qlogic
#if !WITH_QLOGIC
        assert(res == NULL);           /* should never happen */
#endif
    }

  done:
    PTL_FASTLOCK_UNLOCK(&tree->tree_lock);

    return ret;
}

#if !IS_PPE
static void process_ummunotify(EV_P_ ev_io *w, int revents)
{
    ni_t *ni = w->data;
    struct ummunotify_event ev;
    int len;

    while (1) {

        /* Read an event. */
        len = read(ni->umn_fd, &ev, sizeof ev);
        if (len < 0 || len != sizeof ev) {
            WARN();
            return;
        }

        switch (ev.type) {
            case UMMUNOTIFY_EVENT_TYPE_INVAL:{
                /* Search the app tree for the MR with that cookie and
                 * remove it. We don't care for the self tree. */
                struct mr *mr;

                PTL_FASTLOCK_LOCK(&ni->mr_app.tree_lock);

                RB_FOREACH(mr, the_root, &ni->mr_app.tree) {
                    if (mr->umn_cookie == ev.user_cookie_counter) {
                        /* All or part of that region is now invalid. We must not reuse it. Remove it from the tree. */
                        RB_REMOVE(the_root, &ni->mr_app.tree, mr);
                        mr_put(mr);

                        break;
                    }
                }

                PTL_FASTLOCK_UNLOCK(&ni->mr_app.tree_lock);
            }
                break;

            case UMMUNOTIFY_EVENT_TYPE_LAST:
                generation_counter = ev.user_cookie_counter;
                return;
        }
    }
}

/**
 * Try to use the ummunotify driver if present
 */
void mr_init(ni_t *ni)
{
    ni->umn_fd = open("/dev/ummunotify", O_RDONLY | O_NONBLOCK);
    if (ni->umn_fd == -1) {
        fprintf(stderr,
                "WARNING: Ummunotify not found: Not using ummunotify can result in incorrect results download and install ummunotify from:\n http://support.systemfabricworks.com/downloads/ummunotify/ummunotify-v2.tar.bz2\n");
        return;
    }

    ni->umn_counter =
        mmap(NULL, sizeof *(ni->umn_counter), PROT_READ, MAP_SHARED,
             ni->umn_fd, 0);
    if (ni->umn_counter == MAP_FAILED) {
        close(ni->umn_fd);
        ni->umn_fd = -1;
        return;
    }

    ni->umn_watcher.data = ni;
    ev_io_init(&ni->umn_watcher, process_ummunotify, ni->umn_fd, EV_READ);
    EVL_WATCH(ev_io_start(evl.loop, &ni->umn_watcher));
}
#endif

/**
 * Empty an mr cache.
 *
 * @param[in] ni for which cache is emptied
 */
static void cleanup_mr_tree(struct ni_mr_tree *tree)
{
    mr_t *mr;
    mr_t *next_mr;

    PTL_FASTLOCK_LOCK(&tree->tree_lock);

    for (mr = RB_MIN(the_root, &tree->tree); mr != NULL; mr = next_mr) {
        next_mr = RB_NEXT(the_root, &tree->tree, mr);
        RB_REMOVE(the_root, &tree->tree, mr);
        mr_put(mr);
    }

    PTL_FASTLOCK_UNLOCK(&tree->tree_lock);
}

/**
 * Empty the two mr caches.
 *
 * @param[in] ni for which caches are emptied
 */
void cleanup_mr_trees(ni_t *ni)
{
#if !IS_PPE
    if (ni->umn_fd != -1) {
        EVL_WATCH(ev_io_stop(evl.loop, &ni->umn_watcher));
    }
#endif

    cleanup_mr_tree(&ni->mr_self);
    cleanup_mr_tree(&ni->mr_app);
}
