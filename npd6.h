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





// struct Interface {
// 	char    name[32];
// 
// 	struct in6_addr	if_addr;
// 	unsigned int	if_index;
// 
// 	uint8_t		init_racount;	/* Initial RAs */
// 
// 	uint8_t		if_hwaddr[HWADDR_MAX];
// 	int			if_hwaddr_len;
// 	int			if_prefix_len;
// 	int			if_maxmtu;
// 
// 	int			cease_adv;
// 
// 	struct Interface	*next;
// };

#endif

