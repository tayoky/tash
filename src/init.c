#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "tsh.h"

static void ex_script(const char *name){
	//little hack we disable interactive so the prompt don't get showed durring sourcing
	//we need proper prompt desactivation
	int old_flags = flags;
	flags &= ~TASH_INTERACTIVE;

	char path[256];
	snprintf(path,256,"/etc/%s",name);
	FILE *script = fopen(path,"r");
	if(script){
		interpret(script);
		fclose(script);
	}

	if(getenv("HOME")){
		snprintf(path,256,"%s/%s",getenv("HOME"),name);
		script = fopen(path,"r");
		if(script){
			interpret(script);
			fclose(script);
		}
	}

	flags = old_flags;
}

void init(int argc,char **argv){
	(void)argc,(void)argv;
	if(flags & TASH_INTERACTIVE){
		//setup signal control
		if(signal(SIGTTOU,SIG_IGN) == SIG_ERR){
			perror("signal");
		}
	}

	if(flags & TASH_LOGIN){
		//source .profile
		ex_script(".profile");
	} else if(flags & TASH_INTERACTIVE){
		//source .tashrc
		ex_script(".tashrc");
	}
}
