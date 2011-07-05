#include "includes.h"
#include "npd6.h"

int open_packet_socket(void)
{
    int sock;
    int err;

    static const struct sock_filter filter[] = {
        BPF_STMT(BPF_LD|BPF_B|BPF_ABS,
            ETH_HLEN +
            sizeof(struct ip6_hdr) +
            offsetof(struct icmp6_hdr, icmp6_type)),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, ND_NEIGHBOR_SOLICIT, 1, 0),
        BPF_STMT(BPF_RET|BPF_K, 0),
        BPF_STMT(BPF_RET|BPF_K, 0xffffffff),
    };

    struct sock_fprog fprog;
    fprog.filter = (struct sock_filter *)filter;
    fprog.len = sizeof filter / sizeof filter[0];
   
    sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IPV6) );
    if (sock < 0)
    {
        flog(LOG_ERR, "can't create socket(AF_INET6): %s", strerror(errno));
        return (-1);
    }

    err = setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof(fprog));
    if (err < 0)
    {
        flog(LOG_ERR, "setsockopt(SO_ATTACH_FILTER): %s", strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG, "setsockopt(SO_ATTACH_FILTER) OK");

    return sock;
}


int open_icmpv6_socket(void)
{
    int sock;
    struct icmp6_filter i6filter;
    int err, hoplimit;

    sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

    // Hop-limit default
    hoplimit=255;
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hoplimit,
             sizeof(hoplimit));
    if (err < 0)
    {
        flog(LOG_ERR, "setsockopt(IPV6_UNICAST_HOPS): %s", strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG, "setsockopt(IPV6_UNICAST_HOPS) OK");


    
//     // TODO Do we need the ICMP filter for only outbound?
//     ICMP6_FILTER_SETPASSALL( &i6filter );
//     err = setsockopt(sock, IPPROTO_ICMPV6, ICMP6_FILTER, &i6filter,
//              sizeof(i6filter));
//     if (err < 0)
//     {
//         flog(LOG_ERR, "setsockopt(ICMPV6_FILTER): %s", strerror(errno));
//         return (-1);
//     }
//     flog(LOG_DEBUG, "setsockopt(ICMPV6_FILTER) OK");

    
    return sock;
}

/*
 * get_rx()
 */
int get_rx(unsigned char *msg, struct sockaddr_in6 *addr, struct in6_pktinfo **pkt_info) 
{
    struct msghdr mhdr;
    struct iovec iov;

    int len;
    fd_set rfds;

    FD_ZERO( &rfds );
    FD_SET( sockpkt, &rfds );

    if( select( sockpkt+1, &rfds, NULL, NULL, NULL ) < 0 )
    {
        if (errno != EINTR)
            flog(LOG_ERR, "get_rx() select: %s", strerror(errno));

        return -1;
    }

    iov.iov_len = 2048;
    iov.iov_base = (caddr_t) msg;

    memset(&mhdr, 0, sizeof(mhdr));
    mhdr.msg_name = (caddr_t)addr;
    mhdr.msg_namelen = sizeof(*addr);
    mhdr.msg_iov = &iov;
    mhdr.msg_iovlen = 1;
    mhdr.msg_control = NULL;
    mhdr.msg_controllen = 0;

    len = recvmsg(sockpkt, &mhdr, 0);

    if (len < 0)
    {
        if (errno != EINTR)
            flog(LOG_ERR, "get_rx recvmsg: %s", strerror(errno));
        return len;
    }


    return len;
}

