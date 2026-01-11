#ifndef _TSH_H
#define _TSH_H

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <vector.h>
#include <stdio.h>

typedef struct token {
	char *value;
	int type;
	int digit;
	int flags;
} token_t;

#define T_NULL           0
#define T_WORD           1
#define T_AND            2
#define T_OR             3
#define T_EOF            4
#define T_APPEND         5
#define T_DUP_OUT        6
#define T_DUP_IN         7
#define T_PIPE          '|'
#define T_BG            '&'
#define T_SPACE         ' '
#define T_OPEN_PAREN    '('
#define T_CLOSE_PAREN   ')'
#define T_SEMI_COLON    ';'
#define T_DQUOTE        '"'
#define T_QUOTE         '\''
#define T_INFERIOR      '<'
#define T_SUPERIOR      '>'
#define T_NEWLINE       '\n'
#define T_HASH          '#'
#define T_DOLLAR        '$'
#define T_BACKSLASH     '\\'
#define T_QUESTION_MARK '?'
#define T_IF            256
#define T_THEN          257
#define T_ELSE          258
#define T_ELIF          259
#define T_FI            260
#define T_FOR           261
#define T_IN            262
#define T_DO            263
#define T_DONE          264
#define T_WHILE         265
#define T_UNTIL         266
#define T_CASE          267
#define T_ESAC          268
#define T_OPEN_BRACKET  269
#define T_CLOSE_BRACKET 270
#define T_BANG          271

typedef struct word {
	char *text;
	int flags;
} word_t;

#define WORD_HAS_QUOTE 0x01
#define CTLESC  0x01
#define CTLQUOT 0x02

typedef struct redir {
	int fd;
	int type;
	word_t dest;
} redir_t;

#define REDIR_IN     0x01
#define REDIR_APPEND 0x02
#define REDIR_DUP    0x04

typedef struct node {
	int type;
	redir_t *redirs;
	size_t redirs_count;
	union {
		struct {
			struct node *left;
			struct node *right;
		} binary;
		struct {
			struct node *child;
		} single;
		struct {
			word_t *args;
			size_t args_count;
		} cmd;
		struct {
			struct node *condition;
			struct node *body;
			struct node *else_body;
		} _if;
		struct {
			struct node *condition;
			struct node *body;
		} loop;
	};
} node_t;

#define NODE_NULL     0
#define NODE_CMD      1
#define NODE_PIPE     2
#define NODE_NEGATE   3
#define NODE_OR       4
#define NODE_AND      5
#define NODE_SEP      6
#define NODE_BG       7
#define NODE_IF       8
#define NODE_WHILE    9
#define NODE_UNTIL    10
#define NODE_SUBSHELL 11
#define NODE_GROUP    12

typedef struct lexer {
	token_t *putback;
	int hint;
} lexer_t;

#define LEXER_COMMAND   1
#define LEXER_ARGS      2
#define LEXER_FILE      3
#define LEXER_EXPECT_IN 1

typedef struct source {
	void *data;
	int (*get_char)(void *);
	int (*unget_char)(int,void *);
	lexer_t lexer;
} source_t;

typedef struct builtin {
	int (*func)(int,char **);
	char *name;
} builtin_t;

/*
 * @brief represent a process group/job
 */
typedef struct group {
	vector_t childs;
	pid_t pid;
} group_t;

extern int _argc;
extern char **_argv;
extern int flags;
extern int exit_status;
#define TASH_LOGIN       (1 << 0)
#define TASH_INTERACTIVE (1 << 1)
#define TASH_SUBSHELL    (1 << 2)
#define TASH_ERR_EXIT    (1 << 3) // exit on error
#define TASH_UNSET_EXIT  (1 << 4) // exit on unset
#define arraylen(ar) (sizeof(ar)/sizeof(*ar))

void error(const char *fmt,...);

// lexer functions
int peek_char(source_t *src);
int get_char(source_t *src);
token_t *next_token(source_t *src);
void unget_token(source_t *src, token_t *token);
void destroy_token(token_t *token);
const char *token_name(token_t *);

int try_builtin(int argc, char **argv);

// interpret/execute
int interpret(source_t *src);
int eval(const char *str);
void execute(node_t *node, int flags);
char **word_expansion(word_t *words, size_t words_count);

// jobs control

/**
 * @brief fork and create a new proc inside a group
 * @param group the group in which to create
 * @return 0 to child, pid to parent or -1 on error
 */
pid_t job_fork(group_t *group);

/**
 * @brief wait for a job/group to terminate
 */
int job_wait(group_t *group);
int job_single(void);
void job_init_group(group_t *group);
void job_free_group(group_t *group);

// prompt management
void show_ps1(void);
void show_ps2(void);
void prompt_unget(int c);
int prompt_getc(void);

// variable management
void putvar(const char *name,const char *value);
char *getvar(const char *name);
void export_var(const char *name);
void setup_environ(void);
void setup_var(void);

void init();


struct var {
	int exported;
	char *value;
	char *name;
};

extern size_t var_count;
extern struct var *var;

// cute custom perror
#undef perror
#define perror(str) error("%s : %s",str,strerror(errno))

#define SRC_FILE(file) {.data = file,\
	.get_char  = (void *)fgetc,\
	.unget_char = (void *)ungetc,\
}

#endif
