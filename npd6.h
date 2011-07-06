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

#ifndef NPD6_H
#define NPD6_H

#include "includes.h"

#define min(a,b)    (((a) < (b)) ? (a) : (b))

#ifndef NPD6_CONF
#   define NPD6_CONF "/etc/npd6.conf"
#endif

#ifndef NPD6_LOG
#   define NPD6_LOG "/var/log/npd6.log"
#endif

#ifndef NULL
#   define NULL 0
#endif

#ifndef HAVE_GETOPT_LONG
#   define HAVE_GETOPT_LONG 1
#endif

#define LOGTIMEFORMAT "%b %d %H:%M:%S"

#define INTERFACE_STRLEN    12

#define NULLSTR "null"

//extern int sock;
// Globals
char *pname;
char *configfile;
char *paramName;
int sockicmp;
int sockpkt;
int debug;
FILE *logFileFD;
FILE *configFileFD;
volatile int sigusr1_received;
volatile int sigusr2_received;

// Interface we're interested in
char interfacestr[INTERFACE_STRLEN];
unsigned int interfaceIdx;
unsigned char linkAddr[6];

// Recording the target prefix
char prefixaddrstr[INET6_ADDRSTRLEN];
struct in6_addr prefixaddr;
unsigned int prefixaddrlen;

//
// PROTOTYPES
//
// main.c
void showVersion(void);
void showUsage(void);
int readConfig(char *);
int openLog(char *);
void dispatcher(void);
// util.c
int flog(int pri, char *format, ...);
void usersignal(int signal);
void print_addr(struct in6_addr *addr, char *str);
void print_addr16(const struct in6_addr * addr, char * str);
void build_addr(char *, struct in6_addr *);
int prefixset(char *);
char *trimwhitespace(char *);
void dumpHex(unsigned char *, unsigned int);
int getLinkaddress( char *, unsigned char *);
// icmp6.c
int open_packet_socket(void);
int open_icmpv6_socket(void);
int get_rx(unsigned char *, struct sockaddr_in6 *, struct in6_pktinfo **);
//netlink.c
void process_netlink_msg(int sock);
int netlink_socket(void);
// ip6,c
void processNS( unsigned char *, unsigned int);
int addr6match( struct in6_addr *, struct in6_addr *, int);

//
// Various limits and important values
//
#define HWADDR_MAX 16
#define MAX_PKT_BUFF    1500

#endif

