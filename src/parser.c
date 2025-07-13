#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "tsh.h"

token *putback = NULL;

#define syntax_error(tok) {flags|=TASH_IGN_NL;error("syntax error near token %s",token_name(tok));putback = tok;return 0;}

static token *get_token(FILE *file){
	if(putback){
		token *tok = putback;
		putback = NULL;
		return tok;
	}
	return next_token(file);
}

#define append(s) len += strlen(s);\
		str = realloc(str,len);\
		strcat(str,s);

char *parse_var(FILE *file){
	token *tok = get_token(file);
	char tmp[32];
	switch(tok->type){
	case T_DOLLAR:
		destroy_token(tok);
#ifdef __stanix__
		sprintf(tmp,"%ld",getpid());
#else
		sprintf(tmp,"%d",getpid());
#endif
		return strdup(tmp);
		break;
	case T_STR:;
		char *var = getvar(tok->value);
		destroy_token(tok);
		return strdup(var ? var : "");
	case T_OPEN_BRACK:
		destroy_token(tok);
		tok = get_token(file);
		if(tok->type != T_STR)syntax_error(tok);
		char *val = getvar(tok->value);
		destroy_token(tok);
		tok = get_token(file);
		if(tok->type != T_CLOSE_BRACK)syntax_error(tok);
		destroy_token(tok);
		return strdup(val ? val: "");

	default:
		syntax_error(tok);
	}
}

static char *get_string(FILE *file){
	token *tok = get_token(file);
	char *str = strdup("");
	size_t len = 1;
	int has_str = 0;
	for(;;){
	switch(tok->type){
	case T_STR:
		append(tok->value);
		break;
	case T_QUOTE:
		//continue until colon again
		for(;;){
			destroy_token(tok);
			tok = get_token(file);
			if(tok->type == T_EOF){
				syntax_error(tok);
			}
			if(tok->type == T_QUOTE)break;
			if(!(flags & TASH_NOPS) && tok->type == T_NEWLINE)show_ps2();
			const char *name = token2str(tok);
			append(name);
		
		}
		break;
	case T_DQUOTE:
		//continue until double colon again
		for(;;){
			destroy_token(tok);
			tok = get_token(file);
			if(tok->type == T_EOF){
				syntax_error(tok);
			}
			if(tok->type == T_DQUOTE)break;
			if(!(flags & TASH_NOPS) && tok->type == T_NEWLINE)show_ps2();
			switch(tok->type){
			case T_DOLLAR:;
				char *val = parse_var(file);
				if(!val)syntax_error(tok);
				append(val);
				free(val);
				continue;
			case T_BACKSLASH:
				destroy_token(tok);
				tok = get_token(file);
				if(tok->type == T_EOF)syntax_error(tok);
				append(token2str(tok));
				continue;
			}

			const char *name = token2str(tok);
			append(name);
		
		}
		break;
		break;
	case T_DOLLAR:;
		char *val = parse_var(file);
		if(!val)syntax_error(tok);
		append(val);
		free(val);
		break;
	case T_BACKSLASH:
		destroy_token(tok);
		tok = get_token(file);
		if(tok->type == T_EOF)syntax_error(tok);
		if(tok->type == T_NEWLINE)break;
		append(token2str(tok));
		break;
	default:
		goto end;
	}
		has_str = 1;
		destroy_token(tok);
		tok = get_token(file);
	}
end:
	putback = tok;
	if(!has_str){
		//we don't have a str return NULL
		free(str);
		return NULL;
	}
	return str;
}

static void skip_space(FILE *file){
	token *tok = get_token(file);
	while(tok->type == T_SPACE){
		destroy_token(tok);
		tok = get_token(file);
	}
	putback = tok;
}

int interpret_expr(FILE *file,int *is_last){
	*is_last = 0;
	int argc = 0;
	char **argv = malloc(0);
	int in = 0;
	int out = 0;
	for(;;){
		skip_space(file);
		char *arg = get_string(file);
		if(!arg){
			//not a string what it is ?
			token *tok = get_token(file);
			switch(tok->type){
			case T_SEMI_COLON:
				if(!argc)syntax_error(tok);
				destroy_token(tok);
				goto finish;
			case T_EOF:
				*is_last = 1;
				destroy_token(tok);
				if((flags & TASH_IGN_NL) || (flags & TASH_IGN_EOF)){
					flags &= ~(TASH_IGN_NL | TASH_IGN_EOF);
					goto ret;
				}
				goto finish;
			case T_NEWLINE:
				*is_last = 1;
				destroy_token(tok);
				if(flags & TASH_IGN_NL){
					flags &= ~TASH_IGN_NL;
					goto ret;
				}
				goto finish;
			case T_SUPERIOR:
			case T_APPEND:
				if(out)close(out);
				int flags = tok->type == T_APPEND ? O_APPEND : O_TRUNC;
				destroy_token(tok);
				skip_space(file);
				tok = get_token(file);
				if(tok->type != T_STR)syntax_error(tok);
				out = open(tok->value,O_WRONLY | O_CREAT | flags,S_IWUSR | S_IRUSR);
				if(out < 0){
					perror(tok->value);
					return 0;
				}
				destroy_token(tok);

				continue;
			case T_INFERIOR:
				if(in)close(in);
				destroy_token(tok);
				skip_space(file);
				tok = get_token(file);
				if(tok->type != T_STR)syntax_error(tok);
				in = open(tok->value,O_RDONLY);
				if(in < 0){
					perror(tok->value);
					return 0;
				}
				destroy_token(tok);

				continue;
			case T_HASH:
				while(tok->type != T_NEWLINE && tok->type != T_EOF){
					destroy_token(tok);
					tok = get_token(file);
				}
				putback = tok;
				continue;
			}
			destroy_token(tok);

		}
		argc++;
		argv = realloc(argv,sizeof(char *) * argc);
		argv[argc-1] = arg;
	}
finish:
	argv = realloc(argv,sizeof(char *) * (argc + 1));
	argv[argc] = NULL;

	if((flags & TASH_IGN_NL) || (flags & TASH_IGN_EOF)){
ret:
		for(int i=0;i<argc;i++){
			free(argv[i]);
		}
		free(argv);
		return 0;
	}

	int status;
	if(argc > 0){
		//first check for builtin
		status = check_builtin(argc,argv);
		//-1 mean no builtin
		if(status == -1){
		pid_t child = fork();
		if(!child){
			if(out){
				if(dup2(out,STDOUT_FILENO) < 0){
					perror("dup2");
				}
				close(out);
			}
			if(in){
				if(dup2(in,STDIN_FILENO) < 0){
					perror("dup2");
				}
				close(in);
			}
			execvp(argv[0],argv);
			perror(argv[0]);
			exit(EXIT_FAILURE);
		}
		for(int i=0;i<argc;i++){
			free(argv[i]);
		}

		if(out)close(out);
		if(in)close(in);

		if(child < 0){
			perror("fork");
			return 1;
		}

		if(setpgid(child,child) < 0){
			perror("setpgid");
		} else if(!(flags & TASH_NOPS) && tcsetpgrp(STDIN_FILENO,child) < 0){
			perror("tcsetpgrp");
		}
		if(waitpid(child,&status,0) < 0){
			perror("waitpid");
			return 1;
		}
		if(!(flags & TASH_NOPS) && (signal(SIGTTOU,SIG_IGN) == SIG_ERR || tcsetpgrp(STDIN_FILENO,getpid()) < 0 || signal(SIGTTOU,SIG_DFL) == SIG_ERR)){
			perror("tcsetpgrp"); 
		}
		if(WIFSIGNALED(status)){
			printf("terminated on %s\n",strsignal(WTERMSIG(status)));
		}
		}
	}

	free(argv);

	return 0;
}

int interpret_line(FILE *file){
	//if is_last is set
	//that mean that was the expr of the line
	int is_last = 0;
	int status;
	while(!is_last){
		status = interpret_expr(file,&is_last);
	}
	return status;
}

int interpret(FILE *file){
	for(;;){
		if(!(flags & TASH_NOPS)){
			show_ps1();
		}
		token *tok = get_token(file);
		if(tok->type == T_EOF)break;
		putback = tok;
		interpret_line(file);
	}

	return 0;
}
