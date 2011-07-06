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
 * processNS
 *  Takes a received Neighbor Solicitation and handles it. Main logic is:
 *      - bit of extra validation.
 *      - determine who it's asking about.
 *      - see if that matches the prefix we are looking after.
 *      - if it does, send a Neighbor Advertisement.
 *
 *  For more, see the inline comments - There's a lot going on here.
 *
 * Inputs:
 *  char *msg
 *      The received NS.
 *  int len
 *      The length of the received NS.
 *
 * Outputs:
 *  Potentially, sends the Neighbor Advertisement.
 *
 * Return:
 *      void
 *
 */
void processNS( unsigned char *msg,
                unsigned int len)
{
    // String representations of the various addresses
    char                        targetaddr_str[INET6_ADDRSTRLEN];
    char                        prefixaddr_str[INET6_ADDRSTRLEN];
    char                        srcaddr_str[INET6_ADDRSTRLEN];
    char                        dstaddr_str[INET6_ADDRSTRLEN];

    // Offsets into the received packet
    struct ip6_hdr              *ip6h =
        (struct ip6_hdr *)(msg + ETH_HLEN);
    struct icmp6_hdr            *icmph =
        (struct icmp6_hdr *)(msg + ETH_HLEN + sizeof( struct ip6_hdr));
    struct nd_neighbor_solicit  *ns =
        (struct nd_neighbor_solicit *)(msg + ETH_HLEN + sizeof( struct ip6_hdr));

    // Extracted from the received packet
    struct in6_addr             *srcaddr;
    struct in6_addr             *dstaddr;
    struct in6_addr             *targetaddr;
    unsigned int                multicastNS;
    
    // For outgoing NA
    struct in6_addr             srcLinkAddr = IN6ADDR_ANY_INIT;
    struct in6_pktinfo          *pkt_info;
    struct sockaddr_in6         sockaddr;
    unsigned char               nabuff[MAX_PKT_BUFF];
    struct nd_neighbor_advert   *nad;
    size_t                      iovlen=0;
    struct iovec                iov;
    struct cmsghdr              *cmsg;
    char __attribute__((aligned(8))) chdr[CMSG_SPACE(sizeof(struct in6_pktinfo))];
    struct msghdr               mhdr;
    ssize_t                     err;
    struct nd_opt_hdr           *opthdr;
    void                        *optdata;
    

    // Validate ICMP packet type, to ensure filter was correct
    // In theory not required, as the filter CAN'T be wrong...!
    if ( icmph->icmp6_type == ND_NEIGHBOR_SOLICIT )
    {
        flog(LOG_DEBUG2, "Confirmed packet as icmp6 Neighbor Solicitation.");
        srcaddr = &ip6h->ip6_src;
        dstaddr = &ip6h->ip6_dst;
        if (debug)
        {
            print_addr(srcaddr, srcaddr_str);
            print_addr(dstaddr, dstaddr_str);
            flog( LOG_DEBUG, "src addr = %s", srcaddr_str);
            flog( LOG_DEBUG, "dst addr = %s", dstaddr_str);
        }
    }
    else
    {
        flog(LOG_ERR, "Received impossible packet... filter failed. Oooops.");
        exit(1);
    }

    // TODO
    // Based upon the dstaddr, record if this was a unicast or multicast NS.
    // If unicast, we'll use that later when we decide whether to add the
    // target link-layer option to any outgoing NA.
    if (1)
    {
        // This was a multicast NS
        multicastNS = 1;
    }else
    {
        // This was a unicast NS
        multicastNS=0;
    }

    // Within the NS, who are they looking for?
    targetaddr = (struct in6_addr *)&(ns->nd_ns_target);
    if (debug)
    {
        print_addr16(targetaddr, targetaddr_str);
        print_addr16(&prefixaddr, prefixaddr_str);
        flog(LOG_DEBUG, "NS has target addr: %s", targetaddr_str);
        flog(LOG_DEBUG, "Target prefix configured is: %s", prefixaddr_str);
    }

    // Does it match our configured prefix that we're interested in?
    if (! addr6match( targetaddr, &prefixaddr, 32) )
    {
        flog(LOG_DEBUG, "Target and prefix do not match. Ignoring NS");
        return;
    }
    else
    {
        flog(LOG_DEBUG, "Target and prefix match");

        // Start building up the header for the packet
        memset(( void *)&sockaddr, 0, sizeof(struct sockaddr_in6));
        sockaddr.sin6_family = AF_INET6;
        sockaddr.sin6_port = htons(IPPROTO_ICMPV6);
        
        // Set the destination of the NA
        memcpy(&sockaddr.sin6_addr, srcaddr, sizeof(struct in6_addr));

        // Set up the NA itself
        memset( nabuff, 0, sizeof(nabuff) );
        nad = (struct nd_neighbor_advert *)nabuff;
        nad->nd_na_type = ND_NEIGHBOR_ADVERT;
        nad->nd_na_code = 0;
        nad->nd_na_cksum = 0;
        nad->nd_na_flags_reserved |= ND_NA_FLAG_SOLICITED;
        memcpy(&(nad->nd_na_target), targetaddr, sizeof(struct in6_addr) );

        if (multicastNS || naLinkOptFlag)
        {
            // Per rfc, we must add dest link-addr option for NSs that came
            // to the multicast group addr. And do it anyway for all if the config
            // option was set.
            opthdr = (struct nd_opt_hdr *)&nabuff[sizeof(struct nd_neighbor_advert)] ;
            opthdr->nd_opt_type = ND_OPT_TARGET_LINKADDR;
            opthdr->nd_opt_len = 1; // Units of 8-octets
            optdata = (unsigned char *) (opthdr + 1);
            memcpy( optdata, linkAddr, 6);

            // Build the io vector
            iovlen = sizeof(struct nd_neighbor_advert) + sizeof(struct nd_opt_hdr) + ETH_ALEN;
            iov.iov_len = iovlen;
            iov.iov_base = (caddr_t) nabuff;
        } else
        {
            // The NS was unicast AND the config option was unset.
            // Build the io vector
            iovlen = sizeof(struct nd_neighbor_advert) + ETH_ALEN;
            iov.iov_len = iovlen;
            iov.iov_base = (caddr_t) nabuff;
        }
        
        // Build the cmsg
        memset(chdr, 0, sizeof(chdr) );
        cmsg = (struct cmsghdr *) chdr;
        cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo) );
        cmsg->cmsg_level = IPPROTO_IPV6;
        cmsg->cmsg_type = IPV6_PKTINFO;

        pkt_info = (struct in6_pktinfo *)CMSG_DATA(cmsg);

        // Set src (sending) addr and outgoing i/f for the datagram            
        memcpy(&pkt_info->ipi6_addr, &srcLinkAddr, sizeof(struct in6_addr) );
        pkt_info->ipi6_ifindex = interfaceIdx;        
        
        // Build the mhdr
        memset(&mhdr, 0, sizeof(mhdr) );
        mhdr.msg_name = (caddr_t)&sockaddr;
        mhdr.msg_namelen = sizeof(sockaddr);
        mhdr.msg_iov = &iov;
        mhdr.msg_iovlen = 1;
        mhdr.msg_control = (void *) cmsg;
        mhdr.msg_controllen = sizeof(chdr);

        flog(LOG_DEBUG2, "Outbound message built");

        err = sendmsg( sockicmp, &mhdr, 0);
        if (err < 0)
            flog(LOG_ERR, "sendmsg returned with error %d = %s", errno, strerror(errno));
        else
            flog(LOG_DEBUG2, "sendmsg completed OK");
        
    }
}


/*****************************************************************************
 * addr6match
 *      Compare two binary ipv6 addresses and see if they match
 *      in the first N bits.
 *
 * Inputs:
 *  a1 & a2 are the addresses to be compared, in form in6_addr.
 *  bits is the number of bits to compare, starting from the left.
 *
 * Outputs:
 *  void
 *
 * Return:
 *      1 if we match, else 0.
 *
 */
int addr6match( struct in6_addr *a1, struct in6_addr *a2, int bits)
{
    int idx, bdx;

    if (bits > 64)
    {
        flog(LOG_ERR, "Bits > 64 (%d) does not make sense.", bits);
        return 0;
    }
    
    for (bdx=1,idx=0; bdx<=bits; bdx+=4, idx++)
    {
        if ( a1->s6_addr[idx] != a2->s6_addr[idx])
        {
            flog(LOG_DEBUG2, "Match failed at bit position %d", bdx);
            return 0;
        }
    }

    flog(LOG_DEBUG2, "Target and prefix matched up to bit position %d", bits);

    return 1;
}

