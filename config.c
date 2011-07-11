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
#include "npd6config.h"
//## Test comment2

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
        flog(LOG_ERR, "Can't open config file %s: %s", configFileName, strerror(errno));
        return (-1);
    }
    
    // This is real simple config file parsing...
    do {
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

        /**********************************
        * prefix
        **********************************/
        if ( strcmp(lefttoken, NPD6PREFIX)==0 ) {
            strncpy( prefixaddrstr, righttoken, sizeof(prefixaddrstr));
            // We need to pad it up and record the length in bits
            prefixaddrlen = prefixset(prefixaddrstr);
            flog(LOG_DEBUG, "Supplied prefix: %s, significant length = %d", prefixaddrstr, prefixaddrlen);
            // Build a binary image of it
            build_addr(prefixaddrstr, &prefixaddr);
        }
        /**********************************
        * interface
        **********************************/ 
        else if ( strcmp(lefttoken, NPD6INTERFACE)==0 )
        {
            if ( strlen( righttoken) > INTERFACE_STRLEN)
            {
                flog(LOG_ERR, "Invalid length interface name - Exiting");
                exit(1);
            }
            strncpy( interfacestr, righttoken, sizeof(interfacestr));
            flog(LOG_DEBUG, "Supplied interface is %s", interfacestr);
        }
        /**********************************
        * Link option flag
        **********************************/
        else if ( strcmp(lefttoken, NPD6OPTFLAG)==0 )
        {
            if ( !strcmp( righttoken, SET ) )
            {
                flog(LOG_DEBUG, "linkOption flag SET");
                naLinkOptFlag = 1;
            }
            else if ( !strcmp( righttoken, UNSET ) )
            {
                flog(LOG_DEBUG, "linkOption flag UNSET");
                naLinkOptFlag = 0;
            }
            else
            {
                flog(LOG_ERR, "linkOption flag - Bad value");
                exit(1);
            }
        }
        /**********************************
        * Ignore local NS flag
        **********************************/
        else if ( strcmp(lefttoken, NPD6LOCALIG)==0 )
        {
            if ( !strcmp( righttoken, SET ) )
            {
                flog(LOG_DEBUG, "ignoreLocal flag SET");
                nsIgnoreLocal = 1;
            }
            else if ( !strcmp( righttoken, UNSET ) )
            {
                flog(LOG_DEBUG, "ignoreLocal flag UNSET");
                nsIgnoreLocal = 0;
            }
            else
            {
                flog(LOG_ERR, "ignoreLocal flag - Bad value");
                exit(1);
            }
        }
        /**********************************
        * NA ROUTER flag
        **********************************/
        else if ( strcmp(lefttoken, NPD6ROUTERNA)==0 )
        {
            if ( !strcmp( righttoken, SET ) )
            {
                flog(LOG_DEBUG, "routerNA flag SET");
                naRouter = 1;
            }
            else if ( !strcmp( righttoken, UNSET ) )
            {
                flog(LOG_DEBUG, "routerNA flag UNSET");
                naRouter = 0;
            }
            else
            {
                flog(LOG_ERR, "routerNA flag - Bad value");
                exit(1);
            }
        }
        /**********************************
        * ICMP6 max hops
        **********************************/
        else if ( strcmp(lefttoken, NPD6MAXHOPS)==0 )
        {
            maxHops = -1;
            maxHops = atoi(righttoken);
            
            if ( (maxHops < 0) || (maxHops > MAXMAXHOPS) )
            {
                flog(LOG_ERR, "maxHops - invalid value specified in config.");
                exit(1);
            }
            else
            {
                flog(LOG_DEBUG, "maxHops set to %d", maxHops);
            }
        }        
        /**********************************
        * junk
        **********************************/
        else
        {
            flog(LOG_DEBUG2, "Found noise in config file. Skipping.");
        }
    } while (len);


    // Now do a check to ensure all required params were actually given
    if ( ! strcmp(prefixaddrstr, NULLSTR) )
    {
        flog(LOG_ERR, "Prefix not defined in config file.");
        return 1;
    }
    if ( ! strcmp(interfacestr, NULLSTR) )
    {
        flog(LOG_ERR, "interface not defined in config file.");
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
        flog(LOG_ERR, "failed to convert interface specified to a link-level address.");
        return 1;
    }


    return 0;
}
