#include "includes.h"
#include "npd6.h"

#ifdef HAVE_GETOPT_LONG
char usage_str[] = {
"\n"
"  -c, --config=PATH      Sets the config file.  Default is /etc/npd6.conf.\n"
"  -d, --debug            Sets the debug level.\n"
"  -h, --help             Show this help screen.\n"
"  -l, --logfile=PATH     Sets the log file, else default to syslog.\n"
"  -v, --version          Print the version and quit.\n"
};

struct option prog_opt[] = {
    {"config", 1, 0, 'c'},
    {"debug", 1, 0, 'd'},
    {"help", 0, 0, 'h'},
    {"logfile", 1, 0, 'l'},
    {"version", 0, 0, 'v'},
    {NULL, 0, 0, 0}
};
#else
char usage_str[] =
    "[-hvd] [-c config_file] [-l log_file]\n";
#endif
#define OPTIONS_STR "c:l:vhd"

struct Interface *IfaceList = NULL;

int main(int argc, char *argv[])
{
#ifdef HAVE_GETOPT_LONG
    int opt_idx;
#endif
    char *logfile;
    int c;

    // Default some globals
    debug = 0;
    logfile = NPD6_LOG;
    configfile = NPD6_CONF;
    strncpy( interfacestr, NULLSTR, sizeof(NULLSTR));
    interfaceIdx=-1;
    strncpy( prefixaddrstr, NULLSTR, sizeof(NULLSTR));


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
            debug = 1;
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
    if (openLog(logfile) < 0) {
        printf("Exiting. Cannot open log file.");
        exit (1);
    }
    flog(LOG_INFO, "Opened log file OK.");
    
    if ( readConfig(configfile) ) {
        flog(LOG_ERR, "Error in config file: %s", configfile);
        return 1;
    }
    
    flog(LOG_DEBUG, "From config, normalised prefix is %s/%d", prefixaddrstr, prefixaddrlen);
    flog(LOG_DEBUG, "From config, ifIndex for %s is: %d", interfacestr, interfaceIdx);

    /* Raw socket for receiving NSs */
    sockpkt = open_packet_socket();
    if (sockpkt < 0) {
        flog(LOG_ERR, "open_packet_socket: failed. Exiting.");
        exit(1);
    }
    flog(LOG_DEBUG, "open_packet_socket: OK.");

    /* ICMPv6 socket for sending NAs */
    sockicmp = open_icmpv6_socket();
    if (sockicmp < 0) {
        flog(LOG_ERR, "open_icmpv6_socket: failed. Exiting.");
        exit(1);
    }
    flog(LOG_DEBUG, "open_icmpv6_socket: OK.");


    /* Set up signal handlers */
    //sigemptyset(&sigact.sa_mask);
    signal(SIGUSR1, usersignal);
    signal(SIGUSR2, usersignal);
    signal(SIGHUP, usersignal);


    /* And off we go... */
    dispatcher();

    flog(LOG_ERR, "main: Fell back out of dispatcher... This is impossible.");
    return 0;
}

void dispatcher(void)
{
    struct pollfd fds[2];

    flog(LOG_DEBUG, "dispatcher: Start");

    memset(fds, 0, sizeof(fds));
    fds[0].fd = sockpkt;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = -1;
    fds[1].events = 0;
    fds[1].revents = 0;

    for (;;) {
        int timeout = 180000;
        int rc;

        rc = poll(fds, sizeof(fds)/sizeof(fds[0]), timeout);

        if (rc > 0) {
            if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                flog(LOG_WARNING, "socket error on fds[0].fd");
            }
            if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                flog(LOG_WARNING, "socket error on fds[1].fd");
            }
            else if (fds[0].revents & POLLIN) {
                // Main event handling is in here
                unsigned int len;
                struct sockaddr_in6 rcv_addr;
                struct in6_pktinfo *pkt_info = NULL;
                unsigned char msg[4096];                
              
                // Get the message
                len = get_rx(msg, &rcv_addr, &pkt_info);
                flog(LOG_DEBUG, "get_rx() gave msg with len = %d", len);

                // Have processNS() do the rest of validation and work...
                processNS(msg, len);
            }
            else if ( rc == 0 ) {
                // Timer fired?
                // One day. If we implement timers.
            }
            else if ( rc == -1 ) {
                flog(LOG_ERR, "dispatcher: weird poll error: %s", strerror(errno));
            }

            // Pick up and sigusrN signals
            if (sigusr1_received)
            {
                flog(LOG_DEBUG, "sigusr1 seen in main dispatcher");
                sigusr1_received = 0;
            }
            if (sigusr2_received)
            {
                flog(LOG_DEBUG, "sigusr2 seen in main dispatcher");
                sigusr2_received = 0;
            }

        }
    }
}

//*******************************************************
// Take the supplied filename and open it for logging use.
// Upon return, logFileFD set unless we failed.
int openLog(char *logFileName)
{   
    
    if ((logFileFD = fopen(logFileName, "a")) == NULL)
    {
        fprintf(stderr, "Can't open %s: %s\n", logFileName, strerror(errno));
        return (-1);
    }

    return 0;
}

//*******************************************************
// Take supplied filename and open it, then parse the contents.
// Upon return following set:
//
// OR we failed -1.
int readConfig(char *configFileName)
{
    char linein[256];
    int len;
    const char delimiters[] = "=";
    char *lefttoken, *righttoken;
    char *cp;
    
    if ((configFileFD = fopen(configFileName, "r")) == NULL)
    {
        fprintf(stderr, "Can't open %s: %s\n", configFileName, strerror(errno));
        return (-1);
    }
    // This is real dumb, noddy, u8nvalidated config parsing. We find a line begins with EITHER '//' OR
    // NPD,,,,
    // We act only on NPD... All else gets simply ignored.
    do {
        len = 0;
        if (fgets(linein, 128, configFileFD) == NULL)
            break;
        // Tidy it up
        trimwhitespace(linein);
        len = strlen(linein);
        
        // Tokenize
        cp = strdupa(linein);
        lefttoken = strtok(cp, delimiters);
        righttoken = strtok(NULL, delimiters);
        if ( (lefttoken == NULL) || (righttoken == NULL) ) {
            continue;
        }
        
        if ( strcmp(lefttoken, "NPDprefix")==0 ) {
            strncpy( prefixaddrstr, righttoken, sizeof(prefixaddrstr));
            flog(LOG_DEBUG, "readConfig: supplied prefix is %s", prefixaddrstr);
            // We need to pad it up and record the length in bits
            prefixaddrlen = prefixset(prefixaddrstr);
            // Build a binary image of it
            build_addr(prefixaddrstr, &prefixaddr);
        }
        else if ( strcmp(lefttoken, "NPDinterface")==0 ) {
            if ( strlen( righttoken) > INTERFACE_STRLEN)
            {
                flog(LOG_ERR, "readConfig: invalid length interface name - Exiting");
                exit(1);
            }
            strncpy( interfacestr, righttoken, sizeof(prefixaddrstr));
            flog(LOG_DEBUG, "readConfig: supplied interface is %s", interfacestr);
        }
        else {
            flog(LOG_DEBUG, "Found noise in config file. Skipping.");
        }
    }while (len);


    // Now do a check to ensure all required params were actually given
    if ( ! strcmp(prefixaddrstr, NULLSTR) )
    {
        flog(LOG_ERR, "readConfig: prefix not defined in config file.");
        return 1;
    }
    if ( ! strcmp(interfacestr, NULLSTR) )
    {
        flog(LOG_ERR, "readConfig: interface not defined in config file.");
        return 1;
    }

    // Work out the interface index
    interfaceIdx = if_nametoindex(interfacestr);
    if (!interfaceIdx)
    {
        flog(LOG_ERR, "Could not get ifIndex for interface %s", interfacestr);
        return 1;
    }


    if (getLinkaddress( interfacestr, linkAddr) )
    {
        flog(LOG_ERR, "readconfig: failed to convert interface specified to a link-level address.");
        return 1;
    }


    return 0;
}

//*******************************************************
// Just display the version and return.
void showVersion(void)
{
    printf("Version 0.1\n");
}

//*******************************************************
// Give advice.
void showUsage(void)
{
    fprintf(stderr, "usage: %s %s\n", pname, usage_str);
}
