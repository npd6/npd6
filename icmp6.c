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

/* $Id$
 * $HeadURL$
 */

#include "includes.h"
#include "npd6.h"
#include <netpacket/packet.h>


/*****************************************************************************
 * open_packet_socket
 *      Opens the packet-level socket, for incoming traffic,
 *      and sets up the appropriate BSD PF.
 *
 * Inputs:
 *  none
 *
 * Outputs:
 *  none
 *
 * Return:
 *      int sock on success, otherwise -1
 *
 */
int open_packet_socket(void)
{
    int sock, err;
    struct sock_fprog fprog;
    struct sockaddr_ll lladdr;
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
    
    fprog.filter = (struct sock_filter *)filter;
    fprog.len = sizeof filter / sizeof filter[0];
   
    sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IPV6) );
    if (sock < 0)
    {
        flog(LOG_ERR, "Can't create socket(PF_PACKET): %s", strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG2, "Created PF_PACKET socket OK.");

    // Bind the socket to the interface we're interested in
    memset(&lladdr, 0, sizeof(lladdr));
    lladdr.sll_family = AF_PACKET;
    lladdr.sll_protocol = htons(ETH_P_IPV6);
    lladdr.sll_ifindex = interfaceIdx;
    lladdr.sll_hatype = 0;
    lladdr.sll_pkttype = 0;
    lladdr.sll_halen = 0;
    err=bind(sock, (struct sockaddr *)&lladdr, sizeof(lladdr));
    if (err < 0)
    {
        flog(LOG_ERR, "packet socket bind to interface %d failed: %s", interfaceIdx, strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG2, "packet socket bind to interface %d OK", interfaceIdx);

    // Tie the BSD-PF filter to the socket
    err = setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof(fprog));
    if (err < 0)
    {
        flog(LOG_ERR, "setsockopt(SO_ATTACH_FILTER): %s", strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG2, "setsockopt(SO_ATTACH_FILTER) OK");


    return sock;
}


/*****************************************************************************
 * open_icmpv6_socket
 *      Opens the ipv6-level socket, for outgoing traffic.
 *
 * Inputs:
 *  none
 *
 * Outputs:
 *  none
 *
 * Return:
 *      int sock on success, otherwise -1
 *
 */
int open_icmpv6_socket(void)
{
    int sock, err;

    sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0)
    {   //BUG 012
        flog(LOG_ERR, "Can't create socket(AF_INET6): %s", strerror(errno));
        return (-1);
    }

    // BUG 008
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &maxHops, sizeof(maxHops));
    if (err < 0)
    {
        flog(LOG_ERR, "setsockopt(IPV6_UNICAST_HOPS = %d): %s", maxHops, strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG2, "setsockopt(IPV6_UNICAST_HOPS = %d) OK", maxHops);

    
    return sock;
}



/*****************************************************************************
 * get_rx
 *      Called from the dispatcher to pull in the received packet.
 *
 * Inputs:
 *  sockpkt is where the data is waiting.
 *      
 * Outputs:
 *  unsigned char *msg
 *      The data.
 *
 * Return:
 *      int length of data received, otherwise -1 on error
 *
 * NOTES:
 * There's a lot of temp data structures needed here, but we really don't
 * care about them afterwards. Once we've got the raw data and the len
 * we're good.
 */
int get_rx(unsigned char *msg) 
{
    struct sockaddr_in6 saddr;
    struct msghdr mhdr;
    struct iovec iov;
    int len;
    fd_set rfds;

    FD_ZERO( &rfds );
    FD_SET( sockpkt, &rfds );

    if( select( sockpkt+1, &rfds, NULL, NULL, NULL ) < 0 )
    {
        if (errno != EINTR)
            flog(LOG_ERR, "select failed with: %s", strerror(errno));
        return -1;
    }

    iov.iov_len = MAX_MSG_SIZE;
    iov.iov_base = (caddr_t) msg;

    memset(&mhdr, 0, sizeof(mhdr));
    mhdr.msg_name = (caddr_t)&saddr;
    mhdr.msg_namelen = sizeof(saddr);
    mhdr.msg_iov = &iov;
    mhdr.msg_iovlen = 1;
    mhdr.msg_control = NULL;
    mhdr.msg_controllen = 0;

    len = recvmsg(sockpkt, &mhdr, 0);

    /* Impossible.. But let's not take chances */
    if (len > MAX_MSG_SIZE)
    {
        flog(LOG_ERR, "Read more data from socket than we can handle. Ignoring it.");
        return -1;
    }
    

    if (len < 0)
    {
        if (errno != EINTR)
            flog(LOG_ERR, "recvmsg failed with: %s", strerror(errno));
        return -1;
    }


    return len;
}



/*****************************************************************************
 * if_allmulti
 *      Called during startup and shutdown. Set/clear allmulti
 *      as required.
 *
 * Inputs:
 *  ifname is interface name
 *  state: 1-> Set (or confirm) flag is enabled
 *  state: 0-> Set flag back to initial condition.
 *
 * Outputs:
 *  none
 *
 * Return:
 *  void
 */
void if_allmulti(char *ifname, unsigned int state)
{
    struct ifreq    ifr;
    int skfd;

    skfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    // Get current flags, etc.
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        flog(LOG_ERR, "Unknown interface: %s, failed err = %s", ifname, strerror(errno));
        exit(1);
    }

    if (state)
    {
        flog(LOG_DEBUG, "Setting IFFALLMULTI if required.");
        initialIFFlags = ifr.ifr_flags;
        ifr.ifr_flags |= IFF_ALLMULTI;
        if (ifr.ifr_flags == initialIFFlags)
        {
            // Already set
            flog(LOG_DEBUG, "Not required, was set at startup anyway.");
            goto tidyexit;;
        }
    }
    else
    {
        flog(LOG_DEBUG, "Clearing IFFALLMULTI if required.");
        // Was it originally set?
        if (initialIFFlags & IFF_ALLMULTI)
        {
            // Was originally set - so leave it
            flog(LOG_DEBUG, "Not required, was set at startup anyway.");
            goto tidyexit;;
        }
        // else unset it
        ifr.ifr_flags &= ~IFF_ALLMULTI;
    }
    
    if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0)
    {
        flog(LOG_ERR, "Flag change failed: %s, failed err = %s", ifname, strerror(errno));
        exit(1);
    }

tidyexit:
    close(skfd);
    return;
}


/*****************************************************************************
 * init_sockets
 *  Initialises the tx and rx sockets. Normally just called during startup,
 *  but also to reinitialise sockets if they go bad.
 *
 * Inputs:
 *  void
 *
 * Outputs:
 *  Set globals sockpkt & sockicmp.
 *
 * Return:
 *  Non-0 if failure, else 0.
 */
int init_sockets(void)
{
    int errcount = 0;

    /* Raw socket for receiving NSs */
    sockpkt = open_packet_socket();
    if (sockpkt < 0)
    {
        flog(LOG_ERR, "open_packet_socket: failed.");
        errcount++;
    }
    flog(LOG_DEBUG, "open_packet_socket: OK.");

    /* ICMPv6 socket for sending NAs */
    sockicmp = open_icmpv6_socket();
    if (sockicmp < 0)
    {
        flog(LOG_ERR, "open_icmpv6_socket: failed.");
        errcount++;
    }
    flog(LOG_DEBUG, "open_icmpv6_socket: OK.");

    return errcount;
}
