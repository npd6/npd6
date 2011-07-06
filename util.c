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

//*******************************************************
// Log to the previously opened log file.
int flog(int pri, char *format, ...)
{
    time_t now;
    struct tm *timenow;
    char timestamp[128], obuff[2048];
    va_list param;

    // Pick up debug requests and decide if we are logging them
    if (!debug && pri==LOG_DEBUG)
        return 0;  // Silent...

    va_start(param, format);

    vsnprintf(obuff, sizeof(obuff), format, param);
    
    now = time(NULL);
    timenow = localtime(&now);
    (void) strftime(timestamp, sizeof(timestamp), LOGTIMEFORMAT, timenow);

    fprintf(logFileFD, "[%s] %s\n", timestamp, obuff);
    fflush(logFileFD);

    va_end(param);

    return 0;
}

//*******************************************************
// Print ipv6 in (optionally) avbbreviated char form
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


//*******************************************************
// Take ipv6 addr in char form and build a binary version
void build_addr(char *str, struct in6_addr *addr)
{
    int ret;
    flog(LOG_DEBUG, "build_addr: called with address %s", str);
    ret = inet_pton(AF_INET6, str, (void *)addr);
    if (ret == 1)
        flog(LOG_DEBUG, "build_addr: inet_pton OK");
    else if(ret == 0)
        flog(LOG_ERR, "build_addr: invalid input address");
    else
        flog(LOG_ERR, "build_addr: inet_pton: %s", strerror(errno));
}

/*
 * print_addr16
 *      Print ipv6 address in fully expanded 64-bit char form
 *
 * Inputs:
 * PARAMS
 *      const struct in6_addr * addr
 *          Binary ipv6 address
 *
 * Outputs:
 * PARAMS
 *      char * str
 *          String representation, fully padded.
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



//***********************************************************
// Take prefix in the form 1111:2222:3333:4444: and pad it to the ful length
// Return value is the bit length of the unpadded prefix
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
            flog(LOG_ERR, "prefixset: Full 64-bits defined as the configured prefix. Sure???");
            return 64;
        default:
            flog(LOG_ERR, "prefixset: configured prefix not correctly formatted (len = %d)", len);
            return -1;
    }
}


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


void dumpHex(unsigned char *data, unsigned int len)
{
    int ix;

    printf("Dumping %d bytes of hex:\n>>>>>", len);
    
    for(ix=0; ix < len; ix++)
        printf("%02x", data[ix]);
    printf("<<<<<\n");


}



int getLinkaddress( char * iface, unsigned char * link) {
    int sockfd, io;
    struct ifreq ifr;

    strncpy( ifr.ifr_name, iface, INTERFACE_STRLEN );

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        flog(LOG_ERR, "getLinkaddress: failed to open a test SOCK_STREAM.");
        return 1;
    }

    io = ioctl(sockfd, SIOCGIFHWADDR, (char *)&ifr);
    if(io < 0) {
        flog(LOG_ERR, "getLinkaddress ioctl failed to obtain link address");
        return 1;
    }

    memcpy(link, (unsigned char *)ifr.ifr_ifru.ifru_hwaddr.sa_data, 6);

    close(sockfd);

    return 0;
}


