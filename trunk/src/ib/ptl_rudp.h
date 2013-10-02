/*
 *
 * ptl_rudp.h - Header file for reliable UDP
 *
*/

ssize_t ptl_sendmsg(int sockfd, const struct msghdr *msg, int flags, ni_t *ni);

ssize_t ptl_sendto(int sockfd, buf_t * buf, size_t len, int flags,
                   struct sockaddr *dest_addr, socklen_t addrlen, 
                   ni_t *ni);

ssize_t ptl_recvfrom(int sockfd, buf_t *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen, ni_t *ni);

ssize_t ptl_recvmsg(int sockfd, struct msghdr *msg, int flags, ni_t *ni);

int process_rudp_recv_hdr(buf_t * buf, int len, ni_t * ni);

int process_rudp_send_hdr(buf_t * buf, int len, ni_t * ni);

