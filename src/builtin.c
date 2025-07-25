#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tsh.h"

extern char **environ;

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

int export(int argc,char **argv){
	int i = 1;
	if(argc < 2 || !strcmp(argv[1],"-p")){
		char ** e = environ;
		while(*e){
			puts(*e);
			e++;
		}
		return 0;
	}

	int ret = 0;
	while(i < argc){
		if(strchr(argv[i],'=')){
			//TODO : remove from local/export if already exist ?
			putenv(argv[i]);
		} else {
			if(!getenv(argv[i])){
				char *val = getvar(argv[i]);
				if(val){
					//TODO : remove from local ?
					putenv(val);
				} else {
					ret = 1;
				}
			}
		}
		i++;
	}
	return ret;
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
			error("cd : HOME not set");
			return 1;
		}
		if(chdir(home) < 0){
			perror(home);
			return 1;
		}
		return 0;
	}
}

int src(int argc,char **argv){
	//TODO : search in PATH first ???
	if(argc < 2){
		error("source : missing argument");
		return 1;
	}

	FILE *script = fopen(argv[1],"r");
	if(!script){
		perror(argv[1]);
		return 1;
	}
	//save argc/argv to restore after
	int old_argc = _argc;
	char **old_argv = _argv;
	_argc = argc - 1;
	_argv = &argv[1];
	source src = SRC_FILE(script);
	interpret(&src);
	fclose(script);
	_argc = old_argc;
	_argv = old_argv;
	return exit_status;
}

#define CMD(n,cmd) {.name = n,.func = (int (*)(int,char**))cmd}
static struct builtin builtin[] = {
	CMD("cd"    ,cd),
	CMD("exit",exit_cmd),
	CMD("export",export),
	CMD("source",src),
	CMD("."     ,src),
};

int check_builtin(int argc,char **argv){
	for(size_t i=0; i<arraylen(builtin);i++){
		if(!strcmp(argv[0],builtin[i].name)){
			return builtin[i].func(argc,argv);
		}
	}

	return -1;
}
