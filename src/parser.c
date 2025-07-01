#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "tsh.h"

token *putback = NULL;

#define syntax_error(tok) {error("syntax error near token %s\n",token_name(tok));return NULL;}

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
				destroy_token(tok);
				syntax_error(tok);
			}
			if(tok->type == T_QUOTE)break;
			if(tok->type == T_NEWLINE)show_ps2();
			const char *name = token2str(tok);
			append(name);
		
		}
		break;
	case T_DOLLAR:
		destroy_token(tok);
		tok = get_token(file);
		char tmp[32];
		switch(tok->type){
		case T_DOLLAR:
#ifdef __stanix__
			sprintf(tmp,"%ld",getpid());
#else
			sprintf(tmp,"%d",getpid());
#endif
			append(tmp);
			break;
		case T_STR:
			append(getvar(tok->value));
			break;
		}
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


int interpret_line(FILE *file){
	int argc = 0;
	char **argv = malloc(0);
	for(;;){
		skip_space(file);
		char *arg = get_string(file);
		if(!arg)break;
		argc++;
		argv = realloc(argv,sizeof(char *) * argc);
		argv[argc-1] = arg;
	}

	//consume a token
	destroy_token(get_token(file));

	argv = realloc(argv,sizeof(char *) * (argc + 1));
	argv[argc] = NULL;

	if(argc > 0){
		pid_t child = fork();
		if(!child){
			execvp(argv[0],argv);
			exit(EXIT_FAILURE);
		}
		waitpid(child,NULL,0);
	}
		
	return 0;
}


int interpret(FILE *file){
	for(;;){
		if(file == stdin){
			show_ps1();
		}
		token *tok = get_token(file);
		if(tok->type == T_EOF)break;
		putback = tok;
		interpret_line(file);
	}

	return 0;
}
