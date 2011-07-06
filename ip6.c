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

//*******************************************************************
// Called by the dispatcher when we know that a NS has arrived.
// Here the flow needs to be:
// - Obtain the NS's target address
// - Compare the target address's prefix with the configured prefix
// - If they match, we will respond with a NA.
void processNS( unsigned char *msg,
                unsigned int len)
{
    // Offsets into the received packet
    struct ip6_hdr *ip6h;
    struct icmp6_hdr *icmph;
    struct nd_neighbor_solicit *ns;
    // Extracted from the received packet
    struct in6_addr *srcaddr;
    struct in6_addr *dstaddr;
    struct in6_addr *targetaddr;
    // For sending out a response
    struct in6_pktinfo *pkt_info;
    // String representations of the various addresses
    char targetaddr_str[INET6_ADDRSTRLEN];
    char prefixaddr_str[INET6_ADDRSTRLEN];
    char srcaddr_str[INET6_ADDRSTRLEN];
    char dstaddr_str[INET6_ADDRSTRLEN];

    ip6h = (struct ip6_hdr *)(msg + ETH_HLEN);
    icmph = (struct icmp6_hdr *)(msg + ETH_HLEN + sizeof( struct ip6_hdr));
    ns = (struct nd_neighbor_solicit *)(msg + ETH_HLEN + sizeof( struct ip6_hdr));

    // Validate ICMP packet type, to ensure filter was correct
    if ( icmph->icmp6_type == ND_NEIGHBOR_SOLICIT )
    {
        flog(LOG_DEBUG, "processNS: Validated packet as icmp6 neighbor solicitation.");

        srcaddr = &ip6h->ip6_src;
        dstaddr = &ip6h->ip6_dst;
        print_addr(srcaddr, srcaddr_str);
        print_addr(dstaddr, dstaddr_str);
        flog( LOG_DEBUG, "src addr = %s", srcaddr_str);
        flog( LOG_DEBUG, "dst addr = %s", dstaddr_str);
    }
    else
    {
        flog(LOG_ERR, "processNS: received impossible packet... filter failed. Oooops.");
        exit(1);
    }

    // Within the NS, who are they looking for?
    targetaddr = (struct in6_addr *)&(ns->nd_ns_target);
    
    print_addr16(targetaddr, targetaddr_str);
    print_addr16(&prefixaddr, prefixaddr_str);
    flog(LOG_DEBUG, "processNS: NS has target addr: %s", targetaddr_str);
    flog(LOG_DEBUG, "processNS: Target prefix configured is: %s", prefixaddr_str); 

    // Does it match our configured prefix that we're interested in?
    if (! addr6match( targetaddr, &prefixaddr, 32) )
    {
        flog(LOG_DEBUG, "processNS: target and prefix do not match. Ignoring NS");
        return;
    }
    else
    {
        struct sockaddr_in6 sockaddr;
        unsigned char nabuff[MAX_PKT_BUFF];
        struct nd_neighbor_advert *nad;
        size_t len=0;
        struct iovec iov;
        struct cmsghdr *cmsg;
        char __attribute__((aligned(8))) chdr[CMSG_SPACE(sizeof(struct in6_pktinfo))];
        struct msghdr mhdr;
        ssize_t err;

        struct nd_opt_hdr   *opthdr;
        void *optdata;
        
        
        flog(LOG_DEBUG, "processNS: target and prefix match");
        flog(LOG_DEBUG, "processNS: Build NA to send back to source: %s", srcaddr_str);

        // Start building up the header for the packet
        memset(( void *)&sockaddr, 0, sizeof(struct sockaddr_in6));
        sockaddr.sin6_family = AF_INET6;
        sockaddr.sin6_port = htons(IPPROTO_ICMPV6);
        
        // Set the destination of the NA
        memcpy(&sockaddr.sin6_addr, srcaddr, sizeof(struct in6_addr));
        flog(LOG_DEBUG, "processNS: SOCKADDR building complete");


        // Set up the NA itself
        memset( nabuff, 0, sizeof(nabuff) );
        nad = (struct nd_neighbor_advert *)nabuff;
        nad->nd_na_type = ND_NEIGHBOR_ADVERT;
        nad->nd_na_code = 0;
        nad->nd_na_cksum = 0;
        nad->nd_na_flags_reserved |= ND_NA_FLAG_SOLICITED;

        memcpy(&(nad->nd_na_target), targetaddr, sizeof(struct in6_addr) );

        // Per rfc, we must add dest link-addr option for NSs that came
        // to the multicast group addr. And since we can optionally so
        // so for unicast NSs anyway, we'll set it for all.
        opthdr = (struct nd_opt_hdr *)&nabuff[sizeof(struct nd_neighbor_advert)] ;
        
        opthdr->nd_opt_type = ND_OPT_TARGET_LINKADDR;
        opthdr->nd_opt_len = 1; // Units of 8-octets

        optdata = (unsigned char *) (opthdr + 1);

        memcpy( optdata, linkAddr, 6);
        
        flog(LOG_DEBUG, "processNS: NA building complete");

        // Build the io vector
        len = sizeof(struct nd_neighbor_advert) + sizeof(struct nd_opt_hdr) + 6;
        iov.iov_len = len;
        iov.iov_base = (caddr_t) nabuff;
        flog(LOG_DEBUG, "processNS: IO vector building complete");

        // Build the cmsg
        memset(chdr, 0, sizeof(chdr) );
        cmsg = (struct cmsghdr *) chdr;
        cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo) );
        cmsg->cmsg_level = IPPROTO_IPV6;
        cmsg->cmsg_type = IPV6_PKTINFO;
        flog(LOG_DEBUG, "processNS: CMSG building complete");


        
        pkt_info = (struct in6_pktinfo *)CMSG_DATA(cmsg);

        // Set src (sending) addr and outgoing i/f for the datagram
        {
            struct in6_addr wildcardAddr;
            build_addr("fe80::240:63ff:fef5:f93c", &wildcardAddr);
        memcpy(&pkt_info->ipi6_addr, &wildcardAddr, sizeof(struct in6_addr) );
        pkt_info->ipi6_ifindex = interfaceIdx;
        
        flog(LOG_DEBUG, "processNS: PKTINFO building complete");
        }
        
        // Build the mhdr
        memset(&mhdr, 0, sizeof(mhdr) );
        mhdr.msg_name = (caddr_t)&sockaddr;
        mhdr.msg_namelen = sizeof(sockaddr);
        mhdr.msg_iov = &iov;
        mhdr.msg_iovlen = 1;
        mhdr.msg_control = (void *) cmsg;
        mhdr.msg_controllen = sizeof(chdr);

        flog(LOG_DEBUG, "processNS: MHDR building complete");

        err = sendmsg( sockicmp, &mhdr, 0);
        if (err < 0)
            flog(LOG_ERR, "sendmsg returned with error %d = %s", errno, strerror(errno));
        else
            flog(LOG_DEBUG, "sendmsg completed OK");
        
    }
}

//*******************************************************************
// Compares a1 and a2 up to the bit limit.
// Returns 1 if they match, else 0
int addr6match( struct in6_addr *a1, struct in6_addr *a2, int bits)
{
    int idx, bdx;
    
    for (bdx=1,idx=0; bdx<=bits; bdx+=4, idx++)
    {
        if ( a1->s6_addr[idx] != a2->s6_addr[idx])
        {
            flog(LOG_DEBUG, "addr6match: match failed at bit position %d", bdx);
            return 0;
        }
    }

    flog(LOG_DEBUG, "addr6match: target and prefix matched up to bit position %d", bits);

    return 1;
}

