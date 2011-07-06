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
        if ( (lefttoken == NULL) || (righttoken == NULL) )
        {
            continue;
        }

        /**********************************
        * NPDprefix
        **********************************/
        if ( strcmp(lefttoken, "NPDprefix")==0 ) {
            strncpy( prefixaddrstr, righttoken, sizeof(prefixaddrstr));
            // We need to pad it up and record the length in bits
            prefixaddrlen = prefixset(prefixaddrstr);
            flog(LOG_DEBUG, "Supplied prefix: %s, significant length = %d", prefixaddrstr, prefixaddrlen);
            // Build a binary image of it
            build_addr(prefixaddrstr, &prefixaddr);
        }
        /**********************************
        * NPDinterface
        **********************************/ 
        else if ( strcmp(lefttoken, "NPDinterface")==0 )
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
