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

#include "expintf.h"

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
 *      The length of the received (candidate) NS.
 *      *** This has already been sanity checked back in the callers ***
 *
 * Outputs:
 *  Potentially, sends the Neighbor Advertisement.
 *
 * Return:
 *      void
 *
 */
void processNS( int ifIndex,
                unsigned char *msg,
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
    
    // For the interfaceIdx
    struct  in6_addr            prefixaddr = interfaces[ifIndex].prefix;
    int                         prefixaddrlen = interfaces[ifIndex].prefixLen;
    unsigned char               *linkAddr = interfaces[ifIndex].linkAddr;
    int                         interfaceIdx = interfaces[ifIndex].index;
    
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
        return;
    }
    
    // Bug 27 - Handle DAD NS as per RFC4862, 5.4.3
    if ( IN6_IS_ADDR_UNSPECIFIED(srcaddr) )
    { 
        flog(LOG_DEBUG, "Unspecified src addr - DAD activity. Ignoring NS.");
        return;
    }
    
    // Based upon the dstaddr, record if this was a unicast or multicast NS.
    // If unicast, we'll use that later when we decide whether to add the
    // target link-layer option to any outgoing NA.
    if ( IN6_IS_ADDR_MULTICAST(dstaddr) )
    {
        // This was a multicast NS
        flog(LOG_DEBUG2, "Multicast NS");
        multicastNS = 1;
    }else
    {
        // This was a unicast NS
        flog(LOG_DEBUG2, "Unicast NS");
        multicastNS=0;
    }
    
    // Within the NS, who are they looking for?
    targetaddr = (struct in6_addr *)&(ns->nd_ns_target);
    if (debug || listLog)
    {
        print_addr16(targetaddr, targetaddr_str);
        print_addr16(&prefixaddr, prefixaddr_str);
        flog(LOG_DEBUG, "NS target addr: %s", targetaddr_str);
        flog(LOG_DEBUG, "Local prefix: %s", prefixaddr_str);
    }
    
    // If tgt-addr == dst-addr then ignore this, as the automatic mechanisms
    // will reply themselves - we don't need to.
    if ( nsIgnoreLocal && IN6_ARE_ADDR_EQUAL(targetaddr, dstaddr) )
    {
        flog(LOG_DEBUG, "tgt==dst - Ignore.");
        return;
    }
    
    // Check for black or white listing compliance
    switch (listType) {
        case NOLIST:
            flog(LOG_DEBUG2, "Neither white nor black listing in operation.");
            break;
            
        case BLACKLIST:
            // See if the address matches an expression
            if((compareExpression(targetaddr) == 1))
            {
                flog(LISTLOGGING, "NS for blacklisted EXPR address: %s", targetaddr_str);
                return; // Abandon
            }
            // If active and tgt is in the list, bail.
            if ( tfind( (void *)targetaddr, &lRoot, tCompare) )
            {
                flog(LISTLOGGING, "NS for blacklisted specific addr: %s", targetaddr_str);
                return; //Abandon
            }
            break;
            
        case WHITELIST:
            // See if the address matches an expression
            if((compareExpression(targetaddr) == 1))
            {
                flog(LISTLOGGING, "NS for whitelisted EXPR: %s", targetaddr_str);
                break;	// Don't check further - we got a hit.
            }
            
            // If active and tgt is NOT in the list (and didn't match an expr above), bail.
            if ( tfind( (void *)targetaddr, &lRoot, tCompare) )
            {
                flog(LISTLOGGING, "NS for specific addr whitelisted: %s", targetaddr_str);
                break;
            }
            else
            {
                // We have whitelisting in operation but failed to match either type. 
                // Log it if required.
                flog(LOG_DEBUG, "No whitelist match for: %s", targetaddr_str);
                return;
            }
            break;
    }
    
    // Does it match our configured prefix that we're interested in?
    if (! addr6match( targetaddr, &prefixaddr, prefixaddrlen) )
    {
        flog(LOG_DEBUG, "Target/:prefix - Ignore NS.");
        return;
    }
    else
    {
        flog(LOG_DEBUG, "Target:prefix - Build NA response.");
        
        // If configured, log target to list
        if (collectTargets)
        {
            flog(LOG_DEBUG, "Store target to list.");
            storeTarget( targetaddr );
        }
        
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
        if (naRouter)
        {
            nad->nd_na_flags_reserved |= ND_NA_FLAG_SOLICITED | ND_NA_FLAG_ROUTER;
        }
        else
        {
            nad->nd_na_flags_reserved |= ND_NA_FLAG_SOLICITED;
        }
        
        memcpy(&(nad->nd_na_target), targetaddr, sizeof(struct in6_addr) );
        
        if (multicastNS || naLinkOptFlag)
        {
            // If the NS that came in was to a multicast address
            // or if we have forced the option for all packets anyway
            // then add a target link-layer option to the outgoing NA.
            // Per rfc, we must add dest link-addr option for NSs that came
            // to the multicast group addr.
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
            iovlen = sizeof(struct nd_neighbor_advert);
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
        
        err = sendmsg( interfaces[ifIndex].icmpSock, &mhdr, 0);
        if (err < 0)
            flog(LOG_ERR, "sendmsg returned with error %d = %s", errno, strerror(errno));
        else
            flog(LOG_DEBUG2, "sendmsg completed OK");
        
    }
}


/*****************************************************************************
 * processICMP
 * Takes a received ICMP message and handles it. At first, we don't
 * do too much. Based upon NFR 60 we are going to look out for RAs, 
 * extract useful data and log it.
 * 
 * Later on we may go further with that information...
 *
 * Inputs:
 *  char *msg
 *      The received ICMP6.
 *  int len
 *      The length of the received data
 *      *** This has already been sanity checked back in the callers ***
 *
 * Outputs:
 *  As per above, likely just a log/debug for now.
 *
 * Return:
 *      void
 *
 */
void processICMP( int ifIndex,
                  unsigned char *msg,
                  unsigned int len,
                  struct in6_addr *addr6)
{
    // Offsets into the received packet
    struct icmp6_hdr            *icmph = 
    (struct icmp6_hdr *)(msg);
    struct nd_router_advert     *ra = 
    (struct nd_router_advert *)(msg + sizeof(struct icmp6_hdr));

    uint32_t reachableT = ra->nd_ra_reachable;
    uint32_t retransmitT = ra->nd_ra_retransmit;  
    int curHopLimit = ra->nd_ra_curhoplimit;
    int rtrLifetime = ra->nd_ra_router_lifetime;
    
    int counter = 0;
    char addr6_str[INET6_ADDRSTRLEN];
    
    // May not exist - will check
    struct nd_opt_hdr *optHdr = 
    (struct nd_opt_hdr *)(msg + sizeof(struct nd_router_advert));
    
    flog(LOG_DEBUG, "Check for RA in received ICMP6.");
    
    if ( icmph->icmp6_type == ND_ROUTER_ADVERT )
    {            
        print_addr(addr6, addr6_str);
        flog(LOG_INFO, "RA received from address: %s", addr6_str);
        flog(LOG_DEBUG, "Reachable timer = %ld, retransmit timer = %ld", ntohl(reachableT), ntohl(retransmitT));
        flog(LOG_DEBUG, "Cur Hop Limit = %d, Router Lifetime = %d", curHopLimit, rtrLifetime);
        
        counter = sizeof(struct nd_router_advert);
        while (counter < len)
        {
            uint8_t     optionLen; /* n.b. Octets */
            uint8_t     prefixLen;
            uint32_t    prefixValidTime, prefixPreferredTime;
            struct      in6_addr prefixPrefix;
            char        prefixPrefix_str[INET6_ADDRSTRLEN];
            struct      nd_opt_prefix_info   *prefixInfo;
            unsigned char        *linkAddr;
            int         watchDog=0;
            
            // So offset optHdr is now valid and points to 1 or more options
            switch(optHdr->nd_opt_type) {
                case ND_OPT_SOURCE_LINKADDR:
                    flog(LOG_DEBUG, "RA-opt received: Source Link Address");
                    linkAddr = ((unsigned char *)(optHdr) + 2);                  
                    flog(LOG_DEBUG, "Link address: %02x:%02x:%02x:%02x:%02x:%02x",
                         linkAddr[0], linkAddr[1], linkAddr[2], linkAddr[3], linkAddr[4], linkAddr[5]);
                    break;
                    
                case ND_OPT_TARGET_LINKADDR:
                    flog(LOG_DEBUG, "RA-opt received: Target Link Address");
                    linkAddr = ((unsigned char *)(optHdr) + 2);                  
                    flog(LOG_DEBUG, "Link address: %02x:%02x:%02x:%02x:%02x:%02x",
                         linkAddr[0], linkAddr[1], linkAddr[2], linkAddr[3], linkAddr[4], linkAddr[5]);    
                    break;
                    
                case ND_OPT_PREFIX_INFORMATION:
                    flog(LOG_INFO, "RA-opt received: Prefix Info");
                    prefixInfo =            (struct nd_opt_prefix_info *)optHdr;
                    prefixLen =             prefixInfo->nd_opt_pi_prefix_len;
                    prefixValidTime =       prefixInfo->nd_opt_pi_valid_time;
                    prefixPreferredTime =   prefixInfo->nd_opt_pi_preferred_time;
                    prefixPrefix =          prefixInfo->nd_opt_pi_prefix;
                    
                    print_addr(&prefixPrefix, prefixPrefix_str);
                    flog(LOG_INFO, "Received prefix is: %s", prefixPrefix_str);
                    flog(LOG_INFO, "Prefix length: %d", prefixLen);
                    flog(LOG_INFO, "Valid time: %ld", ntohl(prefixValidTime));
                    flog(LOG_INFO, "Preferred time: %ld", ntohl(prefixPreferredTime));
                    break;
                    
                case ND_OPT_REDIRECTED_HEADER:
                    flog(LOG_DEBUG, "RA-opt received: Redirected Header");
                    break;
                    
                case ND_OPT_MTU:
                    flog(LOG_DEBUG, "RA-opt received: MTU");
                    break;
                    
                case ND_OPT_RTR_ADV_INTERVAL:
                    flog(LOG_DEBUG, "RA-opt received: RA Interval");
                    break;
                    
                case ND_OPT_HOME_AGENT_INFO:
                    flog(LOG_DEBUG, "RA-opt received: Home Agent Info");
                    break; 
                    
                default:
                    // *** Important default ***
                    // Got an option that we cannot recognise - log and skip it
                    flog(LOG_ERR, "Had option type = %d  - do not recognise.", optHdr->nd_opt_type);                                  
            }
            
            // Sanity check to catch runaway situation with corrupt packet (malicious or otherwise!)
            watchDog++;
            if(watchDog > 20) // 20 seems enough to say STOP!
            {
                flog(LOG_ERR, "Tripped watchdog in ICMP option decoding... Something very odd...");
                return;
            }
            
            // Increment to (possible) next opt
            optionLen = (optHdr->nd_opt_len) * 8;
            counter += optionLen;
            // 32/64-bit agnostic
            optHdr = (struct nd_opt_hdr *) ((char *)optHdr + optionLen);
        }    
    }
    else
    {
        flog(LOG_ERR, "Received ICMP6 - did not recognise it.");
        flog(LOG_ERR, "Type was %d", icmph->icmp6_type);
        return;
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
    unsigned int mask;
    
    flog(LOG_DEBUG2, "Called to match up to %d bits.", bits);
    
    if (bits > 128)
    {
        flog(LOG_ERR, "Bits > 128 (%d) does not make sense.", bits);
        return 0;
    }
    
    // The approach here is to gallop along the address comparing full octets for as far as possible.
    // Then when/if we reach a non-octet aligned point, we deal with that.
    // Since vast majority of folks will have octet aligned prefixes, this is highly efficient
    for (bdx=bits, idx=0; bdx>0; bdx-=8, idx++)
    {
        if (bdx >= 8)
        {
            // We can compare a full 8-bit comparison - no masking yet
            if ( a1->s6_addr[idx] != a2->s6_addr[idx] )
            {
                //Failed to match
                flog(LOG_DEBUG2, "Failed in 8-bit match test.idx = %d, bdx = %d", idx, bdx);
                flog(LOG_DEBUG2, "a1 value: %2x a2 value: %2x", a1->s6_addr[idx], a2->s6_addr[idx]);
                return 0;
            }
        }
        else 
        {
            // We are in the end-zone - masking required
            switch (bdx) {
                case 7: mask=0xfe;  break;
                case 6: mask=0xfc;  break;
                case 5: mask=0xf8;  break;
                case 4: mask=0xf0;  break;
                case 3: mask=0xe0;  break;
                case 2: mask=0xc0;  break;
                case 1: mask=0x80;  break;
            }
            if ( ((a1->s6_addr[idx])&mask) != a2->s6_addr[idx] )
            {
                //Failed to match
                flog(LOG_DEBUG2, "Failed in mask match test.idx = %d, bdx = %d, mask = %04x", idx, bdx, mask);
                flog(LOG_DEBUG2, "a1 value: %04x a2 value: %04x", a1->s6_addr[idx], a2->s6_addr[idx]);
                return 0;
            }
        }
    }
    
    flog(LOG_DEBUG2, "Target and prefix matched up to bit position %d", bits);
    
    return 1;
}

