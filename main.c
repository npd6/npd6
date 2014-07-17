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

char usage_str[] =
{
    "\n"
    "  -c, --config=PATH      Sets the config file.  Default is /etc/npd6.conf.\n"
    "  -d, --debug            Sets the normal debug level.\n"
    "  -D, --debug2           Sets full debug level. (Lots!)\n"
    "  -f, --foreground       Run in foreground, otherwise daemonize.\n"
    "  -h, --help             Show this help screen.\n"
    "  -l  --logfile=PATH     Sets the log file, else default to syslog. If \"-l -\" then stdout/stderr used.\n"
    "  -v, --version          Print the version and quit.\n"
};

struct option prog_opt[] =
{
    {"config",  required_argument, 0, 'c'},
    {"debug",   optional_argument, 0, 'd'},
    {"debug2",  optional_argument, 0, 'D'},
    {"help",    optional_argument, 0, 'h'},
    {"logfile", required_argument, 0, 'l'},
    {"version", optional_argument, 0, 'v'},
    {"foreground", optional_argument, 0, 'f'},
    {0, 0, 0, 0}
};

#define OPTIONS_STR "c:dDhl:vf"

struct Interface *IfaceList = NULL;

int main(int argc, char *argv[])
{
    char logfile[FILENAME_MAX] = "";
    int c, err, loop;
    
    // Default some globals
    strncpy(configfile, NPD6_CONF, FILENAME_MAX);
    daemonize=1;
    // Default black/whitelisting to OFF
    listType = NOLIST;
    // Default config file values as required
    naLinkOptFlag = 0;
    nsIgnoreLocal = 1;
    naRouter = 1;
    maxHops = MAXMAXHOPS;
    
    // Logging
    listLog=0;
    ralog=0;
    
    /* Interface info */
    interfaceCount = 0;
    
    /* Parse the args */
    while ((c = getopt_long(argc, argv, OPTIONS_STR, prog_opt, NULL)) > 0)
    {
        if (c==-1)
            break;
        
        switch (c) {
            case 'c':
                strcpy(configfile, optarg);
                break;
            case 'l':
                strcpy(logfile, optarg);
                break;
            case 'd':
                debug=1;
                break;
            case 'D':
                debug=2;
                break;
            case 'f':
                daemonize=0;
                break;
            case 'v':
                showVersion();
                return 0;
                break;
            case 'h':
                showUsage();
                return 0;
                break;
        }
    }
    
    /* Sort out where to log */
    if ( strlen(logfile) )
    {
        if (strlen(logfile) == 1 && (logfile[0]='-')  )
            logging = USE_STD;
        else
            logging = USE_FILE;
    }
    else
    {
        logging = USE_SYSLOG;
    }
    
    /* Open the log and config*/
    if ( (logging == USE_FILE) && (openLog(logfile) < 0)  )
    {
        printf("Exiting. Error in setting up logging correctly.\n");
        exit (1);
    }
    flog(LOG_INFO, "*********************** npd6 *****************************");
    
    if ( readConfig(configfile) )
    {
        flog(LOG_ERR, "Error in config file: %s", configfile);
        return 1;
    } 
    
    err = init_sockets();
    if (err) {
        flog(LOG_ERR, "init_sockets: failed to initialise %d sockets.", err);
        exit(1);
    }
    
    /* Set allmulti on the interfaces */
    for (loop=0; loop<interfaceCount; loop++)
    {
        interfaces[loop].multiStatus = if_allmulti(interfaces[loop].nameStr, TRUE);
    }
    
    /* Seems like about the right time to daemonize (or not) */
    if (daemonize)
    {
        if (daemon(0, 0) < 0 )
        {
            flog(LOG_ERR, "Failed to daemonize. Error: %s", strerror(errno) );
            exit(1);
        }
    }
    
    /* Set up signal handlers */
    signal(SIGUSR1, usersignal);
    signal(SIGUSR2, usersignal);
    signal(SIGHUP, usersignal);
    signal(SIGINT, usersignal);
    signal(SIGTERM, usersignal);    // Typically used by init.d scripts
    
    /* And off we go... */
    dispatcher();
    
    flog(LOG_ERR, "Fell back out of dispatcher... This is impossible.");
    return 0;
}

void dispatcher(void)
{
    struct pollfd   *fds;
    unsigned int    msglen;
    unsigned char   msgdata[MAX_MSG_SIZE * 2];
    int             rc;
    int             fdIdx;
    int             consecutivePollErrors = 0;
    
    // Each interface has 2 sockets, so we need to allocate for that + 1
    fds = (struct pollfd *)calloc( (interfaceCount*2)+1, sizeof(struct pollfd) );
    flog(LOG_DEBUG2, "Dynamically allocated %d bytes to the master FD array", 
         (interfaceCount+1) * sizeof(struct pollfd) );
    
    // In the fds set, the first N positions are for the v6 sockets, the second N
    // are for the icmpv6 sockets.
    for(fdIdx=0; fdIdx < interfaceCount; fdIdx++)
    {
        // Packet socket
        fds[fdIdx].fd = interfaces[fdIdx].pktSock;
        fds[fdIdx].events = POLLIN;
        fds[fdIdx].revents = 0;
        
        // ICMP socket
        // We only bother with this as we get inbound junk on this socket 
        // (including RAs which we might actually care about now cf. bug 60)
        fds[fdIdx+interfaceCount].fd = interfaces[fdIdx].icmpSock;
        fds[fdIdx+interfaceCount].events = POLLIN;
        fds[fdIdx+interfaceCount].revents = 0;
    }
    
    // Tail it
    fds[interfaceCount*2].fd = -1;
    fds[interfaceCount*2].events = 0;
    fds[interfaceCount*2].revents = 0;
    
    for (;;)
    {
        rc = poll(fds, interfaceCount+1, DISPATCH_TIMEOUT);
        //flog(LOG_DEBUG2, "Came off poll with rc = %d", rc);
        
        if (rc > 0)
        {
            // Most likely event is a valid data item received.
            for (fdIdx=0; fdIdx < (interfaceCount*2); fdIdx++)
            {
                if (fds[fdIdx].revents & POLLIN)
                {
                    // Was it a packet socket?
                    if(fdIdx < interfaceCount) {
                        consecutivePollErrors = 0;	// reset it
                        msglen = get_rx(interfaces[fdIdx].pktSock, msgdata);
                        // msglen is checked for sanity already within get_rx()
                        flog(LOG_DEBUG2, "For packet socket, get_rx() gave msg with len = %d", msglen);
                        processNS(fdIdx, msgdata, msglen);
                        continue;
                    }
                    // Or was it an ICMP socket?
                    if(fdIdx >= interfaceCount) {
                        struct in6_addr icmp6Addr;
                        consecutivePollErrors = 0;	// reset it
                        msglen = get_rx_icmp6(interfaces[fdIdx/2].icmpSock, msgdata, &icmp6Addr);
                        flog(LOG_DEBUG2, "For ICMP6 socket, get_rx_icmp6() gave msg with len = %d", msglen);
                        // We do nothing at all with the received data!
                        // Or maybe we do.... Ref. bug/NFR 60: process them
                        // and yank out the RAs for logging.
                        // Decide what to do based upon config file option ralog
                        if (ralog) 
                        {
                            processICMP(fdIdx, msgdata, msglen, &icmp6Addr);
                        }
                        continue;
                    }
                }
                
                // If it wasn't a POLLIN, it is likely an significant error
                if (fds[fdIdx].revents & (POLLERR | POLLHUP | POLLNVAL) )
                {
                    flog(LOG_WARNING, "Major socket error on fds %d", fdIdx);
                    // Try and recover... long shot
                    close(interfaces[fdIdx].pktSock);
                    sleep(1);
                    interfaces[fdIdx].pktSock = open_packet_socket(interfaces[fdIdx].index);
                    if ( interfaces[fdIdx].pktSock < 0)
                    {
                        // Drop dead. We're stuffed.
                        flog(LOG_ERR, "dispatcher(): failed to reinit stuck socket. Dead.");
                        exit(1);
                    }
                    fds[fdIdx].fd = interfaces[fdIdx].pktSock;
                    fds[fdIdx].events = POLLIN;
                    fds[fdIdx].revents = 0;
                    
                    // Have we had more consecutive errors than the threshold value?
                    consecutivePollErrors++;
                    if ((pollErrorLimit > 0) && (consecutivePollErrors >= pollErrorLimit) )
                    {
                        flog(LOG_ERR, "dispatcher(): %d consecutive major poll errors. Terminating.",
                             consecutivePollErrors);
                        dropdead();
                    }
                    
                    continue;
                }
            }
            continue;
        }
        else if ( rc == 0 )
        {
            flog(LOG_DEBUG, "Stale select - Idling....... Low activity........");
            consecutivePollErrors = 0; // Using the select timeout as our quantum of error counting
            // Timer fired?
            // One day. If we implement timers.
        }
        else if ( rc == -1 )
        {
            /* Truly an error or maybe we processed a signal?*/
            if ( errno == EINTR )
            {
                flog(LOG_ERR, "Broke out of the poll() via a signal event.");
            }
            else
            {
                flog(LOG_ERR, "Weird poll error: %s", strerror(errno));
            }
            continue;
        }
        
        flog(LOG_DEBUG, "Timed out of poll(). Timeout was %d ms", DISPATCH_TIMEOUT);
    }
}


//*******************************************************
// Give advice.
void showUsage(void)
{
    fprintf(stderr, "usage: %s %s\n", pname, usage_str);
}
