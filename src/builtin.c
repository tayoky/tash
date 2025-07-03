#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tsh.h"

int exit_cmd(int argc,char **argv){
	if(argc > 2){
		error("exit : too many arguments");
		return 1;
	}
	if(argc == 2){
		char *ptr;
		int status = strtol(argv[1],&ptr,10);
		if(ptr == argv[1]){
			error("exit : numeric argument required");
			exit(2);
		}
		exit(status);
	}

	exit(0);
}

int cd(int argc,char **argv){
	if(argc > 2){
		error("cd :too many arguments");
		return 1;
	}
	if(argc == 2){
		if(chdir(argv[1]) < 0){
			perror(argv[1]);
			return 1;
		}
		return 0;
	} else {
		const char *home = getenv("HOME");
		if(!home){
			error("HOME not set");
			return 1;
		}
		if(chdir(home) < 0){
			perror(home);
			return 1;
		}
		return 0;
	}
}

#define CMD(n,cmd) {.name = n,.func = (int (*)(int,char**))cmd}
static struct builtin builtin[] = {
	CMD("cd",cd),
	CMD("exit",exit_cmd),
};

int check_builtin(int argc,char **argv){
	for(size_t i=0; i<arraylen(builtin);i++){
		if(!strcmp(argv[0],builtin[i].name)){
			return builtin[i].func(argc,argv);
		}
	}

	return -1;
}
