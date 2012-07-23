/*****************************************************************************
 Expression Parser

  Copyright (c) 2010,2011,2012 RoyalAnarchy.com, All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//
//  Original algorithm/code from Bjarne Stroustrup (bs@research.att.com)
//  The original published work appears to be free of any copy restrictions.
//    http://www2.research.att.com/~bs/dc_command_line.c
//
//  Revision History:
//    - Modified to use unsigned long long instead of double
//    - Added: %, >>, <<, ~, ==, !=, >=, <=, >, <, &&, ||, !
//    - Added: abs()
//    - Added: mask()
//    - Added: network to/from unsigned long long conversions
//
//    program:
//	END			   // END is end-of-input
//	expr_list END
//
//    expr_list:
//	expression PRINT	   // PRINT is semicolon
//	expression PRINT expr_list
//
//    expression:
//	expression + term
//	expression - term
//	term
//
//    term:
//	term / primary
//	term * primary
//	term & primary
//	term | primary
//	term % primary
//	term >> primary
//	term << primary
//      term ^ primary
//      term > primary
//      term >= primary
//      term < primary
//      term <= primary
//      term == primary
//      term != primary
//      term && primary
//      term || primary
//	primary
//
//    primary:
//	NUMBER
//	NAME
//	NAME = expression
//	- primary
//	~ primary
//	! primary
//	( expression )
//
//  Notes:
//   - This code has only been tested on LITTLE_ENDIAN (Intel) machines.
//   - If you get unexpected results, consider using more parenthesis.
//
//*****************************************************************************

#define PARSER_LIBRARY_VERSION  "0.9.10"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#ifdef INCLUDE_IP_ADDRESS_SUPPORT
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "exparser.h"

static unsigned long long s_expression(exp_pstat_t* pstat, int get);
static Token_value_t s_get_token(exp_pstat_t* pstat);

char*
exp_get_library_version(void)
{
  return PARSER_LIBRARY_VERSION;
}

unsigned long long
exp_get_mapped_value(exp_pstat_t* pstat, char* name)
{
  int i;
  for(i=0; i<EXP_MAX_MAPPED_SYMBOLS; i++)
  {
    if(pstat->map[i].name[0] != 0)
    {
      if((strcmp(name, pstat->map[i].name)) == 0)
      {
        return pstat->map[i].value;
      }
    }
  }
  return 0;
}

int
exp_set_mapped_value(exp_pstat_t* pstat, char* name, unsigned long long value)
{
  int i;
  for(i=0; i<EXP_MAX_MAPPED_SYMBOLS; i++)
  {
    if(pstat->map[i].name[0] != 0)
    {
      if((strcmp(name, pstat->map[i].name)) == 0)
      {
        pstat->map[i].value = value;
        return 0;
      }
    }
  }

  for(i=0; i<EXP_MAX_MAPPED_SYMBOLS; i++)
  {
    if(pstat->map[i].name[0] == 0)
    {
      strcpy(pstat->map[i].name, name);
      pstat->map[i].value = value;
      return 0;
    }
  }

  return -1;
}

static unsigned long long
s_function_abs(exp_pstat_t* pstat)
{
  unsigned long long v = 0;
  long long sv = 0;

  if(s_get_token(pstat) != LP)
  {
    fprintf(stderr, "( expected: %d(0x%x)", pstat->curr_tok, pstat->curr_tok);
    return 0;
  }
  v = s_expression(pstat, 1);
  if(pstat->curr_tok != RP)
  { 
    fprintf(stderr, ") expected: %d(0x%x)", pstat->curr_tok, pstat->curr_tok);
    return 0;
  }
  s_get_token(pstat);
  sv = (long long)v;
  if(sv < 0)
  {
    return -sv;
  }
  return v;
}

static unsigned long long
s_function_mask(exp_pstat_t* pstat)
{
  long long startbit = 0;
  long long endbit = 0;
  long long tmpbit = 0;
  unsigned long long value = 0;
  unsigned char* vp = 0;
  int byte = 0;
  int shift = 0;
  int mask = 0;

  if(s_get_token(pstat) != LP)
  {
    fprintf(stderr, "( expected: %d(0x%x)", pstat->curr_tok, pstat->curr_tok);
    return 0;
  }
  startbit = s_expression(pstat, 1);
  if(pstat->curr_tok != COMMA)
  { 
    fprintf(stderr, ", expected: %d(0x%x)", pstat->curr_tok, pstat->curr_tok);
    return 0;
  }
  endbit = s_expression(pstat, 1);
  if(pstat->curr_tok != RP)
  { 
    fprintf(stderr, ") expected: %d(0x%x)", pstat->curr_tok, pstat->curr_tok);
    return 0;
  }
  s_get_token(pstat);

  value = 0;
  vp = (unsigned char*)&value+7;

  if(startbit > 63)
  {
    startbit = 63;
  }
  if(endbit < 0)
  {
    endbit = 0;
  }
  if(endbit > startbit)
  {
    tmpbit = endbit;
    endbit = startbit;
    startbit = tmpbit;
  }

  vp = (unsigned char*)&value+7;
  for(byte=7; byte>= 0; byte--)
  {
    if((startbit >= (byte*8)) && (startbit < (byte+1)*8))
    {
      shift = (((byte+1)*8)-1) - startbit;
      mask = ((0xffU >> (shift)) & 0xffU);
      *vp = mask;
    }
    else if((startbit > (byte*8)))
    {
      *vp = 0xff;
    }

    if((endbit >= (byte*8)) && (endbit < (byte+1)*8))
    {
      shift = (((byte+1)*8)) - endbit;
      mask = ((0xffU >> (shift)) & 0xffU);
      *vp &= ~mask;
      break;
    }
    vp--;
  }
  return value;
}

static Token_value_t
s_get_token(exp_pstat_t* pstat)
{
  char ch = 0;

  //Skip white space
  while(1)
  {
    if(*pstat->input == 0)
    {
      return pstat->curr_tok = END;
    }
    if(!(isspace(*pstat->input)))
    {
      break;
    }
    pstat->input++;
  }

  ch = *pstat->input;

  switch(ch)
  {
    case ';':
    case '\n':
      pstat->input++;
      return pstat->curr_tok = PRINT;

    case '*':
    case '/':
    case '+':
    case '-':
    case '(':
    case ')':
    case '%':
    case '^':
    case '~':
    case ',':
      pstat->input++;
      return pstat->curr_tok = (Token_value_t)ch;

    case '&':
      pstat->input++;
      switch(*pstat->input)
      {
        case '&':
          pstat->input++;
          return pstat->curr_tok = LAND;
        default:
          return pstat->curr_tok = AND;
      }

    case '|':
      pstat->input++;
      switch(*pstat->input)
      {
        case '|':
          pstat->input++;
          return pstat->curr_tok = LOR;
        default:
          return pstat->curr_tok = OR;
      }

    case '!':
      pstat->input++;
      switch(*pstat->input)
      {
        case '=':
          pstat->input++;
          return pstat->curr_tok = CNE;
        default:
          return pstat->curr_tok = LNOT;
      }

    case '=':
      pstat->input++;
      switch(*pstat->input)
      {
        case '=':
          pstat->input++;
          return pstat->curr_tok = CE;
        default:
          return pstat->curr_tok = ASSIGN;
      }

    case '>':
      pstat->input++;
      switch(*pstat->input)
      {
        case '=':
          pstat->input++;
          return pstat->curr_tok = CGE;
        case '>':
          pstat->input++;
          return pstat->curr_tok = SR;
        default:
          return pstat->curr_tok = CGT;
      }

    case '<':
      pstat->input++;
      switch(*pstat->input)
      {
        case '=':
          pstat->input++;
          return pstat->curr_tok = CLE;
        case '<':
          pstat->input++;
          return pstat->curr_tok = SL;
        default:
          return pstat->curr_tok = CLT;
      }

    case '0' ... '9':
      pstat->number_value = strtoull(pstat->input, &pstat->input, 0);
      return pstat->curr_tok = NUMBER;

    default:
      if((isalpha(ch)) || (ch == '$') || (ch == '.'))
      {
        int i = 0;
        pstat->string_value[i] = 0;
        while((*pstat->input != 0) && ((isalnum(*pstat->input)) ||
              (*pstat->input == '$') || (*pstat->input == '.')))
        {
          pstat->string_value[i] = *pstat->input;
          pstat->input++;
          i++;
        }
        pstat->string_value[i] = 0;
        return pstat->curr_tok = NAME;
      }
      fprintf(stderr, "Bad token: %d(0x%x)", ch, ch);
      pstat->errors++;
      pstat->input++;
      return pstat->curr_tok = PRINT;
  }
}

static unsigned long long
s_primary(exp_pstat_t* pstat, int get)
{
  unsigned long long v = 0;
  char string_save[256];

  if(get)
  {
    s_get_token(pstat); 
  }

  switch(pstat->curr_tok)
  {
    case NUMBER: // Unsigned unsigned long long constant
      v = pstat->number_value;
      s_get_token(pstat);
      return v;

    case NAME:
      // abs() function?
      if((strcasecmp(pstat->string_value, EXP_FUNCTION_ABS)) == 0)
      {
        return s_function_abs(pstat);
      }

      // mask() function?
      if((strcasecmp(pstat->string_value, EXP_FUNCTION_MASK)) == 0)
      {
        return s_function_mask(pstat);
      }

      if(s_get_token(pstat) == ASSIGN)
      {
        strcpy(string_save, pstat->string_value);
        v = s_expression(pstat, 1);
        exp_set_mapped_value(pstat, string_save, v);
      }
      else
      {
        v = exp_get_mapped_value(pstat, pstat->string_value);
      }
      return v;

    case MINUS:
      return -s_primary(pstat, 1);

    case PLUS:
      return s_primary(pstat, 1);

    case NOT:
      return ~s_primary(pstat, 1);

    case LNOT:
      v = s_primary(pstat, 1);
      if(v)
      {
        return 0;
      }
      return 1;

    case LP:
      v = s_expression(pstat, 1);
      if(pstat->curr_tok != RP)
      {
        fprintf(stderr, ") expected: %d(0x%x)", pstat->curr_tok, pstat->curr_tok);
        pstat->errors++;
        return 0;
      } 
      s_get_token(pstat);
      return v;

    default:
      fprintf(stderr, "primary expected: %d(0x%x)", pstat->curr_tok, pstat->curr_tok);
      pstat->errors++;
      return 0;
  }
}

static unsigned long long
s_term(exp_pstat_t* pstat, int get)
{
  unsigned long long left = s_primary(pstat, get);
  unsigned long long right = 0;
  
  while(1)
  {
    switch(pstat->curr_tok)
    {
      case AND:
        left &= s_primary(pstat, 1);
        break;

      case OR:
        left |= s_primary(pstat, 1);
        break;

      case MUL:
        left *= s_primary(pstat, 1);
        break;

      case DIV:
        right = s_primary(pstat, 1);
        if(right != 0)
        {
          left /= right;
          break;
        }
        fprintf(stderr, "Divide by zero: %d(0x%x)", pstat->curr_tok, pstat->curr_tok);
        pstat->errors++;
        left = 0;
        break;

      case SR:
        left >>= s_primary(pstat, 1);
        break;

      case SL:
        left <<= s_primary(pstat, 1);
        break;

      case MOD:
        left %= s_primary(pstat, 1);
        break;

      case XOR:
        left ^= s_primary(pstat, 1);
        break;

      case CE:
        right = s_primary(pstat, 1);
        if(left == right)
        {
          left = 1;
        }
        else
        {
          left = 0;
        }
        break;

      case CNE:
        right = s_primary(pstat, 1);
        if(left != right)
        {
          left = 1;
        }
        else
        {
          left = 0;
        }
        break;

      case CGT:
        right = s_primary(pstat, 1);
        if(left > right)
        {
          left = 1;
        }
        else
        {
          left = 0;
        }
        break;

      case CLT:
        right = s_primary(pstat, 1);
        if(left < right)
        {
          left = 1;
        }
        else
        {
          left = 0;
        }
        break;

      case CGE:
        right = s_primary(pstat, 1);
        if(left >= right)
        {
          left = 1;
        }
        else
        {
          left = 0;
        }
        break;

      case CLE:
        right = s_primary(pstat, 1);
        if(left <= right)
        {
          left = 1;
        }
        else
        {
          left = 0;
        }
        break;

      case LOR:
        right = s_primary(pstat, 1);
        left = left || right;
        break;

      case LAND:
        right = s_primary(pstat, 1);
        left = left && right;
        break;

      default:
        return left;
    }
  }
}

static unsigned long long
s_expression(exp_pstat_t* pstat, int get)
{
  unsigned long long left = s_term(pstat, get);

  while(1)
  {
    switch(pstat->curr_tok)
    {
      case PLUS:
	left += s_term(pstat, 1);
        break;

      case MINUS:
        left -= s_term(pstat, 1);
        break;

      default:
        return left;
    }
  }
}

int
exp_parse_expression(exp_pstat_t* pstat, char* expr, unsigned long long* result)
{
  pstat->input = expr;

  while(*pstat->input)
  {
    s_get_token(pstat);

    if(pstat->curr_tok == END)
    {
      break;
    }

    if(pstat->curr_tok == PRINT)
    {
      continue;
    }

    *result = s_expression(pstat, 0);
  }

  return 0;
}

#ifdef INCLUDE_IP_ADDRESS_SUPPORT
unsigned long long
exp_ipv6_prefix_to_ull(struct in6_addr* ipv6)
{
  int offset = 0;
  unsigned long long temp = 0;
  unsigned char* dst = (unsigned char*)&temp+7;
  unsigned char* src = (unsigned char*)ipv6;
  for(offset=0; offset<8; offset++, dst--, src++)
  {
    *dst = *src;
  }
  return temp;
}

unsigned long long
exp_ipv6_host_to_ull(struct in6_addr* ipv6)
{
  int offset = 0;
  unsigned long long temp = 0;
  unsigned char* dst = (unsigned char*)&temp+7;
  unsigned char* src = (unsigned char*)ipv6+8;
  for(offset=0; offset<8; offset++, dst--, src++)
  {
    *dst = *src;
  }
  return temp;
}

void
exp_pull_hull_to_ipv6(unsigned long long prefix, unsigned long long host, struct in6_addr* ipv6)
{
  int offset = 0;
  unsigned char* src = (unsigned char*)&prefix;
  unsigned char* dst = (unsigned char*)ipv6+7;
  for(offset=0; offset<8; offset++, dst--, src++)
  {
    *dst = *src;
  }

  src = (unsigned char*)&host;
  dst = (unsigned char*)ipv6+15;
  for(offset=0; offset<8; offset++, dst--, src++)
  {
    *dst = *src;
  }

  return;
}

unsigned long long
exp_ipv4_address_to_ull(struct in_addr* ipv4)
{
  unsigned long long temp = ntohl(ipv4->s_addr);
  return temp;
}

void
exp_ull_to_ipv4_address(unsigned long long ull, struct in_addr* ipv4)
{
  unsigned long temp = ull & 0xffffffff;
  temp = htonl(temp);
  memcpy(ipv4, &temp, 4);
  return;
}

void
exp_ipv6_string_to_addr(char* ipv6_str, struct in6_addr* ipv6)
{
  memset(ipv6, 0, sizeof(struct in6_addr));
  inet_pton(AF_INET6, ipv6_str, ipv6);
}

void
exp_ipv6_addr_to_string(struct in6_addr* ipv6, char* ipv6_str, int length)
{
  memset(ipv6_str, 0, length);
  inet_ntop(AF_INET6, ipv6, ipv6_str, (socklen_t)length);
}

void
exp_ipv4_string_to_addr(char* ipv4_str, struct in_addr* ipv4)
{
  memset(ipv4, 0, sizeof(struct in_addr));
  inet_pton(AF_INET, ipv4_str, ipv4);
}

void
exp_ipv4_addr_to_string(struct in_addr* ipv4, char* ipv4_str, int length)
{
  memset(ipv4_str, 0, length);
  inet_ntop(AF_INET, ipv4, ipv4_str, (socklen_t)length);
}

#endif

