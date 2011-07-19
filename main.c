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

#ifdef HAVE_GETOPT_LONG
char usage_str[] =
{
"\n"
"  -c, --config=PATH      Sets the config file.  Default is /etc/npd6.conf.\n"
"  -d, --debug            Sets the normal debug level.\n"
"  -d2, --debug2          Sets full debug level. (Lots!)\n"
"  -h, --help             Show this help screen.\n"
"  -l, --logfile=PATH     Sets the log file, else default to syslog.\n"
"  -v, --version          Print the version and quit.\n"
};

struct option prog_opt[] =
{
    {"config",  1, 0, 'c'},
    {"debug",   optional_argument, &debug, 1},
    {"debug2",  optional_argument, &debug, 2},
    {"help",    optional_argument, 0, 'h'},
    {"logfile", 1, 0, 'l'},
    {"version", optional_argument, 0, 'v'},
    {NULL, 0, 0, 0}
};
#else
char usage_str[] =
    "[-hv] [-d|-D] [-c config_file] [-l log_file]\n";
#endif
#define OPTIONS_STR "c:l:vhdD"

struct Interface *IfaceList = NULL;

int main(int argc, char *argv[])
{
#ifdef HAVE_GETOPT_LONG
    int opt_idx;
#endif
    char *logfile;
    int c;

    // Default some globals
    //debug = 0;
    logfile = NPD6_LOG;
    configfile = NPD6_CONF;
    strncpy( interfacestr, NULLSTR, sizeof(NULLSTR));
    strncpy( prefixaddrstr, NULLSTR, sizeof(NULLSTR));
    interfaceIdx=-1;

    // Default config file values as required
    naLinkOptFlag = 0;
    nsIgnoreLocal = 1;
    naRouter = 1;
    maxHops = MAXMAXHOPS;

    pname = ((pname=strrchr(argv[0],'/')) != NULL)?pname+1:argv[0];
    paramName = ((paramName=strrchr(argv[0],'/')) != NULL)?paramName+1:argv[0];

    /* Parse the args */
#ifdef HAVE_GETOPT_LONG
    while ((c = getopt_long(argc, argv, OPTIONS_STR, prog_opt, &opt_idx)) > 0)
#else
    while ((c = getopt(argc, argv, OPTIONS_STR)) > 0)
#endif
    {
        switch (c) {
        case 'c':
            configfile = optarg;
            break;
        case 'l':
            logfile = optarg;
            break;
        case 'd':
            debug=1;
            break;
        case 'D':
            debug=2;
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

    /* Open the log and config*/
    if (openLog(logfile) < 0)
    {
        printf("Exiting. Cannot open log file.");
        exit (1);
    }
    flog(LOG_INFO, "*********************** npd6 *****************************");
    
    if ( readConfig(configfile) )
    {
        flog(LOG_ERR, "Error in config file: %s", configfile);
        return 1;
    }
    
    flog(LOG_INFO, "Using normalised prefix %s/%d", prefixaddrstr, prefixaddrlen);
    flog(LOG_DEBUG2, "ifIndex for %s is: %d", interfacestr, interfaceIdx);

    /* Raw socket for receiving NSs */
    sockpkt = open_packet_socket();
    if (sockpkt < 0)
    {
        flog(LOG_ERR, "open_packet_socket: failed. Exiting.");
        exit(1);
    }
    flog(LOG_DEBUG, "open_packet_socket: OK.");

    /* ICMPv6 socket for sending NAs */
    sockicmp = open_icmpv6_socket();
    if (sockicmp < 0)
    {
        flog(LOG_ERR, "open_icmpv6_socket: failed. Exiting.");
        exit(1);
    }
    flog(LOG_DEBUG, "open_icmpv6_socket: OK.");

    /* Set allmulti on the interface */
    if_allmulti(interfacestr, TRUE);

    /* Set up signal handlers */
    signal(SIGUSR1, usersignal);
    signal(SIGUSR2, usersignal);
    signal(SIGHUP, usersignal);


    /* And off we go... */
    dispatcher();

    flog(LOG_ERR, "Fell back out of dispatcher... This is impossible.");
    return 0;
}

void dispatcher(void)
{
    struct pollfd   fds[2];
    unsigned int    msglen;
    unsigned char   msgdata[4096];
    int             rc;

    memset(fds, 0, sizeof(fds));
    fds[0].fd = sockpkt;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = -1;
    fds[1].events = 0;
    fds[1].revents = 0;

    for (;;)
    {
        rc = poll(fds, sizeof(fds)/sizeof(fds[0]), DISPATCH_TIMEOUT);

        if (rc > 0) {
            if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                flog(LOG_WARNING, "Socket error on fds[0].fd");
                continue;
            }
            else if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                flog(LOG_WARNING, "Socket error on fds[1].fd");
                continue;
            }
            else if (fds[0].revents & POLLIN)
            {
                msglen = get_rx(msgdata);
                flog(LOG_DEBUG2, "get_rx() gave msg with len = %d", msglen);

                // Have processNS() do the rest of validation and work...
                processNS(msgdata, msglen);
                continue;
            }
            else if ( rc == 0 )
            {
                flog(LOG_DEBUG, "Timer event");
                // Timer fired?
                // One day. If we implement timers.
            }
            else if ( rc == -1 )
            {
                flog(LOG_ERR, "Weird poll error: %s", strerror(errno));
                continue;
            }
            // Pick up and sigusr signals
            else if (sigusr1_received)
            {
                flog(LOG_DEBUG2, "sigusr1 seen in main dispatcher");
                sigusr1_received = 0;
                continue;
            }
            else if (sigusr2_received)
            {
                flog(LOG_DEBUG2, "sigusr2 seen in main dispatcher");
                sigusr2_received = 0;
                continue;
            }

            flog(LOG_DEBUG, "Timed out of poll(). Timeout was %d ms", DISPATCH_TIMEOUT);
        }
    }
}


//*******************************************************
// Give advice.
void showUsage(void)
{
    fprintf(stderr, "usage: %s %s\n", pname, usage_str);
}
