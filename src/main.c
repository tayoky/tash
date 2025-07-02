#include <stdio.h>
#include "tsh.h"

int main(int argc,char **argv){
	init(argc,argv);
	if(argc >= 2){
		if(!strcmp(argv[1],"--version")){
			printf("tash %s by tayoky\n",TASH_VERSION);
			return 0;
		}
		FILE *script = fopen(argv[1],"r");
		if(!script){
			perror(argv[1]);
			return 1;
		}
		int ret = interpret(script);
		fclose(script);
		return ret;
	}

	return interpret(stdin);
}
