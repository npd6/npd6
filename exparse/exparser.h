/*
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

#ifndef _EXPARSER_LIB_H
#define _EXPARSER_LIB_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define EXP_MAX_MAPPED_SYMBOLS (32)
#define EXP_ARITHMETIC         (0x000)
#define EXP_LOGICAL            (0x100)
#define EXP_COMPARE            (0x200)
#define EXP_RELATIONAL1        (0x400)
#define EXP_RELATIONAL2        (0x800)

#define EXP_FUNCTION_ABS       "abs"
#define EXP_FUNCTION_MASK      "mask"

typedef enum Token_value {
        NAME,
        NUMBER,
        END,
        PLUS='+',
        MINUS='-',
        MUL='*',
        DIV='/',
        NOT='~',
        AND='&',
        OR='|',
        SR='>',
        SL='<',
	MOD='%',
        XOR='^',
        PRINT=';',
        ASSIGN='=',
        LP='(',
        RP=')',
        COMMA=',',
        LNOT=EXP_LOGICAL|'!',
        LOR=EXP_LOGICAL|'|',
        LAND=EXP_LOGICAL|'&',
        CNE=EXP_COMPARE|'!',
        CE=EXP_COMPARE|'=',
        CGT=EXP_RELATIONAL1|'>',
        CLT=EXP_RELATIONAL1|'<',
        CGE=EXP_RELATIONAL2|'>',
        CLE=EXP_RELATIONAL2|'<',
} Token_value_t;

typedef struct _exp_parse_map {
  char name[32];
  unsigned long long value;
} exp_parse_map_t;

typedef struct _exp_pstat {
  int errors;
  unsigned long long number_value;
  char string_value[256];
  Token_value_t curr_tok;
  char* input;
  exp_parse_map_t map[EXP_MAX_MAPPED_SYMBOLS];
} exp_pstat_t;

char* exp_get_library_version();
int   exp_parse_expression(exp_pstat_t* pstat, char* expr, unsigned long long* result);
int   exp_set_mapped_value(exp_pstat_t* pstat, char* name, unsigned long long value);

unsigned long long exp_ipv6_prefix_to_ull(struct in6_addr* ipv6);
unsigned long long exp_ipv6_host_to_ull(struct in6_addr* ipv6);
void exp_pull_hull_to_ipv6(unsigned long long prefix, unsigned long long host, struct in6_addr* ipv6);

unsigned long long exp_ipv4_address_to_ull(struct in_addr* ipv4);
void exp_ull_to_ipv4_address(unsigned long long ull, struct in_addr* ipv4);

void exp_ipv6_string_to_addr(char* ipv6_str, struct in6_addr* ipv6);
void exp_ipv6_addr_to_string(struct in6_addr* ipv6, char* ipv6_str, int length);
void exp_ipv4_string_to_addr(char* ipv4_str, struct in_addr* ipv4);
void exp_ipv4_addr_to_string(struct in_addr* ipv4, char* ipv4_str, int length);


#ifdef __cplusplus
}
#endif

#endif // !_EXPARSER_LIB_H

