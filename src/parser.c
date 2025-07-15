#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "tsh.h"

struct cmd {
	int argc;
	char **argv;
	int in;
	int out;
};

token *putback = NULL;

int exit_status ;

#define syntax_error(tok) {flags|=TASH_IGN_NL;\
	error("syntax error near token %s",token_name(tok));\
	if(tok->type == T_NEWLINE || tok->type == T_EOF){\
		putback = tok;\
	} else {\
		destroy_token(tok);\
	}\
	exit_status = 2;\
	return 0;}

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
	case T_QUESTION_MARK:
		sprintf(tmp,"%d",exit_status);
		return strdup(tmp);
	case T_HASH:
		sprintf(tmp,"%d",_argc-1 < 0  ? 0 : _argc-1);
		return strdup(tmp);
	case T_OPEN_BRACK:
		destroy_token(tok);
		tok = get_token(file);
		int len = 0;
		switch(tok->type){
		case T_HASH:
			len = 1;
			destroy_token(tok);
			tok = get_token(file);
			break;
		default:
			break;
		}
		if(tok->type != T_STR)syntax_error(tok);
		char *val = getvar(tok->value);
		destroy_token(tok);
		tok = get_token(file);
		if(tok->type != T_CLOSE_BRACK)syntax_error(tok);
		destroy_token(tok);
		if(len){
			sprintf(tmp,"%zd",val ? strlen(val) : 0);
			return strdup(tmp);
		}
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
		if(!val)break;
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
	int cmdc = 1;
	struct cmd *cmdv = malloc(sizeof(struct cmd));
	memset(cmdv,0,sizeof(struct cmd));
	for(;;){
		skip_space(file);
		char *arg = get_string(file);
		if(!arg){
			//not a string what it is ?
			token *tok = get_token(file);
			switch(tok->type){
			case T_SEMI_COLON:
				//todo : check somthing else 
				//to allow empty command with redir
				if(!cmdv[cmdc-1].argc)syntax_error(tok);
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
			case T_INFERIOR:
				if(cmdv[cmdc-1].in)close(cmdv[cmdc-1].in);
				destroy_token(tok);
				skip_space(file);
				tok = get_token(file);
				if(tok->type != T_STR)syntax_error(tok);
				cmdv[cmdc-1].in = open(tok->value,O_RDONLY);
				if(cmdv[cmdc-1].in < 0){
					exit_status = 1;
					perror(tok->value);
					flags |= TASH_IGN_NL;
				}
				destroy_token(tok);

				continue;
			case T_SUPERIOR:
			case T_APPEND:
				if(cmdv[cmdc-1].out)close(cmdv[cmdc-1].out);
				int oflags = tok->type == T_APPEND ? O_APPEND : O_TRUNC;
				destroy_token(tok);
				skip_space(file);
				tok = get_token(file);
				if(tok->type != T_STR)syntax_error(tok);
				cmdv[cmdc-1].out = open(tok->value,O_WRONLY | O_CREAT | oflags,S_IWUSR | S_IRUSR);
				if(cmdv[cmdc-1].out < 0){
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
			case T_PIPE:
				if(cmdv[cmdc-1].argc <= 0)syntax_error(tok);
				destroy_token(tok);
				//finish old command
				cmdv[cmdc-1].argv = realloc(cmdv[cmdc-1].argv,sizeof(char *) * (cmdv[cmdc-1].argc + 1));
				cmdv[cmdc-1].argv[cmdv[cmdc-1].argc]= NULL;
				//and createa new one
				cmdc++;
				cmdv = realloc(cmdv,cmdc * sizeof(struct cmd));
				memset(&cmdv[cmdc-1],0,sizeof(struct cmd));
				int pipefd[2];
				if(pipe(pipefd) < 0){
					perror("pipe");
				} else {
					if(cmdv[cmdc-2].out){
						close(pipefd[1]);
					} else {
						cmdv[cmdc-2].out = pipefd[1];
					}
					cmdv[cmdc-1].in = pipefd[0];
				}
				continue;
			}
			destroy_token(tok);

		}
		cmdv[cmdc-1].argc++;
		cmdv[cmdc-1].argv = realloc(cmdv[cmdc-1].argv,sizeof(char *) * cmdv[cmdc-1].argc);
		cmdv[cmdc-1].argv[cmdv[cmdc-1].argc-1] = arg;
	}
finish:
	cmdv[cmdc-1].argv = realloc(cmdv[cmdc-1].argv,sizeof(char *) * (cmdv[cmdc-1].argc + 1));
	cmdv[cmdc-1].argv[cmdv[cmdc-1].argc]= NULL;

	if((flags & TASH_IGN_NL) || (flags & TASH_IGN_EOF)){
ret:
		for(int i=0;i<cmdc;i++){
			for(int j=0;j<cmdv[i].argc;j++){
				free(cmdv[i].argv[j]);
			}
			if(cmdv[i].out)close(cmdv[i].out);
			if(cmdv[i].in)close(cmdv[i].in);
		}
		free(cmdv);
		return 0;
	}

	int to_wait = 0;
	pid_t leader = 0;
	for(int k=0;k<cmdc;k++){
		if(cmdv[k].argc <= 0)continue;

		//check for variable asignement
		if(cmdv[k].argc == 1 && strchr(cmdv[k].argv[0],'=')){
			//TODO : pretty sure this isen't posix compliant
			putvar(cmdv[k].argv[0]);
			goto ret;
		}
		exit_status = check_builtin(cmdv[k].argc,cmdv[k].argv);
		//-1 mean no builtin
		if(exit_status != -1)continue;
		pid_t child = fork();
		if(!child){
			if(cmdv[k].out){
				if(dup2(cmdv[k].out,STDOUT_FILENO) < 0){
					perror("dup2");
				}
			}
			if(cmdv[k].in){
				if(dup2(cmdv[k].in,STDIN_FILENO) < 0){
					perror("dup2");
				}
			}

			//putenv if needed
			int i=0;
			while(strchr(cmdv[k].argv[i],'=')){
				putenv(cmdv[k].argv[i]);
				i++;
				if(i==cmdv[k].argc){
					//TODO : handle this before executing command
					i--;
					break;
				}
			}

			//close all fd as they aren't needed anymore

			for(int i=0;i<cmdc;i++){
				if(cmdv[i].out)close(cmdv[i].out);
				if(cmdv[i].in)close(cmdv[i].in);
			}
			execvp(cmdv[k].argv[i],cmdv[k].argv);
			perror(cmdv[k].argv[i]);
			exit_status = 127;
			_Exit(EXIT_FAILURE);
		}
		if(child < 0){
			perror("fork");
			goto ret;
		}
		if(!leader)leader = child;


		if(setpgid(child,leader) < 0){
			perror("setpgid");
		}
		to_wait++;
		
	}
	exit_status = 2;

	//a bit of cleanup so pipes get closed
	for(int i=0;i<cmdc;i++){
		if(cmdv[i].out)close(cmdv[i].out);
		if(cmdv[i].in)close(cmdv[i].in);
		cmdv[i].in = 0;
		cmdv[i].out = 0;
	}

	if(!to_wait)goto ret;

	if(!(flags & TASH_NOPS) && tcsetpgrp(STDIN_FILENO,leader) < 0){
		perror("tcsetpgrp");
	}
	for(int i=0; i<to_wait; i++){
		if(wait(&exit_status) < 0){
			perror("wait");
			exit_status = 2;
			goto ret;
		}
	}
	if(!(flags & TASH_NOPS) && (signal(SIGTTOU,SIG_IGN) == SIG_ERR || tcsetpgrp(STDIN_FILENO,getpid()) < 0 || signal(SIGTTOU,SIG_DFL) == SIG_ERR)){
		perror("tcsetpgrp"); 
	}
	if(WIFSIGNALED(exit_status)){
		printf("terminated on %s\n",strsignal(WTERMSIG(exit_status)));
	}

	goto ret;
}

void interpret_line(FILE *file){
	//if is_last is set
	//that mean that was the expr of the line
	int is_last = 0;
	while(!is_last){
		interpret_expr(file,&is_last);
	}
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
