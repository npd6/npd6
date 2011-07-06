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
 *  None yet... placeholder
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
            flog(LOG_DEBUG, "usersignal called with USR1");
            sigusr1_received = 1;
            break;
         case SIGUSR2:
            signal(SIGUSR2, usersignal);
            flog(LOG_DEBUG, "usersignal called with USR2");
            sigusr2_received = 1;
            break;
         default:
            flog(LOG_DEBUG, "usersignal(): Why am I here?");
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
 */
int npd6log(const char *function, int pri, char *format, ...)
{
    time_t now;
    struct tm *timenow;
    char timestamp[128], obuff[2048];
    va_list param;

    // Pick up debug requests and decide if we are logging them
    if (!debug && pri<LOG_DEBUG)
        return 0;  // Silent...

    // Check for extra super power debug
    if ( (debug!=2) && (pri == LOG_DEBUG2) )
        return 0;   // Silent...

    // Normalise the debug level
    if( pri==LOG_DEBUG2)
        pri=LOG_DEBUG;  // Since only we understand the two levels
    
    va_start(param, format);
    vsnprintf(obuff, sizeof(obuff), format, param);
    now = time(NULL);
    timenow = localtime(&now);
    (void) strftime(timestamp, sizeof(timestamp), LOGTIMEFORMAT, timenow);

    fprintf(logFileFD, "[%s] %s: %s\n", timestamp, function, obuff);
    fflush(logFileFD);
    va_end(param);

    return 0;
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
 *      void
 */
void build_addr(char *str, struct in6_addr *addr)
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
 */
int prefixset(char *px)
{
    size_t len = strlen(px);

    switch (len) {
        case 5:
            strcat(px, "0000:0000:0000:0000:0000:0000:0000");
            return 8;
        case 10:
            strcat(px, "0000:0000:0000:0000:0000:0000");
            return 16;
        case 15:
            strcat(px, "0000:0000:0000:0000:0000");
            return 24;
        case 20:
            strcat(px, "0000:0000:0000:0000");
            return 32;
        case 25:
            strcat(px, "0000:0000:0000");
            return 40;
        case 30:
            strcat(px, "0000:0000");
            return 48;
        case 35:
            strcat(px, "0000");
            return 56;
        case 39:
            flog(LOG_ERR, "Full 64-bits defined as the configured prefix. Sure???");
            return 64;
        default:
            flog(LOG_ERR, "configured prefix not correctly formatted (len = %d)", len);
            return -1;
    }
}


/*****************************************************************************
 * trimwhitespace
 *  Tidy up lines of text from the config file.
 *
 * Inputs:
 *  char * str
 *      String to be tidied.
 *
 * Outputs:
 *  As input
 *
 * Return:
 *  Pointer to the string (redundant)
 */
char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
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

    if ((logFileFD = fopen(logFileName, "a")) == NULL)
    {
        fprintf(stderr, "Can't open %s: %s\n", logFileName, strerror(errno));
        return (-1);
    }

    return 0;
}


//*******************************************************
// Just display the version and return.
void showVersion(void)
{
    printf("Version 0.1\n");
}

