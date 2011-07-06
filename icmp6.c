/*
*   This software is Copyright 2011 by Sean Groarke <sgroarke@gmail.com>
*   All rights reserved.
*
*   This file is part of npd6.
*
*   npd6 is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   npd6 is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with npd6.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "npd6.h"

/*****************************************************************************
 * open_packet_socket
 *      Opens the packet-level socket, for incoming traffic,
 *      and sets up the BSD PF.
 *
 * Inputs:
 *  const struct in6_addr * addr
 *      Binary ipv6 address
 *
 * Outputs:
 *  char * str
 *      String representation, *not* fully padded.
 *
 * Return:
 *      void
 *
 * Notes:
 *  Compare with print_addr16 - this version does not pad.
 */
int open_packet_socket(void)
{
    int sock;
    int err;

    static const struct sock_filter filter[] =
    {
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

