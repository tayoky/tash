#include <stdio.h>
#include <unistd.h>
#include "tsh.h"

int flags;

#define OPT(name,f) if(!strcmp(name,opt)){\
	flags |= f;\
	continue;\
}

int main(int argc,char **argv){
	//parse args and set flags
	flags = 0 ;
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
			case 'c':
				flags &= ~TASH_INTERACTIVE;
				break;
			default:
				error("unknow option %c (see --help)",*opt);
				return 1;
			}
			opt++;
		}
	}
	//automatic detection of tty when no script is given
	if(i == argc && isatty(STDIN_FILENO) == 1){
		flags |= TASH_INTERACTIVE;
	}

	init(argc,argv);
	if(argc - i > 0){
		if(!strcmp(argv[i],"--version")){
			printf("tash %s by tayoky\n",TASH_VERSION);
			return 0;
		}
		FILE *script = fopen(argv[i],"r");
		if(!script){
			perror(argv[i]);
			return 1;
		}
		int ret = interpret(script);
		fclose(script);
		return ret;
	}

	return interpret(stdin);
}
