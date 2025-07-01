#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "tsh.h"

void show_ps1(void){
	char cwd[256];
	getcwd(cwd,256);

	char *home = getenv("HOME");
	if(!home){
		home = "/home";
	}

	//remove trailing /
	if(home[0] && home[strlen(home)-1] == '/'){
		home[strlen(home)-1] = '\0';
	}

	if(!memcmp(home,cwd,strlen(home))){
		memmove(&cwd[1],&cwd[strlen(home)],strlen(cwd) - strlen(home) + 1);
		cwd[0] = '~';
	}
	printf("%s $ ",cwd);
}

void show_ps2(void){
	printf("> ");
}
