#include <npd6.h>

//*****************************************************************************
// Globals
//
char            *pname;
char            *paramName;
int             sockpkt;
int             debug;
int             daemonize;
FILE            *logFileFD;
int             logging;
char            configfile[FILENAME_MAX];
FILE            *configFileFD;
int             initialIFFlags;

unsigned int    interfaceCount;         // Total number of interface/prefix combos
// We dynaimcally size this at run-time
struct  npd6Interface *interfaces;

// Key behaviour
int             naLinkOptFlag;      // From config file NPD6OPTFLAG
int             nsIgnoreLocal;      // From config file NPD6LOCALIG
int             naRouter;           // From config file NPD6ROUTERNA
int             maxHops;            // From config file NPD6MAXHOPS
int             collectTargets;     // From config file NPD6TARGETS

// Target tree data structures etc
void            *tRoot;
int             tCompare(const void *, const void *);
void            tDump(const void *, const VISIT , const int);
int             tEntries;

// Black/whitelisting data
void            *lRoot;
int             listType;
#define         NOLIST      0
#define         BLACKLIST   1
#define         WHITELIST   2
int             listLog;            // From config file NPD6LISTLOG

// Logging - various
int             ralog;              // From config file NPD6RALOG

// Error handling
int		        pollErrorLimit;     // From config file

