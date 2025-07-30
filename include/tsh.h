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

typedef struct source {
	void *data;
	int (*getc)(void *);
	int (*unget)(int,void *);
	size_t if_depth;
	int op;
	int flags;
	token *putback;
} source;

#define T_NULL          0
#define T_STR           1
#define T_AND           2
#define T_OR            3
#define T_EOF           4
#define T_APPEND        5
#define T_PIPE         '|'
#define T_BG           '&'
#define T_SPACE        ' '
#define T_OPEN_BRACK   '{'
#define T_CLOSE_BRACK  '}'
#define T_OPEN_PAREN   '('
#define T_CLOSE_PAREN  ')'
#define T_SEMI_COLON   ';'
#define T_DQUOTE       '"'
#define T_QUOTE        '\''
#define T_INFERIOR     '<'
#define T_SUPERIOR     '>'
#define T_NEWLINE      '\n'
#define T_HASH         '#'
#define T_DOLLAR       '$'
#define T_BACKSLASH    '\\'
#define T_QUESTION_MARK '?'

#define KEYWORD_IF     1
#define KEYWORD_ELSE   2
#define KEYWORD_ELIF   3
#define KEYWORD_FI     4
#define KEYWORD_THEN   5
#define KEYWORD_FOR    6
#define KEYWORD_IN     7
#define KEYWORD_DO     8
#define KEYWORD_DONE   9
#define KEYWORD_WHILE  10
#define KEYWORD_UNTIL  11
#define KEYWORD_CASE   12
#define KEYWOED_ESAC   13

extern int _argc;
extern char **_argv;
extern int flags;
extern int exit_status;
#define TASH_LOGIN       (1 << 0)
#define TASH_INTERACTIVE (1 << 1)
#define TASH_NOPS        (1 << 2)
#define TASH_IGN_NL      (1 << 3)
#define TASH_IGN_EOF     (1 << 4)
#define TASH_SUBSHELL    (1 << 5)

#define arraylen(ar) (sizeof(ar)/sizeof(*ar))

void error(const char *fmt,...);

token *next_token(source *src);
void destroy_token(token *);
const char *token_name(token *);
const char *token2str(token *);

int check_builtin(int argc,char **argv);

int interpret(source *src);
int eval(const char *str,int flags);

void show_ps1(void);
void show_ps2(void);
void prompt_unget(int c);
int prompt_getc(void);

void init_var(void);
char *getvar(const char *name);
void *putvar(const char *var);

void init();

// cute custom perror
#undef perror
#define perror(str) error("%s : %s",str,strerror(errno))

#define SRC_FILE(file) {.data = file,\
	.getc  = (void *)fgetc,\
	.unget = (void *)ungetc,\
	.flags = TASH_NOPS}

#endif
