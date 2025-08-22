#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "tsh.h"

struct cmd {
	int argc;
	char **argv;
	int in;
	int out;
};

int exit_status ;

#define syntax_error(tok) {src->flags|=TASH_IGN_NL;\
	error("syntax error near token %s",token_name(tok));\
	if(tok->type == T_NEWLINE || tok->type == T_EOF){\
		src->putback = tok;\
	} else {\
		destroy_token(tok);\
	}\
	exit_status = 2;\
	return 0;}

static token *get_token(source *src){
	if(src->putback){
		token *tok = src->putback;
		src->putback = NULL;
		return tok;
	}
	return next_token(src);
}

static void skip_space(source *src){
	token *tok = get_token(src);
	while(tok->type == T_SPACE){
		destroy_token(tok);
		tok = get_token(src);
	}
	src->putback = tok;
}

static void subshell(source *src){
	int old = src->flags;
	src->flags |= TASH_NOPS | TASH_SUBSHELL;
	interpret(src);
	src->flags = old;
}

#define append(s) {const char *__str = s;\
	len += strlen(__str);\
		str = realloc(str,len);\
		strcat(str,__str);}

static char *parse_var(source *src){
	//first check for single digit
	int c = src->getc(src->data);
	if(isdigit(c)){
		c -= '0';
		if(c >= _argc){
			return strdup("");
		} else {
			return strdup(_argv[c]);
		}
	} else {
		src->unget(c,src->data);
	}
	token *tok = get_token(src);
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
		tok = get_token(src);
		int len = 0;
		switch(tok->type){
		case T_HASH:
			len = 1;
			destroy_token(tok);
			tok = get_token(src);
			break;
		default:
			break;
		}
		if(tok->type != T_STR)syntax_error(tok);
		char *val;
		char *end;
		long digit = strtol(tok->value,&end,10);
		if(end != tok->value){
			if(digit >= _argc){
				val = "";
			} else {
				val = _argv[digit];
			}
		} else {
			val = getvar(tok->value);
		}
		destroy_token(tok);
		tok = get_token(src);
		if(tok->type != T_CLOSE_BRACK)syntax_error(tok);
		destroy_token(tok);
		if(len){
			sprintf(tmp,"%zd",val ? strlen(val) : 0);
			return strdup(tmp);
		}
		return strdup(val ? val: "");
	case T_OPEN_PAREN:
		//TODO : this is full of fd leak
		//TODO : if pipe become full ?
		destroy_token(tok);
		int pipefd[2];
		if(pipe(pipefd) < 0){
			perror("pipe");
		}
		int old_out = dup(STDOUT_FILENO);
		dup2(pipefd[1],STDOUT_FILENO);
		subshell(src);
		close(pipefd[1]);
		dup2(old_out,STDOUT_FILENO);
		close(old_out);
		char buf[4096];
		ssize_t size = read(pipefd[0],buf,sizeof(buf)-1);
		if(size < 0){
			perror("read");
			close(pipefd[0]);
			return strdup("");
		}
		buf[size] = '\0';
		close(pipefd[0]);
		return strdup(buf);
	default:
		syntax_error(tok);
	}
}

//TODO : this func need some refactoring
static char **get_strings(source *src,size_t *count){
	*count = 0;
	char **table = malloc(sizeof(char *));
	skip_space(src);
	token *tok = get_token(src);
	char *str = strdup("");
	size_t len = 1;
	int has_str = 0;
	//if start with a string that start with ~
	//tilde completion
	if(tok->type == T_STR && tok->value[0] == '~' && getenv("HOME")){
		append(getenv("HOME"));
		append(tok->value+1);
		destroy_token(tok);
		tok = get_token(src);
		has_str = 1;
	}
	for(;;){
	switch(tok->type){
	case T_SPACE:
		if(!has_str)goto skip;
		goto end;
	case T_STR:
		append(tok->value);
		break;
	case T_QUOTE:
		//continue until colon again
		for(;;){
			destroy_token(tok);
			tok = get_token(src);
			if(tok->type == T_EOF){
				syntax_error(tok);
			}
			if(tok->type == T_QUOTE)break;
			if(!(src->flags & TASH_NOPS) && tok->type == T_NEWLINE)show_ps2();
			const char *name = token2str(tok);
			append(name);
		
		}
		break;
	case T_DQUOTE:
		//continue until double colon again
		for(;;){
			destroy_token(tok);
			tok = get_token(src);
			if(tok->type == T_EOF){
				syntax_error(tok);
			}
			if(tok->type == T_DQUOTE)break;
			if(!(src->flags & TASH_NOPS) && tok->type == T_NEWLINE)show_ps2();
			switch(tok->type){
			case T_DOLLAR:;
				char *val = parse_var(src);
				if(!val)syntax_error(tok);
				append(val);
				free(val);
				continue;
			case T_BACKSLASH:
				destroy_token(tok);
				tok = get_token(src);
				if(tok->type == T_EOF)syntax_error(tok);
				append(token2str(tok));
				continue;
			}

			const char *name = token2str(tok);
			append(name);
		
		}
		break;
	case T_DOLLAR:;
		char *val = parse_var(src);
		if(!val)goto skip;
		if(!strcmp(val,"")){
			free(val);
			goto skip;
		}
		//cut the thing
		size_t i = 0;
		ssize_t index = -1;
		for(;val[i];i++){
			if(val[i] == ' ' || val[i] == '\n'){
				val[i] = '\0';
				if(index >= 0){
					if(index != 0){
						//more than one arg, we need to make list bigger

						(*count)++;
						table = realloc(table,sizeof(char *) * (1 + *count));
						table[*count-1] = str;
						str = strdup(&val[index]);
					} else {
						append(&val[index]);
					}
					index = -1;
				}
			} else if(index == -1){
				index = i;
			}
		}
		if(index >= 0){
			if(index != 0){
				//more than one arg, we need to make list bigger
				(*count)++;
				table = realloc(table,sizeof(char *) * (1 + *count));
				table[*count-1] = str;
				str = strdup(&val[index]);
			} else {
				append(&val[index]);
			}
			index = -1;
		}
		free(val);
		break;
	case T_BACKSLASH:;
		int c = src->getc(src->data);
		if(c == '\r') c = src->getc(src->data);
		if(c == EOF)break;
		if(c == '\n')break;
		static char s[2] = {'\0','\0'};
		s[0] = c;
		append(s);
		break;
	default:
		goto end;
	}
		has_str = 1;
skip:
		destroy_token(tok);
		tok = get_token(src);
	}
end:
	src->putback = tok;
	if(has_str){
		//TODO : make table bigger
		table[*count] = str;
		(*count)++;
	} else {
		free(str);
	}
	if(!*count){
		free(table);
		return NULL;
	}
	return table;
}

static int parse_pipeline(source *src){
	int cmdc = 1;
	struct cmd *cmdv = malloc(sizeof(struct cmd));
	memset(cmdv,0,sizeof(struct cmd));
	for(;;){
		size_t count;
		char **arg = get_strings(src,&count);
		if(arg){
			cmdv[cmdc-1].argv = realloc(cmdv[cmdc-1].argv,sizeof(char *) * (cmdv[cmdc-1].argc + count));
			memcpy(&cmdv[cmdc-1].argv[cmdv[cmdc-1].argc],arg,sizeof(char *) * count);
			cmdv[cmdc-1].argc += count;
			free(arg);
			continue;
		}
		//not a string what it is ?
		token *tok = get_token(src);
		switch(tok->type){
		case T_INFERIOR:
			if(cmdv[cmdc-1].in)close(cmdv[cmdc-1].in);
			destroy_token(tok);
			skip_space(src);
			tok = get_token(src);
			if(tok->type != T_STR)syntax_error(tok);
			cmdv[cmdc-1].in = open(tok->value,O_RDONLY);
			if(cmdv[cmdc-1].in < 0){
				exit_status = 1;
				perror(tok->value);
				src->flags |= TASH_IGN_NL;
			}
			break;
		case T_SUPERIOR:
		case T_APPEND:
			if(cmdv[cmdc-1].out)close(cmdv[cmdc-1].out);
			int oflags = tok->type == T_APPEND ? O_APPEND : O_TRUNC;
			destroy_token(tok);
			skip_space(src);
			tok = get_token(src);
			if(tok->type != T_STR)syntax_error(tok);
			cmdv[cmdc-1].out = open(tok->value,O_WRONLY | O_CREAT | oflags,S_IWUSR | S_IRUSR);
			if(cmdv[cmdc-1].out < 0){
				exit_status = 1;
				perror(tok->value);
				src->flags |= TASH_IGN_NL;
			}
			break;
		case T_PIPE:
			if(cmdv[cmdc-1].argc <= 0)syntax_error(tok);
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
			break;
		default:
			src->putback = tok;
			goto finish;
		}
		destroy_token(tok);
	}

finish:
	if(src->op){
		switch(src->op){
		case T_AND:
			src->op = 0;
			if(exit_status != 0)goto ret;
			break;
		case T_OR:
			src->op = 0;
			if(exit_status == 0)goto ret;
			break;
		}
	}

	cmdv[cmdc-1].argv = realloc(cmdv[cmdc-1].argv,sizeof(char *) * (cmdv[cmdc-1].argc + 1));
	cmdv[cmdc-1].argv[cmdv[cmdc-1].argc]= NULL;

	if((src->flags & TASH_IGN_NL) || (src->flags & TASH_IGN_EOF)){
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
		int assign = 1;
		for(int i=0; i<cmdv[k].argc; i++){
			//FIXME : we should use token instead
			if(!strchr(cmdv[k].argv[i],'=')){
				assign = 0;
				break;
			}
		}
		if(assign){
			for(int i=0; i<cmdv[k].argc; i++){
				char * value = strchr(cmdv[k].argv[i],'=');
				*value = '\0';
				putvar(cmdv[k].argv[i],value+1);
			}
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

			setup_environ();
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

	//a bit of cleanup so pipes get closed
	for(int i=0;i<cmdc;i++){
		if(cmdv[i].out)close(cmdv[i].out);
		if(cmdv[i].in)close(cmdv[i].in);
		cmdv[i].in = 0;
		cmdv[i].out = 0;
	}

	if(!to_wait)goto ret;
	exit_status = 2;

	if(!(src->flags & TASH_NOPS) && tcsetpgrp(STDIN_FILENO,leader) < 0){
		perror("tcsetpgrp");
	}
	for(int i=0; i<to_wait; i++){
		if(wait(&exit_status) < 0){
			perror("wait");
			exit_status = 2;
			goto ret;
		}
	}
	if(!(src->flags & TASH_NOPS) && (signal(SIGTTOU,SIG_IGN) == SIG_ERR || tcsetpgrp(STDIN_FILENO,getpid()) < 0 || signal(SIGTTOU,SIG_DFL) == SIG_ERR)){
		perror("tcsetpgrp"); 
	}
	if(WIFSIGNALED(exit_status)){
		printf("terminated on %s\n",strsignal(WTERMSIG(exit_status)));
	}

	goto ret;
}

static int tok2keyword(const token *tok){
	if(tok->type != T_STR){
		return 0;
	} else if(!strcmp(tok->value,"if")){
		return KEYWORD_IF;
	} else if(!strcmp(tok->value,"else")){
		return KEYWORD_ELSE;
	} else if(!strcmp(tok->value,"elif")){
		return KEYWORD_ELIF;
	} else if(!strcmp(tok->value,"fi")){
		return KEYWORD_FI;
	} else if(!strcmp(tok->value,"then")){
		return KEYWORD_THEN;
	} else if(!strcmp(tok->value,"for")){
		return KEYWORD_FOR;
	} else if(!strcmp(tok->value,"in")){
		return KEYWORD_IN;
	} else if(!strcmp(tok->value,"do")){
		return KEYWORD_DO;
	} else if(!strcmp(tok->value,"done")){
		return KEYWORD_DONE;
	} else if(!strcmp(tok->value,"while")){
		return KEYWORD_WHILE;
	} else if(!strcmp(tok->value,"until")){
		return KEYWORD_UNTIL;
	} else if(!strcmp(tok->value,"case")){
		return KEYWORD_CASE;
	} else if(!strcmp(tok->value,"esac")){
		return KEYWOED_ESAC;
	} else {
		return 0;
	}
}

//return 1 if good
static int expect_keyword(source *src,int type){
	token *tok = get_token(src);
	if(tok2keyword(tok) != type){
		syntax_error(tok);
	}
	return 1;
}


#define GETBUF() buf  = get_buffer(src);\
	if(!buf){\
		free(str);\
		return NULL;\
	}\
	append(buf);\
	free(buf);
//return a buffer that contain everything until next keyword
static char *get_buffer(source *src){
	char *str = strdup("");
	size_t len = 1;
	int prev_is_sep = 1;
	for(;;){
		token *tok = get_token(src);
		//puts(token2str(tok));
		if(tok->type == T_SPACE){
			append(token2str(tok));
			destroy_token(tok);
			continue;
		}
		if(prev_is_sep){
			prev_is_sep = 0;
			char *buf;
			switch(tok2keyword(tok)){
			case 0:
				goto cont;
			case KEYWORD_WHILE:
			case KEYWORD_UNTIL:
				append(token2str(tok));
				GETBUF()
				if(!expect_keyword(src,KEYWORD_DO)){
					free(str);
					return NULL;
				}
				append("do");
				GETBUF()
				if(!expect_keyword(src,KEYWORD_DONE)){
					free(str);
					return NULL;
				}
				append("done");
				break;
			case KEYWORD_IF:
				append(token2str(tok));
				GETBUF()
				if(!expect_keyword(src,KEYWORD_THEN)){
					free(str);
					return NULL;
				}
				append("then");
				//we will already do syntax when interpreting the if
				//so just continue unti fi
				for(;;){
					GETBUF()
					token *t = get_token(src);
					append(token2str(t));
					if(tok2keyword(t) == KEYWORD_FI){
						destroy_token(t);
						break;
					}
					destroy_token(t);

				}
				break;
			default:
				src->putback = tok;
				goto finish;
			}
finish_keyword:
			destroy_token(tok);
			continue;
		}
cont:
		append(token2str(tok));
		if(tok->type == T_NEWLINE || tok->type == T_SEMI_COLON){
			prev_is_sep = 1;
		}
		if(tok->type == T_NEWLINE && !(src->flags & TASH_NOPS)){
			show_ps2();
		}
		destroy_token(tok);
	}
finish:
	return str;
}

//TODO : error handling for NULL buffer in if
static int interpret_expr(source *src,int *is_last){
	*is_last = 0;
	skip_space(src);
	token *first = get_token(src);
	switch(tok2keyword(first)){
		char *condition;
		char *content;
	case 0:
		src->putback = first;
		break;
	case KEYWORD_WHILE:
	case KEYWORD_UNTIL:
		condition = get_buffer(src);
		if(!expect_keyword(src,KEYWORD_DO)){
			free(condition);
			destroy_token(first);
			return 0;
		}
		content = get_buffer(src);
		if(!expect_keyword(src,KEYWORD_DONE)){
			free(condition);
			destroy_token(first);
			return 0;
		}
		if(tok2keyword(first) == KEYWORD_UNTIL){
			while(eval(condition,src->flags | TASH_NOPS) != 0){
				eval(content,src->flags | TASH_NOPS);
			}
		} else {
			while(eval(condition,src->flags | TASH_NOPS) == 0){
				eval(content,src->flags | TASH_NOPS);
			}
		}
		destroy_token(first);
		return 0;
	case KEYWORD_IF:
		destroy_token(first);
		condition = get_buffer(src);
		if(!expect_keyword(src,KEYWORD_THEN)){
			free(condition);
			return 0;
		}
		content = get_buffer(src);
		//printf("condition : '%s'\ncontent : '%s'\n",condition,content);
		if(eval(condition,src->flags | TASH_NOPS) == 0){
			eval(content,src->flags | TASH_NOPS);
		}
		free(condition);
		free(content);
		for(;;){
		token *next = get_token(src);
		switch(tok2keyword(next)){
		case KEYWORD_FI:
			return 0;
		case KEYWORD_ELIF:
			condition = get_buffer(src);
			if(!expect_keyword(src,KEYWORD_THEN)){
				free(condition);
				return 0;
			}
			content = get_buffer(src);
			if(exit_status != 0 && eval(condition,src->flags | TASH_NOPS) == 0){
				eval(content,src->flags | TASH_NOPS);
			}
			free(condition);
			free(content);
			break;
		case KEYWORD_ELSE:
			destroy_token(next);
			content = get_buffer(src);
			if(!expect_keyword(src,KEYWORD_FI)){
				free(content);
				return 0;
			}
			if(exit_status != 0){
				eval(content,src->flags | TASH_NOPS);
			}
			return 0;
		default:
			syntax_error(next);
		}
		destroy_token(next);
		}
	}

	//if a open parenthese we can launch a subshell
	first = get_token(src);
	if(first->type == T_OPEN_PAREN){
		subshell(src);
		return 0;
	} else {
		src->putback = first;
	}

	parse_pipeline(src);

	token * tok = get_token(src);

	//TODO check for empty command ???

	switch(tok->type){
	case T_SEMI_COLON:
		destroy_token(tok);
		break;
	case T_CLOSE_PAREN:
	case T_EOF:
		*is_last = 1;
		src->putback = tok;
		if((src->flags & TASH_IGN_NL) || (src->flags & TASH_IGN_EOF)){
			src->flags &= ~(TASH_IGN_NL | TASH_IGN_EOF);
			}
		break;
	case T_HASH:
		while(tok->type != T_NEWLINE && tok->type != T_EOF){
			destroy_token(tok);
			tok = get_token(src);
		}
		destroy_token(tok);
		break;
	case T_NEWLINE:
		*is_last = 1;
		destroy_token(tok);
		if(src->flags & TASH_IGN_NL){
			src->flags &= ~TASH_IGN_NL;
		}
		break;
	case T_AND:
	case T_OR:
		src->op = tok->type;
		destroy_token(tok);
		//skip all boank lines for multi line
		tok = get_token(src);
		while(tok->type == T_SPACE || tok->type == T_NEWLINE){
			destroy_token(tok);
			tok = get_token(src);
		}
		src->putback = tok;
		break;
	default:
		syntax_error(tok);
	}

	return 0;
}

static void interpret_line(source *src){
	//if is_last is set
	//that mean that was the expr of the line
	int is_last = 0;
	while(!is_last){
		interpret_expr(src,&is_last);
	}
}

int interpret(source *src){
	for(;;){
		if(!(src->flags & TASH_NOPS)){
			show_ps1();
		}
		token *tok = get_token(src);
		if(tok->type == T_EOF || tok->type == T_CLOSE_PAREN){
				if(tok->type == T_CLOSE_PAREN && !(src->flags & TASH_SUBSHELL))syntax_error(tok);
			destroy_token(tok);
			break;
		}
		src->putback = tok;
		interpret_line(src);
	}

	return 0;
}

static int buf_getc(unsigned char **buf){
	int c = **buf;
	if(!c)return EOF;
	(*buf)++;
	return c;
}

static void buf_unget(int c,char **buf){
	if(c == EOF)return;
	(*buf)--;
}

int eval(const char *str,int flags){	
	source src = {
		.data = &str,
		.getc = (void*)buf_getc,
		.unget = (void *)buf_unget,
		.flags = flags,
	};
	interpret(&src);
	return exit_status;
}
