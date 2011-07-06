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

#ifndef INCLUDES_H
#define INCLUDES_H

/* whether compiling on Linux, glibc>=2.8 doesn't expose in6_pktinfo
   otherwise.. */
#define _GNU_SOURCE /**/


#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <pwd.h>
#include <grp.h>

#include <sys/types.h>
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#else
# ifdef HAVE_MACHINE_PARAM_H
#  include <machine/param.h>
# endif
# ifdef HAVE_MACHINE_LIMITS_H
#  include <machine/limits.h>
# endif
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <netinet/in.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#include <sys/sysctl.h>

#include <net/if.h>

#include <getopt.h>

#include <ifaddrs.h>

#include <poll.h>

#include <linux/netlink.h>

#include <netinet/in.h>

#include <ctype.h>

#include <linux/if_ether.h>
#include <linux/filter.h>
#include <stddef.h>


#endif /* INCLUDES_H */

