#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static char *get_string(FILE *file){
	token *tok = get_token(file);
	char *str = strdup("");
	size_t len = 1;
	for(;;){
	switch(tok->type){
	case T_STR:
		//append
		len += strlen(tok->value);
		str = realloc(str,len);
		strcat(str,tok->value);
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
			const char *name = token2str(tok);
			len += strlen(name);
			str = realloc(str,len);
			strcat(str,name);
		
		}
		break;
	default:
		goto end;
	}
		destroy_token(tok);
		tok = get_token(file);
	}
end:
	putback = tok;

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


int interpret(FILE *file){
	skip_space(file);
	get_string(file);
	if(putback){
		destroy_token(putback);
	}
	return 0;
}
