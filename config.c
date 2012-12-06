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
#include "npd6config.h"

#include "expintf.h"

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
    unsigned int    prefixCount=0; // Temp count of prefixes, to match interfaces
    // Used if building white/blacklist
    struct  in6_addr    listEntry;
    unsigned int check;
    // For prefix manipulation:
    char            prefixaddrstr[INET6_ADDRSTRLEN];
    struct          in6_addr prefixaddr;
    int             prefixaddrlen, masklen=0;
    char            interfacestr[INTERFACE_STRLEN];
    char            *slashMarker;
    int             approxInterfaces = 0;

    
    // Ensure global set correctly
    interfaceCount = 0;
    pollErrorLimit = 10;    // Vaguely sensible default

    if ((configFileFD = fopen(configFileName, "r")) == NULL)
    {
        fprintf(stderr, "Can't open %s: %s\n", configFileName, strerror(errno));
        flog(LOG_ERR, "Can't open config file %s: %s", configFileName, strerror(errno));
        return (-1);
    }
    // First of we need to whip through and count the number of "interface" strings to
    // give us a worst-case/maximum size for the master data structures.
    do {
        if (fgets(linein, 128, configFileFD) == NULL)
            break;
        
        // Quick check for the presence of the string "interface"
        if ( strstr( linein, configStrs[NPD6INTERFACE]) )
            approxInterfaces++;
    } while (1);
    flog(LOG_DEBUG2, "Sizing master interface data structure for %d interfaces.",
         approxInterfaces);
    
    // Go back to the start of the file 
    rewind(configFileFD);
    // This is real simple config file parsing...
    do {
        int strToken, strIdx;

        len = 0;
        if (fgets(linein, 128, configFileFD) == NULL)
            break;
        // Tidy it up
            stripwhitespace(linein);
            // Special mega-hacky thing for blank lines:
            len = strlen(linein);
            if (len==0)
            {
                len=1;
                continue;
            }

            // Tokenize
            cp = strdupa(linein);
            lefttoken = strtok(cp, delimiters);
            righttoken = strtok(NULL, delimiters);
            if ( (lefttoken == NULL) || (righttoken == NULL) )
            {
                continue;
            }

            // Match token
            strIdx = -1;
            for(strToken = 0; strToken < CONFIGTOTAL; strToken++)
            {
                if( !strcmp(lefttoken, configStrs[strToken]) )
                {
                    strIdx = strToken;
                    // Matched, so drop to next step
                    break;
                }
            }
            flog(LOG_DEBUG2, "Matched config item index: %d", strIdx);

            // If config params are being added, it should only be required
            // to update the strings in npd6config.h and then insert a
            // case XXXXXXX: here with self-contined code inside.
            switch (strIdx) {
                case NOMATCH:
                    flog(LOG_DEBUG2, "Found noise in config file. Skipping.");
                    continue;

                case NPD6PREFIX:
                    strncpy( prefixaddrstr, righttoken, sizeof(prefixaddrstr));
                    flog(LOG_DEBUG, "Raw prefix: %s", prefixaddrstr);
                    // The prefix may be optionally specified with a mask.
                    // e.g. 1:2:3:: or 1:2:3::/12
                    slashMarker = strchr( prefixaddrstr, '/');
                    if (slashMarker != NULL)
                    {
                        // We found a mask marker
                        masklen = atoi(slashMarker + 1);
                        // Reterminate prefix
                        flog(LOG_DEBUG2, "Pre: %s", prefixaddrstr);
                        slashMarker[0] = '\0';
                        flog(LOG_DEBUG2, "Post: %s", prefixaddrstr);
                    }

                    // We need to pad it up and record the length in bits
                    prefixaddrlen = prefixset(prefixaddrstr);
                    flog(LOG_INFO, "Padded prefix: %s, length = %d", prefixaddrstr, prefixaddrlen);
                    flog(LOG_INFO, "Mask specified: %d", masklen);
                    if ( prefixaddrlen <= 0 )
                    {
                        flog(LOG_ERR, "Invalid prefix.");
                        return 1;
                    }
                    // If no mask specified, assume a default value
                    if ( masklen == 0 )
                    {
                        flog(LOG_INFO, "No mask specified. Assuming mask length %d", prefixaddrlen);
                        masklen = prefixaddrlen;
                    }
                    // If specified mask length at odds with the prefix itself, flag it
                    // i.e. if the mask specified is not on a 16-bit boundary. Quite legal, but likely
                    // not common
                    if ( masklen != prefixaddrlen )
                    {
                        flog(LOG_INFO, "Mask of %d correct? Prefix looked like %d. Continuing with your value (%d)",
                                        masklen, prefixaddrlen, masklen);
                    }
                    // Build a binary image of it
                    build_addr(prefixaddrstr, &prefixaddr);
                    // Store it
                    interfaces[prefixCount].prefix = prefixaddr;
                    strncpy( interfaces[prefixCount].prefixStr, prefixaddrstr, 
                             sizeof(interfaces[prefixCount].prefixStr) );
                    interfaces[prefixCount].prefixLen = masklen;
                    prefixCount++;
                    break;

                case NPD6INTERFACE:
                    if ( strlen( righttoken) > INTERFACE_STRLEN )
                    {
                        flog(LOG_ERR, "Invalid length interface name");
                        return 1;
                    }
                    strncpy( interfacestr, righttoken, sizeof(interfacestr));
                    flog(LOG_INFO, "Supplied interface is %s", interfacestr);
                    // Store it
                    strncpy( interfaces[interfaceCount].nameStr, interfacestr, 
                             sizeof(interfaces[interfaceCount].nameStr) );
                    interfaceCount++;
                    if ( interfaceCount > MAXINTERFACES )
                    {
                        flog(LOG_ERR, "Maximum %d interfaces permitted. Error.", MAXINTERFACES);
                        return 1;
                    }
                    break;

                case NPD6OPTFLAG:
                    if ( !strcmp( righttoken, SET ) )
                    {
                        flog(LOG_INFO, "linkOption flag SET");
                        naLinkOptFlag = 1;
                    }
                    else if ( !strcmp( righttoken, UNSET ) )
                    {
                        flog(LOG_INFO, "linkOption flag UNSET");
                        naLinkOptFlag = 0;
                    }
                    else
                    {
                        flog(LOG_ERR, "linkOption flag - Bad value");
                        return 1;
                    }
                    break;

                case NPD6LOCALIG:
                    if ( !strcmp( righttoken, SET ) )
                    {
                        flog(LOG_INFO, "ignoreLocal flag SET");
                        nsIgnoreLocal = 1;
                    }
                    else if ( !strcmp( righttoken, UNSET ) )
                    {
                        flog(LOG_INFO, "ignoreLocal flag UNSET");
                        nsIgnoreLocal = 0;
                    }
                    else
                    {
                        flog(LOG_ERR, "ignoreLocal flag - Bad value");
                        return 1;
                    }
                    break;

                case NPD6ROUTERNA:
                    if ( !strcmp( righttoken, SET ) )
                    {
                        flog(LOG_INFO, "routerNA flag SET");
                        naRouter = 1;
                    }
                    else if ( !strcmp( righttoken, UNSET ) )
                    {
                        flog(LOG_INFO, "routerNA flag UNSET");
                        naRouter = 0;
                    }
                    else
                    {
                        flog(LOG_ERR, "routerNA flag - Bad value");
                        return 1;
                    }
                    break;

                case NPD6MAXHOPS:
                    maxHops = -1;
                    maxHops = atoi(righttoken);

                    if ( (maxHops < 0) || (maxHops > MAXMAXHOPS) )
                    {
                        flog(LOG_ERR, "maxHops - invalid value specified in config.");
                        return 1;
                    }
                    else
                    {
                        flog(LOG_INFO, "maxHops set to %d", maxHops);
                    }
                    break;

                case NPD6TARGETS:
                    // If we arrive here and the tRoot tree already exists,
                    // then we're re-reading the config and so need to zap
                    // the tRoot data first.
                    if (tRoot != NULL)
                    {
                        tdestroy(tRoot, free);
                        tEntries = 0;
                    }
                    collectTargets = -1;
                    tRoot = NULL;
                    collectTargets = atoi(righttoken);

                    if ( (collectTargets < 0) || (collectTargets > MAXTARGETS) )
                    {
                        flog(LOG_ERR, "collectTargets - invalid value specified in config.");
                        return 1;
                    }
                    else
                    {
                        flog(LOG_INFO, "collectTargets set to %d", collectTargets);
                    }
                    break;
                case NPD6LISTTYPE:
                    if ( !strcmp( righttoken, NPD6NONE ) )
                    {
                        flog(LOG_INFO, "List-type = NONE");
                        listType = NOLIST;
                    }
                    else if ( !strcmp( righttoken, NPD6BLACK ) )
                    {
                        flog(LOG_INFO, "List-type = BLACK");
                        listType = BLACKLIST;
                    }
                    else if( !strcmp( righttoken, NPD6WHITE ) )
                    {
                        flog(LOG_INFO, "List-type = WHITE");
                        listType = WHITELIST;
                    }
                    else
                    {
                        flog(LOG_ERR, "List-type = <invalid value> - Setting to NONE");
                        listType = NOLIST;
                    }
                    break;
                case NPD6LISTADDR:
                    if (build_addr( righttoken, &listEntry) )
                    {
                        flog(LOG_DEBUG, "Address %s valid.", righttoken);
                        storeListEntry(&listEntry);
                    }
                    else
                    {
                        flog(LOG_ERR, "Address %s invalid.", righttoken);
                    }
                    break;
                    
                case NPD6ERRORTH:
                    pollErrorLimit = -1;
                    pollErrorLimit = atoi(righttoken);

                    if ( (pollErrorLimit < 0) )
                    {
                        flog(LOG_ERR, "pollErrorLimit - invalid -ve value specified in config.");
                        return 1;
                    }
                    else
                    {
                        flog(LOG_INFO, "pollErrorLimit set to %d", pollErrorLimit);
                    }
                    break;                    
                    
                    
                case NPD6EXPRADDR:
                    flog(LOG_DEBUG, "Address expression %s added.", linein);
                    storeExpression(linein);
                    break;
                case NPD6LISTLOG:
                    if ( !strcmp( righttoken, ON ) )
                    {
                        flog(LOG_INFO, "listlogging set to ON");
                        listLog = 1;
                    }
                    else if ( !strcmp( righttoken, OFF ) )
                    {
                        flog(LOG_INFO, "listlogging set to OFF");
                        listLog = 0;
                    }
                    else
                    {
                        flog(LOG_ERR, "listlogging flag - Bad value");
                        return 1;
                    }
                    break;
            }
    } while (len);


    // Basic check: did we have the same number of interfaces as prefixes?
    if ( interfaceCount != prefixCount )
    {
        flog(LOG_ERR, "Must have same number of prefixes as interfaces. Error in config.");
        return 1;
    }
    // Did we have ANY interfaces?
    if ( interfaceCount < 1)
    {
        flog(LOG_ERR, "Must define at least one interface/prefix pair.");
        return 1;
    }

    flog(LOG_DEBUG, "Total interfaces defined: %d", interfaceCount);

    // Work out the interface indices and link addrs
    for (check = 0; check < interfaceCount; check ++)
    {
        unsigned int    interfaceIdx;
        //unsigned char   linkAddr[6];
        
        // Interface index number
        interfaceIdx = if_nametoindex( interfaces[check].nameStr );
        if ( !interfaceIdx )
        {
            flog(LOG_ERR, "Could not get ifIndex for interface %s",
                 interfaces[check].nameStr);
            return 1;
        }
        interfaces[check].index = interfaceIdx;
        flog(LOG_DEBUG2, "i/f name = %s, i/f index = %d",
                    interfaces[check].nameStr,
                    interfaces[check].index);
        
        // Interface's link address
        if (getLinkaddress( interfaces[check].nameStr, interfaces[check].linkAddr) )
        {
            flog(LOG_ERR, "Failed to match interface %s to a lnik-level address.", 
                 interfaces[check].nameStr );
            return 1;
        }
    }
    
    return 0;
}
