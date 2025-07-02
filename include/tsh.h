#ifndef _TSH_H
#define _TSH_H

#include <errno.h>
#include <string.h>
#include <stdio.h>

typedef struct token {
	int type;
	char *value;
} token;


typedef struct builtin {
	int (*func)(int,char **);
	char *name;
	int lock_bypass;
} builtin;

#define T_NULL         0
#define T_STR          1
#define T_AND          2
#define T_OR           3
#define T_EOF          4
#define T_APPEND       5
#define T_PIPE        '|'
#define T_BG          '&'
#define T_SPACE       ' '
#define T_OPEN_BRACK  '{'
#define T_CLOSE_BRACK '}'
#define T_OPEN_PAREN  '('
#define T_CLOSE_PAREN ')'
#define T_SEMI_COLON  ';'
#define T_DQUOTE      '"'
#define T_QUOTE       '\''
#define T_INFERIOR    '<'
#define T_SUPERIOR    '>'
#define T_NEWLINE     '\n'
#define T_HASH        '#'
#define T_DOLLAR      '$'

#define arraylen(ar) (sizeof(ar)/sizeof(*ar))

void error(const char *fmt,...);

token *next_token(FILE *file);
void destroy_token(token *);
const char *token_name(token *);
const char *token2str(token *);

int interpret(FILE *file);

void show_ps1(void);
void show_ps2(void);

char *getvar(const char *name);

void init(int argc,char **argv);

// cute custom perror
#undef perror
#define perror(str) error("%s : %s",str,strerror(errno))

#endif
