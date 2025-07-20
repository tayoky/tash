#include <stdio.h>
#include <unistd.h>
#include "tsh.h"

int flags;
int _argc;
char **_argv;

#define OPT(name,f) if(!strcmp(name,opt)){\
	flags |= f;\
	continue;\
}

int buf_getc(unsigned char **buf){
	int c = **buf;
	if(!c)return EOF;
	(*buf)++;
	return c;
}

void buf_unget(int c,char **buf){
	if(c == EOF)return;
	(*buf)--;
}

int main(int argc,char **argv){
	//parse args and set flags
	flags = 0 ;
	//by default only $0 with shell path
	_argc = argc > 0 ? 1 : 0;
	_argv = argv;
	if(argc > 0 && argv[0][0] == '-'){
		flags |= TASH_LOGIN;
	}

	int i = 1;

	for(;i<argc; i++){
		if(argv[i][0] != '-' || argv[i][1] == 'c')break;
		char *opt = argv[i] + 1;
		if(*opt == '-'){
			opt++;
			OPT("login",TASH_LOGIN);
			OPT("interactive",TASH_INTERACTIVE);
			continue;
		}
		while(*opt){
			switch(*opt){
			case 'l':
				flags |= TASH_LOGIN;
				break;
			case 'i':
				flags |= TASH_INTERACTIVE;
				break;
			default:
				error("unknow option %c (see --help)",*opt);
				return 1;
			}
			opt++;
		}
	}
	if(argc - i  >= 1 && !strcmp(argv[i],"-c")){
		//shell launched with -c
		if(argc - i < 2){
			error("missing command string");
			return 1;
		}
		init();
		source src = {
			.data = &argv[i+1],
			.getc = (void*)buf_getc,
			.unget = (void *)buf_unget,
		};
		
		return interpret(&src);
	} else if(argc - i >= 1){
		//shell launched with script or option
		if(!strcmp(argv[i],"--version")){
			printf("tash %s by tayoky\n",TASH_VERSION);
			return 0;
		}
		//in script mode remove everything before script name from arguement
		_argc = argc - i;
		_argv = &argv[i];
		init();
		FILE *script = fopen(argv[i],"r");
		if(!script){
			perror(argv[i]);
			return 1;
		}
		source src = SRC_FILE(script);
		int ret = interpret(&src);
		fclose(script);
		return ret;
	} else {
		//automatic detection of tty when no script is given
		if(isatty(STDIN_FILENO) == 1){
			flags |= TASH_INTERACTIVE;
		}
		init();
		source src = SRC_FILE(stdin);
		if(flags & TASH_INTERACTIVE){
			src.getc = (void*)prompt_getc;
			src.unget = (void*)prompt_unget;
			src.flags = 0;
		}
		return interpret(&src);
	}
}
