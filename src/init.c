#include <stdio.h>
#include <signal.h>
#include "tsh.h"

void init(int argc,char **argv){
	(void)argc,(void)argv;
	//setup signal control
	if(signal(SIGTTOU,SIG_IGN) == SIG_ERR){
		perror("signal");
	}
}
