#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tsh.h"

int set(int argc,char **argv){
	for(int i=1; i<argc; i++){
		int mask  = 0;
		char *f = argv[i];
		if(!*f)goto invalid;
		f++;
		while(*f){
			switch(*f){
			case 'e':
				mask |= TASH_ERR_EXIT;
				break;
			default:
				goto invalid;
			}
			f++;
		}
		switch(argv[i][0]){
		case '-':
			//src->flags |= mask;
			break;
		case '+':
			//src->flags &= ~mask;
			break;
		default:
			goto invalid;
		}
		continue;
invalid:
		error("set : invalid option : '%s'",argv[i]);
		return 1;
	}
	return 0;
}
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
	if(argc < 2 || !strcmp(argv[1],"-p")){
		for(size_t i=0; i<var_count; i++){
			if(var[i].exported)printf("%s=%s\n",var[i].name,var[i].value);
		}
		return 0;
	}

	int ret = 0;
	int i = 1;
	while(i < argc){
		if(strchr(argv[i],'=')){
			char *name = argv[i];
			char *value = strchr(name,'=');
			*value = '\0';
			value++;
			putvar(name,value);
			export_var(name);
		} else {
			export_var(argv[i]);
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
	source_t src = SRC_FILE(script);
	interpret(&src);
	fclose(script);
	_argc = old_argc;
	_argv = old_argv;
	return exit_status;
}

#define CMD(n,cmd) {.name = n,.func = (int (*)(int,char**))cmd}
static struct builtin builtin[] = {
	CMD("cd"    ,cd),
	CMD("exit"  ,exit_cmd),
	CMD("export",export),
	CMD("source",src),
	CMD("."     ,src),
	CMD("set"   ,set),
};

int check_builtin(int argc,char **argv){
	for(size_t i=0; i<arraylen(builtin);i++){
		if(!strcmp(argv[0],builtin[i].name)){
			return builtin[i].func(argc,argv);
		}
	}

	return -1;
}
