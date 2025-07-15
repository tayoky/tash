#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include "tsh.h"

struct tok {
	char *str;
	size_t len;
	int type;
};

#define TOK(t,n) {.type = t,.str = n,.len = sizeof(n)-1}

//must be from bigger to smaller
struct tok operators[]={
	TOK(T_NEWLINE,"\r\n"),
	TOK(T_AND,"&&"),
	TOK(T_OR,"||"),
	TOK(T_APPEND,">>"),
	TOK(T_BG,"&"),
	TOK(T_PIPE,"|"),
	TOK(T_OPEN_BRACK,"{"),
	TOK(T_CLOSE_BRACK,"}"),
	TOK(T_OPEN_PAREN,"("),
	TOK(T_CLOSE_PAREN,")"),
	TOK(T_SEMI_COLON,";"),
	TOK(T_INFERIOR,"<"),
	TOK(T_SUPERIOR,">"),
	TOK(T_NEWLINE,"\r"),
	TOK(T_NEWLINE,"\n"),
	TOK(T_QUOTE,"'"),
	TOK(T_DQUOTE,"\""),
	TOK(T_HASH,"#"),
	TOK(T_DOLLAR,"$"),
	TOK(T_BACKSLASH,"\\"),
	TOK(T_QUESTION_MARK,"?"),
};

const char *token2str(token *t){
	switch(t->type){
	case T_NEWLINE:
		return "\n";
	case T_STR:
	case T_SPACE:
		return t->value;
	default:
		return token_name(t);
	}
}
const char *token_name(token *t){
	switch(t->type){
	case T_EOF:
		return "<eof>";
	case T_STR:
		return "<string>";
	case T_NEWLINE:
		return "<newline>";
	case T_SPACE:
		return "<space>";
	}

	for(size_t i=0; i<arraylen(operators); i++){
		if(operators[i].type == t->type){
			return operators[i].str;
		}
	}
	return "<unknow>";
}

static int get_operator(FILE *file){
	int c = fgetc(file);
	int best_match = -1;
	size_t size = 1;
	char str[8];
	str[0] = c;
	str[1] = '\0';
	for(size_t i=0; i < arraylen(operators); i++){
		if(size == operators[i].len && !memcmp(str,operators[i].str,operators[i].len)){
			best_match = i;
			//if we find a \n no token as anything after a \n
			//so we can return
			if(c == '\n')return i;
			c = fgetc(file);
			if(c == EOF)return i;
			str[size] = c;
			size++;
			str[size] = '\0';
			i = 0;
		}
	}

	ungetc(c,file);
	return best_match;
}


token *next_token(FILE *file){
	token *new = malloc(sizeof(token));
	memset(new,0,sizeof(token));

	//if aready at the end return EOF
	int c = fgetc(file);
	if(c == EOF){
		new->type = T_EOF;
		return new;
	}

	//if blank just extract every blank
	if(isblank(c)){
		new->type = T_SPACE;
		new->value = strdup("");
		size_t size = 1;

		while(isblank(c)){
			size++;
			new->value = realloc(new->value,size);
			new->value[size-2] = c;
			new->value[size-1] = '\0';
			c = fgetc(file);
		}

		ungetc(c,file);
		return new;
	} else {
		ungetc(c,file);
	}

	int op = get_operator(file);
	if(op < 0){
		new->type = T_STR;
		new->value = strdup("");
		size_t size = 1;
		int c;
		while((c = fgetc(file)) != EOF){
			if(isblank(c))b: break;
			//check if we are at the start of a new op
			for(size_t i=0; i<arraylen(operators); i++){
				if(c == operators[i].str[0])goto b;
			}
			size++;
			new->value = realloc(new->value,size);
			new->value[size-2] = c;
			new->value[size-1] = '\0';
		}
		ungetc(c,file);
	} else {
		new->type = operators[op].type;
	}
	return new;
}

void destroy_token(token *t){
	free(t->value);
	free(t);
}
