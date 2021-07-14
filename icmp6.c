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
 *  Index of the interface we're opening it for.
 *
 * Outputs:
 *  none
 *
 * Return:
 *      int sock on success, otherwise -1
 *
 */
int open_packet_socket(int ifIndex)
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
    lladdr.sll_family = PF_PACKET;
    lladdr.sll_protocol = htons(ETH_P_IPV6);
    lladdr.sll_ifindex = ifIndex;
    lladdr.sll_hatype = 0;
    lladdr.sll_pkttype = 0;
    lladdr.sll_halen = ETH_ALEN;
    err=bind(sock, (struct sockaddr *)&lladdr, sizeof(lladdr));
    if (err < 0)
    {
        flog(LOG_ERR, "packet socket bind to interface %d failed: %s", ifIndex, strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG2, "packet socket bind to interface %d OK", ifIndex);

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
    int sock, err, optval = 1;

    sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0)
    {
        flog(LOG_ERR, "Can't create socket(AF_INET6): %s", strerror(errno));
        return (-1);
    }

    err = setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &maxHops, sizeof(maxHops));
    if (err < 0)
    {
        flog(LOG_ERR, "setsockopt(IPV6_UNICAST_HOPS = %d): %s", maxHops, strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG2, "setsockopt(IPV6_UNICAST_HOPS = %d) OK", maxHops);

    /* Set packet info return via recvmsg() later */
    /* Needed to get src addr of received ICMP pkts */
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_2292PKTINFO, &optval, sizeof(optval));
    if (err < 0)
    {
        flog(LOG_ERR, "setsockopt(IPV6_2292PKTINFO): %s", strerror(errno));
        return (-1);
    }
    flog(LOG_DEBUG2, "setsockopt(IPV6_2292PKTINFO) OK");
    
    
    return sock;
}



/*****************************************************************************
 * get_rx
 *      Called from the dispatcher to pull in the received packet.
 *
 * Inputs:
 *  socket is where the data is waiting.
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
int get_rx(int socket, unsigned char *msg) 
{
    struct sockaddr_in6 saddr;
    struct msghdr mhdr;
    struct iovec iov;
    int len;
    fd_set rfds;

    FD_ZERO( &rfds );
    FD_SET( socket, &rfds );

    if( select( socket+1, &rfds, NULL, NULL, NULL ) < 0 )
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

    len = recvmsg(socket, &mhdr, 0);

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
 * get_rx_icmp6
 *      Called from the dispatcher to pull in the received packet.
 *
 * Inputs:
 *  socket is where the data is waiting.
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
int get_rx_icmp6(int socket, unsigned char *msg, struct in6_addr *addr6) 
{
    struct sockaddr_in6 saddr;
    struct msghdr mhdr;
    struct iovec iov[2];
    int len;
    fd_set rfds;
    
    /* For ancillary data */
    u_char cbuf[2048];
    struct cmsghdr *cm;
    struct in6_pktinfo *thispkt6;
    char    addr_str[INET6_ADDRSTRLEN];

    FD_ZERO( &rfds );
    FD_SET( socket, &rfds );

    if( select( socket+1, &rfds, NULL, NULL, NULL ) < 0 )
    {
        if (errno != EINTR)
            flog(LOG_ERR, "select failed with: %s", strerror(errno));
        return -1;
    }

    iov[0].iov_len = MAX_MSG_SIZE;
    iov[0].iov_base = (caddr_t) msg;

    memset(&mhdr, 0, sizeof(mhdr));
    mhdr.msg_name = &saddr;
    mhdr.msg_namelen = sizeof(saddr);
    mhdr.msg_iov = iov;
    mhdr.msg_iovlen = 1;
    mhdr.msg_control = (caddr_t)cbuf;
    mhdr.msg_controllen = sizeof(cbuf);
   
    len = recvmsg(socket, &mhdr, 0);
    
    /* Src addr  - copy it for caller*/
    memcpy(addr6, &(saddr.sin6_addr), sizeof(struct in6_addr));
    print_addr(&(saddr.sin6_addr), addr_str);
    flog( LOG_DEBUG2, "RA received src address: %s", addr_str);
    
    /* Walk the chain of cmsg blocks (although unlikely to be more than 1) */
    for (cm = CMSG_FIRSTHDR(&mhdr); cm != NULL; cm = CMSG_NXTHDR(&mhdr, cm))
    {
        if (cm->cmsg_level == IPPROTO_IPV6 &&
            cm->cmsg_type  == IPV6_2292PKTINFO &&
            cm->cmsg_len   == CMSG_LEN(sizeof(struct in6_pktinfo)))
        {
            flog(LOG_DEBUG2, "Ancillary data contains in6_pktinfo.");
            thispkt6 = (struct in6_pktinfo *)CMSG_DATA(cm);
            // Dst addr
            char addr_str[INET6_ADDRSTRLEN];
            print_addr(&(thispkt6->ipi6_addr), addr_str);
            flog( LOG_DEBUG2, "RA received dst address: %s", addr_str);
        }
        else
        {
            flog(LOG_DEBUG2, "Ancillary data was unrecognised.");
        }
    }

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
 *  state: 0-> Set flag to unset condition.
 *
 * Outputs:
 *  none
 *
 * Return:
 *  The previous value of the flag, prior to change.
 * 
 * Notes:
 *  Miserere mihi peccatori.
 */
int if_allmulti(char *ifname, unsigned int state)
{
    struct ifreq    ifr;
    int skfd;
    int current;

    flog(LOG_DEBUG2, "Requesting that %s be set to state = %d",
                ifname, state);
    
    skfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    // Get current flags, etc.
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        flog(LOG_ERR, "Unknown interface: %s, failed err = %s", ifname, strerror(errno));
        exit(1);
    }

    current = ifr.ifr_flags;
    
    if (state)
    {
        flog(LOG_DEBUG, "Setting IFFALLMULTI if required.");
        ifr.ifr_flags |= IFF_ALLMULTI;
        if (ifr.ifr_flags == current)
        {
            // Already set
            flog(LOG_DEBUG, "Not required, was set at anyway.");
            goto sinfulexit;;
        }
    }
    else
    {
        flog(LOG_DEBUG, "Clearing IFFALLMULTI if required.");
        ifr.ifr_flags &= ~IFF_ALLMULTI;
    }
    
    if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0)
    {
        flog(LOG_ERR, "Flag change failed: %s, failed err = %s", ifname, strerror(errno));
        exit(1);
    }

sinfulexit:
    close(skfd);
    return (current || IFF_ALLMULTI);
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
 *  Set global sockicmp and per i/f rx pkt socket
 *
 * Return:
 *  Non-0 if failure, else 0.
 */
int init_sockets(void)
{
    int errcount = 0, err;
    int loop, sock, sockicmp;
    struct ifreq ifr;

    /* Raw socket for receiving NSs */
    for (loop=0; loop < interfaceCount; loop++)
    {
        sock = open_packet_socket(interfaces[loop].index);
  
        if (sock < 0)
        {
            flog(LOG_ERR, "open_packet_socket: failed on iteration %d", loop);
            errcount++;
        }
        interfaces[loop].pktSock = sock;
        flog(LOG_DEBUG, "open_packet_socket: %d OK.", loop);
        flog(LOG_DEBUG2, "open_packet_socket value = %d", sock);
    
        /* ICMPv6 socket for sending NAs */
        sockicmp = open_icmpv6_socket();
        if (sockicmp < 0)
        {
            flog(LOG_ERR, "open_icmpv6_socket: failed.");
            errcount++;
        }
	
        memset(&ifr, 0, sizeof(ifr));
        strcpy(ifr.ifr_name, interfaces[loop].nameStr);
	err = setsockopt(sockicmp, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr));
        if ( err < 0) 
	{
	   flog(LOG_ERR, "bindtodevice: failed.");
	   errcount ++;
	}

        flog(LOG_DEBUG, "open_icmpv6_socket: OK.");
        interfaces[loop].icmpSock = sockicmp;
    }
    
    return errcount;
}
