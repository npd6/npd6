// Interface to the expression parser

#include <stdio.h>
#include <string.h>
#include <netinet/ip6.h>

#include "expintf.h"
#include "exparser.h"

#define MAX_EXPRESSION_LENGTH  128
#define MAX_EXPRESSION_ENTRIES 64

static int  sExpression = 0;
static char sExpressions[MAX_EXPRESSION_ENTRIES][MAX_EXPRESSION_LENGTH];

int storeExpression(char* expression)
{
  if(sExpression >= MAX_EXPRESSION_ENTRIES)
  {
    return -1;
  }
  strncpy(&sExpressions[sExpression][0], expression+sizeof("exprlist=")-1, MAX_EXPRESSION_LENGTH);
  sExpressions[sExpression][MAX_EXPRESSION_LENGTH-1] = 0;
  sExpression++;
  return 0;
}

int compareExpression(struct in6_addr* ipv6)
{
  int expression = 0;
  unsigned long long prefix = 0;
  unsigned long long host = 0;
  unsigned long long result = 0;
  exp_pstat_t pstat;

  memset(&pstat, 0, sizeof(exp_pstat_t));

  // Convert the IPv6 address into two 64-bit values
  prefix = exp_ipv6_prefix_to_ull(ipv6);
  host = exp_ipv6_host_to_ull(ipv6);

  // Preload variables with the 64-bit values
  exp_set_mapped_value(&pstat, "PREFIX", prefix);
  exp_set_mapped_value(&pstat, "HOST", host);

  // Compare each compression to the values
  for(expression=0; expression<sExpression; expression++)
  {
    pstat.errors = 0; result = 0;
    exp_parse_expression(&pstat, sExpressions[expression], &result);
    if(pstat.errors == 0)
    {
      if(result != 0)
      {
        return 1;
      }
    }
  }
  return 0;
}

