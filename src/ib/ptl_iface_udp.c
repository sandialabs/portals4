/**
 * @file ptl_iface_udp.c
 *
 * @brief Interface support for UDP transport.
 */
#include "ptl_loc.h"

/**
 * @brief Get an IPv4 address from network device name (e.g. ib0).
 *
 * Returns INADDR_ANY on error or if address is not assigned.
 *
 * @param[in] ifname The network interface name to use
 *
 * @return IPV4 address as an in_addr_t in network byte order
 */
static in_addr_t get_ip_address(const char *ifname)
{
    int fd;
    struct ifreq devinfo;
    struct sockaddr_in *sin = (struct sockaddr_in *)&devinfo.ifr_addr;
    in_addr_t addr;

    fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0)
        return INADDR_ANY;

    strncpy(devinfo.ifr_name, ifname, IFNAMSIZ);

    if (ioctl(fd, SIOCGIFADDR, &devinfo) == 0)
        addr = sin->sin_addr.s_addr;
    else
        addr = INADDR_ANY;

    close(fd);

    return addr;
}

/**
 * @brief Initialize interface.
 *
 * @param[in] iface The iface to init
 *
 * @return status
 */
int init_iface_udp(iface_t *iface)
{
    int err;
    in_addr_t addr;

    /* check to see if interface is already initialized. */
    if (iface->udp.connect_s != -1)
        return PTL_OK;

    /* check to see if interface has a valid IPV4 address */
    addr = get_ip_address(iface->ifname);
    if (addr == INADDR_ANY) {
        ptl_warn
            ("interface %d doesn't exist or doesn't have an IPv4 address\n",
             iface->iface_id);
        err = PTL_FAIL;
        goto err1;
    }

    iface->udp.sin.sin_family = AF_INET;
    iface->udp.sin.sin_addr.s_addr = addr;

    return PTL_OK;

  err1:
    cleanup_iface(iface);
    return err;
}

int PtlNIInit_UDP(gbl_t *gbl, ni_t *ni)
{
    int err;
    int ret;
    int flags;
    struct sockaddr_in addr;
    //uint16_t port;
    int port;
    iface_t *iface = ni->iface;

    //if already initialized
    if (ni->id.phys.pid == (port_to_pid(ni->iface->udp.sin.sin_port))) {
        ptl_warn("attempting to re-initialize the interface \n");
        ni->udp.dest_addr = &iface->udp.sin;
        ni->id.phys.nid = iface->id.phys.nid;
        ni->udp.s = ni->iface->udp.connect_s;
        ni->iface->udp.ni_count++;
#if !IS_PPE
        ni->umn_fd = -1;
#endif
        return PTL_OK;
    }

    ni->udp.s = -1;
    ni->id.phys.nid = addr_to_nid(&iface->udp.sin);

    if (iface->id.phys.nid == PTL_NID_ANY) {
        iface->id.phys.nid = ni->id.phys.nid;
    } else if (iface->id.phys.nid != ni->id.phys.nid) {
        WARN();
        err = PTL_FAIL;
        goto error;
    }

    ptl_info("setting ni->id.phys.nid = %x\n", ni->id.phys.nid);

    ptl_info("id.phys.pid = %i\n", ni->id.phys.pid);

    /* Create a socket to be used for the transport. All connections
     * will use it. */
    ni->udp.s = socket(AF_INET, SOCK_DGRAM, 0);
    if (ni->udp.s == -1) {
        ptl_warn("Failed to create socket\n");
        err = PTL_FAIL;
        goto error;
    }

    /* Set the socket in non blocking mode. */

    //REG: set to non-blocking socket mode
    flags = fcntl(ni->udp.s, F_GETFL);
    ret = fcntl(ni->udp.s, F_SETFL, flags | O_NONBLOCK);
    if (ret == -1) {
        ptl_warn("cannot set asynchronous fd to non blocking\n");
        err = PTL_FAIL;
        goto error;
    }

    /* Bind it and retrieve the port assigned. */
    addr = iface->udp.sin;

    for (port = 49152; port <= 65535; port++) {
        addr.sin_port = htons(port);
        ret = bind(ni->udp.s, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == -1) {
            if (errno == EADDRINUSE)
                continue;

            ptl_warn
                ("unable to bind to local address:port %x:%d (errno=%d)\n",
                 addr.sin_addr.s_addr, port, errno);
            break;
        }
        break;
    }

    if (ret == -1) {
        /* Bind failed or no port available. */
        err = PTL_FAIL;
        goto error;
    }
    //ptl_info("UDP socket udp.s bound to socket: %i IP:%s:%i \n",ni->udp.s,inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));

    ni->udp.src_port = htons(port);
    //REG: This is the struct used for ports
    ni->iface->udp.sin.sin_port = htons(port);
    ni->iface->udp.connect_s = ni->udp.s;

    //set NI pid and nid
    ni->id.phys.pid = port_to_pid(ni->iface->udp.sin.sin_port);
    ni->id.phys.nid = addr_to_nid((struct sockaddr_in *)&ni->iface->udp.sin);
    ptl_info("NI PID set to: %x NID: %i ni: %p\n", ni->id.phys.pid,
             ni->id.phys.nid, ni);


    ptl_info("UDP bound to socket: %i port: %i reqport: %i address: %s \n",
             ni->udp.s, ntohs(ni->iface->udp.sin.sin_port), port,
             inet_ntoa(ni->iface->udp.sin.sin_addr));

    ptl_info("Interface bound to socket %i\n", ni->iface->udp.connect_s);

    ni->udp.dest_addr = &iface->udp.sin;

    iface->id.phys.pid = ntohs(ni->iface->udp.sin.sin_port);
    iface->id.phys.nid = addr_to_nid(&iface->udp.sin);

    if ((ni->options & PTL_NI_PHYSICAL) && (ni->id.phys.pid == PTL_PID_ANY)) {
        /* No well known PID was given. Retrieve the pid given by
         * bind. */
        ni->id.phys.pid = iface->id.phys.pid;
        ptl_info("set iface pid(1) = %x\n", iface->id.phys.pid);
    }
    // TODO: Does this belong here or even in UDP at all?
    //off_t bounce_buf_offset;
    //off_t bounce_head_offset;

    //bounce_head_offset = ni->udp.comm_pad_size;
    //ni->udp.comm_pad_size += ROUND_UP(sizeof(struct udp_bounce_head), pagesize);

    atomic_set(&ni->udp.self_recv, 0);
    ni->udp.self_recv_addr = NULL;
    ni->udp.self_recv_len = 0;

    ni->udp.udp_buf.buf_size = get_param(PTL_BOUNCE_BUF_SIZE);
    ni->udp.udp_buf.num_bufs = get_param(PTL_BOUNCE_NUM_BUFS);

    //bounce_buf_offset = ni->udp.comm_pad_size;
    //ni->udp.comm_pad_size += ni->udp.udp_buf.buf_size * ni->udp.udp_buf.num_bufs;

    ni->iface->udp.ni_count++;

    return PTL_OK;

  error:
    if (ni->udp.s != -1) {
        close(ni->udp.s);
        ni->udp.s = -1;
    }
    return err;
}

void cleanup_udp(ni_t *ni)
{

    ni->iface->udp.ni_count--;
    if (ni->iface->udp.ni_count <= 0) {
        //remove address information
        ni->udp.dest_addr = NULL;
        //close the socket
        close(ni->udp.s);
    }
}
