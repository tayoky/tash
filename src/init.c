#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "tsh.h"

//TODO: move this to a proper source builtin
static void ex_script(const char *name){
	char path[256];
	snprintf(path,256,"/etc/%s",name);
	FILE *script = fopen(path,"re");
	if(script){
		source_t src = SRC_FILE(script);
		interpret(&src);
		fclose(script);
	}

	if(getenv("HOME")){
		snprintf(path,256,"%s/%s",getenv("HOME"),name);
		script = fopen(path,"re");
		if(script){
			source_t src = SRC_FILE(script);
			interpret(&src);
			fclose(script);
		}
	}
}

void init(void){
	setup_var();
	return;

	if(flags & TASH_LOGIN){
		//source .profile
		ex_script(".profile");
	} else if(flags & TASH_INTERACTIVE){
		//source .tashrc
		ex_script(".tashrc");
	}
}
