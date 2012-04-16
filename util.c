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

//*******************************************************
// When we receive sigusrN, do something awesome.
/*****************************************************************************
 * usersignal
 *      When dispatcher picks up a user signal, do something with it.
 *
 * Inputs:
 *  int mysig
 *      The signal received.
 *
 * Outputs:
 *  Various, depending upon the sig.
 *
 * Return:
 *      void
 *
 */
void usersignal(int mysig)
{
    switch(mysig) {
        case SIGUSR1:
            signal(SIGUSR1, usersignal);
            flog(LOG_DEBUG, "called with USR1");
            flog(LOG_INFO, "SIGUSR1 received: rereading config");
            if ( readConfig(configfile) )
            {
                flog(LOG_ERR, "Error in config file: %s", configfile);
                exit(1);
            }
            break;
         case SIGUSR2:
            signal(SIGUSR2, usersignal);
            flog(LOG_DEBUG, "called with USR2");
            dumpAddressData();
            break;
        case SIGHUP:
            signal(SIGUSR2, usersignal);
            flog(LOG_DEBUG, "called with HUP");
            /* TODO action? */
            break;
        case SIGINT:
        case SIGTERM:
            signal(SIGUSR2, usersignal);
            flog(LOG_DEBUG, "called with INT");
            /* We're dying, so handle directly here*/
            dropdead();
            break;
         default:
            flog(LOG_DEBUG, "Why am I here? Confused.");
            break;
    }
}



/*****************************************************************************
 * npd6log
 *      Log as per level to the previously defined log file or system..
 *
 * Inputs:
 *  char * fn name
 *      From the caller's __FUNCTION__
 *  int pri
 *      Log level: e.g. LOG_ERR, LOG_INFO, LOG_DEBUG, etc.
 *  char *format
 *      Standard format string.
 *  ...
 *      Variable number of params to accompany the format string.
 * GLOBAL
 *  logFileFD
 *      Previously opened lof device.
 *
 * Outputs:
 *  Via fprintf to the log file.
 *
 * Return:
 *      0 on success
 *
 * Notes:
 *      This will be called from the macro expansion of "flog()"
 *      This gets called a lot, but much of the time only does
 *      anything if the debug options are turned on. Hence it's
 *      written in a slightly odd manner to avoid data allocation
 *      or work unless we have debug turned on of it's a non-debug
 *      call.
 */
int npd6log(const char *function, int pri, char *format, ...)
{
    // Pick up debug requests and decide if we are logging them
    if (!debug && pri>=LOG_DEBUG)
    {
        return 0;  // Silent...
    }

    // Check for extra super power debug
    if ( (debug!=2) && (pri == LOG_DEBUG2) )
    {
        return 0;   // Silent...
    }

    // Normalise the debug level
    if( pri==LOG_DEBUG2)
    {
        pri=LOG_DEBUG;  // Since only we understand the two levels
    }

    // Artificial blocking of code to improve efficiency... if the compiler plays ball. :-)
    {
        time_t now;
        struct tm *timenow;
        char timestamp[128], obuff[2048];
        va_list param;

        va_start(param, format);
        vsnprintf(obuff, sizeof(obuff), format, param);
        now = time(NULL);
        timenow = localtime(&now);
        (void) strftime(timestamp, sizeof(timestamp), LOGTIMEFORMAT, timenow);

        switch (logging) {
            case USE_FILE:
                fprintf(logFileFD, "[%s] %s: %s\n", timestamp, function, obuff);
                break;
            case USE_SYSLOG:
                syslog(pri, "%s: %s\n", function, obuff);
                break;
            case USE_STD:
                if (pri <= LOG_ERR)
                    fprintf(stderr, "[%s] %s: %s\n", timestamp, function, obuff);
                else
                    fprintf(stdout, "[%s] %s: %s\n", timestamp, function, obuff);
                break;
        }

        va_end(param);

        return 0;
    }
}


/*****************************************************************************
 * print_addr
 *      Convert ipv6 address to string representation.
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
void print_addr(struct in6_addr *addr, char *str)
{
        const char *res;

        res = inet_ntop(AF_INET6, (void *)addr, str, INET6_ADDRSTRLEN);

        if (res == NULL)
        {
                flog(LOG_ERR, "print_addr: inet_ntop: %s", strerror(errno));
                strcpy(str, "[invalid address]");
        }
}


/*****************************************************************************
 * build_addr
 *      Convert char represetation of ipv6 addr to binary form.
 *
 * Inputs:
 *  char * str
 *      String representation, fully padded.
 *
 * Outputs:
 *  const struct in6_addr * addr
 *      Binary ipv6 address
 *
 * Return:
 *      return code from inet_pton
 *      1 => Good, else Bad
 */
int build_addr(char *str, struct in6_addr *addr)
{
    int ret;
    flog(LOG_DEBUG2, "called with address %s", str);
    ret = inet_pton(AF_INET6, str, (void *)addr);
    if (ret == 1)
        flog(LOG_DEBUG2, "inet_pton OK");
    else if(ret == 0)
        flog(LOG_ERR, "invalid input address");
    else
        flog(LOG_ERR, "inet_pton: %s", strerror(errno));

    return ret;
}



/*****************************************************************************
 * print_addr16
 *      Print ipv6 address in fully expanded 64-bit char form
 *
 * Inputs:
 *  const struct in6_addr * addr
 *      Binary ipv6 address
 *
 * Outputs:
 *  char * str
 *      String representation, fully padded.
 *
 * Return:
 *      void
 */
void print_addr16(const struct in6_addr * addr, char * str)
{
   sprintf(str, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                 (int)addr->s6_addr[0], (int)addr->s6_addr[1],
                 (int)addr->s6_addr[2], (int)addr->s6_addr[3],
                 (int)addr->s6_addr[4], (int)addr->s6_addr[5],
                 (int)addr->s6_addr[6], (int)addr->s6_addr[7],
                 (int)addr->s6_addr[8], (int)addr->s6_addr[9],
                 (int)addr->s6_addr[10], (int)addr->s6_addr[11],
                 (int)addr->s6_addr[12], (int)addr->s6_addr[13],
                 (int)addr->s6_addr[14], (int)addr->s6_addr[15]);
}



/*****************************************************************************
 * prefixset
 *      Take prefix in the form 1111:2222:3333:4444: and pad it to the
 *      full length
 *
 * Inputs:
 *  char * px
 *      String representation, fully padded.
 *
 * Outputs:
 *  As input
 *
 * Return:
 *      -1 on error, else bit length of unpadded prefix.
 *
 * Note:
 *  IMPORTANT! px *must* be big enough to hold full ipv6 string
 */
int prefixset(char px[])
{
    size_t len;
    int missing, c1, c2;

    // First we must ensure fully padded with leading 0s
    for(c1=0; c1<INET6_ADDRSTRLEN; c1+=5 )
    {
        len = strlen(px);
        for(c2 = 0; c2 < 4; c2++)
        {
            if (px[c1+c2] != ':')
                continue;
            else
                break;
        }

        missing = abs(c2-4);

        if (missing != 0)
        {
            char suffix[INET6_ADDRSTRLEN];
            strcpy( suffix, &px[c1]);       // Grab the tail
            memset(&px[c1], '0', missing);  // pad it with missing zeros
            px[c1+missing] = '\0';          // Reterminate
            strcat(px, suffix);             // Add the tail back
        }
    }

    len = strlen(px);
    switch (len) {
        case 5:
            strcat(px, "0000:0000:0000:0000:0000:0000:0000");
            return 16;
        case 10:
            strcat(px, "0000:0000:0000:0000:0000:0000");
            return 32;
        case 15:
            strcat(px, "0000:0000:0000:0000:0000");
            return 48;
        case 20:
            strcat(px, "0000:0000:0000:0000");
            return 64;
        case 25:
            strcat(px, "0000:0000:0000");
            return 80;
        case 30:
            strcat(px, "0000:0000");
            return 96;
        case 35:
            strcat(px, "0000");
            return 112;
        case 39:
            flog(LOG_ERR, "Full 128-bits defined as the configured prefix. Sure???");
            return 128;
        default:
            flog(LOG_ERR, "configured prefix not correctly formatted (len = %d)", len);
            return -1;
    }
}



/*****************************************************************************
 * stripwhitespace
 *  Tidy up lines of text from the config file.
 * BUG 32 - Rewite. original had v ugly bug...
 *
 * Inputs:
 *  char * str
 *      String to be tidied.
 *
 * Outputs:
 *  As input
 *
 * Return:
 *  void
 */
void stripwhitespace(char *str)
{
    char *p1 = str;
    char *p2 = str;

    p1=str;
    while(*p1 != 0) {
        if(isspace(*p1))
            ++p1;
        else
            *p2++ = *p1++;
    }
    *p2=0;
}



/*****************************************************************************
 * dumpHex
 *  Take a lump of binary data (like an ethernet frame!) and print it in hex.
 *  Useful for debugging!
 *
 * Inputs:
 *  unsigned char *data
 *      Lump of data
 *  unsigned int len
 *      Amount of data
 *
 * Outputs:
 *  Via printf to stdio
 *
 * Return:
 *  void
 */
void dumpHex(unsigned char *data, unsigned int len)
{
    int ix;

    printf("Dumping %d bytes of hex:\n>>>>>", len);

    for(ix=0; ix < len; ix++)
        printf("%02x", data[ix]);
    printf("<<<<<\n");
}


/*****************************************************************************
 * getLinkaddress
 *  Get the link-level address (i.e. MAC address) for an interface.
 *
 * Inputs:
 *  char * iface
 *      String containg the interface name (e.g. "eth1")
 *
 * Outputs:
 *  unsigned char * link
 *      String of the form "00:12:34:56:78:90"
 *
 * Return:
 *  0 is successful,
 *  1 if error.
 */
int getLinkaddress( char * iface, unsigned char * link) {
    int sockfd, io;
    struct ifreq ifr;

    strncpy( ifr.ifr_name, iface, INTERFACE_STRLEN );

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        flog(LOG_ERR, "failed to open a test SOCK_STREAM.");
        return 1;
    }

    io = ioctl(sockfd, SIOCGIFHWADDR, (char *)&ifr);
    if(io < 0) {
        flog(LOG_ERR, "octl failed to obtain link address");
        return 1;
    }

    memcpy(link, (unsigned char *)ifr.ifr_ifru.ifru_hwaddr.sa_data, 6);

    close(sockfd);

    return 0;
}


//*******************************************************
// Take the supplied filename and open it for logging use.
// Upon return, logFileFD set unless we failed.
int openLog(char *logFileName)
{
    if (logging == USE_FILE)
    {
        if ((logFileFD = fopen(logFileName, "a")) == NULL)
        {
            fprintf(stderr, "Can't open %s: %s\n", logFileName, strerror(errno));
            return (-1);
        }
        // Set line-buffering so we don't need to explicitly fflush()
        // when we write via npd6log
        setlinebuf(logFileFD);
        return 0;
    }

    if (logging == USE_SYSLOG)
    {
        openlog("npd6", (LOG_NDELAY|LOG_PID),LOG_DAEMON);
        return 0;
    }

    // Error
    return -1;
}


//*******************************************************
// Just display the version and return.
void showVersion(void)
{
    printf("Beta version, so showing SVN version id for this file:\n");
    printf("Version $Revision$\n");
    printf("Tagged version ID is >= 0.4.3\n");
}


void dropdead(void)
{
    /* We're dying, so tidy up*/

    /* Restore interface flags*/
    if_allmulti(interfacestr, 0);

    close(sockicmp);
    close(sockpkt);

    flog(LOG_ERR, "Tidied up. Goodbye cruel world.");
    exit(0);
}


/*****************************************************************************
 * dumpData
 *  Dump internal data. Initially this will mean the set of collected
 *  target addresses seen (if that option is enabled)
 *
 * Inputs:
 *  tRoot is the tree of collected targets.
 *
 * Outputs:
 *  Data is dumped to the defined log.
 *
 * Return:
 *  Void
 */
void dumpAddressData(void)
{
    if (!collectTargets)
    {
        flog(LOG_INFO, "Not dumping collected addresses - feature disabled via config.");
        return;
    }

    flog(LOG_INFO, "====================================");
    flog(LOG_INFO, "Dumping list of targets seen so far:");
    flog(LOG_INFO, "------------------------------------");

    twalk(tRoot, tDump);

    if (tEntries == collectTargets)
    {
        flog(LOG_INFO, "(reached the configured limit - there were maybe more.)");
    }

    flog(LOG_INFO, "Total unique targets seen: %d", tEntries);
    flog(LOG_INFO, "====================================");
}


/*****************************************************************************
 * storeTarget
 *  Look in tRoot tree to see if we have it already. If we don't store
 *  it in the tree. If we do have it, then ignore.
 *
 * Inputs:
 *  in6_addr *Target - this is the newly seen target to check
 *
 * Outputs:
 *  tRoot has a new item added if the address was new.
 *
 * Return:
 *  Void
 */
void storeTarget(struct in6_addr *newTarget)
{
    struct in6_addr *ptr;

    // Take a permanenet copy of the target
    ptr = (struct in6_addr *)malloc(sizeof(struct in6_addr) );
    if (!ptr)
    {
        flog(LOG_ERR, "Malloc failed. Ignoring.");
    }
    memcpy(ptr, newTarget, sizeof(struct in6_addr) );

    // We can't just tsearch() it into the tree, as there's no
    // way to then know if the entry already existed or is now newly created.
    // Hence we can't know to bump the count. So we tfind() first
    // and only tsearch() if required. In a typical net the initial
    // tfind() is going to return a result almost all the time, so the
    // overhead of this double-call is actually low.
    if ( tfind( (void *)ptr, &tRoot, tCompare) == NULL )
    {
        if (tEntries >= collectTargets)
        {
            flog(LOG_INFO, "Reached max threshold of recorded targets (%d). Not recording.", collectTargets);
            return;
        }
        // New entry
        flog(LOG_DEBUG2, "New entry - recording.");
        if ( tsearch( (void *)ptr, &tRoot, tCompare) == NULL)
        {
            flog(LOG_ERR, "tsearch failed. Cannot record entry.");
            return;
        }
        else
        {
            tEntries++;
        }
    }
    else
    {
        flog(LOG_DEBUG2, "Entry already recorded. Ignoring.");
    }
}

/*****************************************************************************
 * tCompare
 *  This is the compare fn used by the tree handler.
 *
 * Inputs:
 *  The two generic pointers are struct in6_addr *.
 *
 * Outputs:
 *  None.
 *
 * Return:
 *  0 if item already present, 1 if it was new.
 *
 * Notes:
 * Need to compare two 128 bit numbers! Yucky.
 * On 64-bit, we could assume long int => 64 bit, but
 * given we may be on 32-bit, need to assume long int
 * is max 32 bit, as per ANSI.
 *
 * We also make use of the underlying structure of in6_addr,
 * which is int[16]
 *
 * Needs to be moderately efficient, since if we're recording
 * a lot of addresses we call this via tfind/tsearch quite a lot,
 * so we do minimal-comparison.
 */
int tCompare(const void *pa, const void *pb)
{
    int paI=0, pbI=0;
    int idx;

    for(idx=0; idx<=15; idx++)
    {
        paI = ((struct in6_addr *)pa)->s6_addr[idx];
        pbI = ((struct in6_addr *)pb)->s6_addr[idx];

        if (paI == pbI)
            continue;
        if (paI < pbI)
            return -1;
        else
            return 1;
    };

    // If we reach here, the items were identical
    return 0;
}


/*****************************************************************************
 * tDump
 *  This is the action used when walking tRoot from dumpData()
 *
 * Inputs:
 *  node, type of visit, depth - Check the man page for more...
 *
 * Outputs:
 *  Data dumped to log.
 *
 * Return:
 *  void
 */
void tDump(const void *nodep, const VISIT which, const int depth)
{
    struct in6_addr *data;
    char addressString[INET6_ADDRSTRLEN];

    switch (which) {
        case preorder:
        case endorder:
            break;
        case postorder:
        case leaf:
            data = *(struct in6_addr **) nodep;
            print_addr(data, addressString);
            flog(LOG_INFO, "Address: %s", addressString);
            break;
    }
}


/*****************************************************************************
 * storeListEntry
 *
 * Inputs:
 *  in6_addr *Target - this is the newly seen target to check
 *
 * Outputs:
 *  lRoot has a new item added if the address was new.
 *
 * Return:
 *  Void
 */
void storeListEntry(struct in6_addr *newEntry)
{
    struct in6_addr *ptr;

    // Take a permanenet copy of the target
    ptr = (struct in6_addr *)malloc(sizeof(struct in6_addr) );
    if (!ptr)
    {
        flog(LOG_ERR, "Malloc failed. Ignoring.");
    }
    memcpy(ptr, newEntry, sizeof(struct in6_addr) );

    if ( tfind( (void *)ptr, &lRoot, tCompare) == NULL )
    {
        // New entry
        flog(LOG_DEBUG2, "New list entry");
        if ( tsearch( (void *)ptr, &lRoot, tCompare) == NULL)
        {
            flog(LOG_ERR, "tsearch failed. Cannot record entry.");
            return;
        }
    }
    else
    {
        flog(LOG_ERR, "Dupe list entry. Ignoring.");
    }
}





