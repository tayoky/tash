#include <stdio.h>
#include "tsh.h"

int main(int argc,char **argv){
	if(argc >= 2){
		FILE *script = fopen(argv[1],"r");
		int ret = interpret(script);
		fclose(script);
		return ret;
	}

	return interpret(stdin);
}
