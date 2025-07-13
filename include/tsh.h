#ifndef _TSH_H
#define _TSH_H

#include <errno.h>
#include <string.h>
#include <stdio.h>

typedef struct token {
	int type;
	char *value;
} token;


struct builtin {
	int (*func)(int,char **);
	char *name;
	int lock_bypass;
};

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
#define T_BACKSLASH   '\\'

extern int flags;
#define TASH_LOGIN       (1 << 0)
#define TASH_INTERACTIVE (1 << 1)
#define TASH_NOPS        (1 << 2)
#define TASH_IGN_NL      (1 << 3)
#define TASH_IGN_EOF     (1 << 4)

#define arraylen(ar) (sizeof(ar)/sizeof(*ar))

void error(const char *fmt,...);

token *next_token(FILE *file);
void destroy_token(token *);
const char *token_name(token *);
const char *token2str(token *);

int check_builtin(int argc,char **argv);

int interpret(FILE *file);

void show_ps1(void);
void show_ps2(void);

char *getvar(const char *name);

void init(int argc,char **argv);

// cute custom perror
#undef perror
#define perror(str) error("%s : %s",str,strerror(errno))

#endif
