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
		if(argv[i][0] != '-')break;
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
		FILE *script = tmpfile();
		fprintf(script,"%s\n",argv[i]);
		i++;
		rewind(script);
		return interpret(script);
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
		int ret = interpret(script);
		fclose(script);
		return ret;
	} else {
		//automatic detection of tty when no script is given
		if(isatty(STDIN_FILENO) == 1){
			flags |= TASH_INTERACTIVE;
		}
		init();
		return interpret(stdin);
	}
}
