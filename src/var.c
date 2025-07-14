#include <stdlib.h>
#include "tsh.h"

static char **var = NULL;
void init_var(void){
	var = malloc(sizeof(char *));
	var[0] = NULL;
}

void *putvar(const char *str){
	//find name lenght
	size_t name_len = (uintptr_t)strchr(str,'=') - (uintptr_t)str + 1;

	//try to find the key
	int key = 0;
	while(var[key]){
		//is it the good key ?
		if(strlen(var[key]) > name_len && !memcmp(var[key],str,name_len)){
			break;
		}
		key++;
	}
	
	if(!var[key]){
		//no key found
		var = realloc(var,(key + 2) * sizeof(char *));

		//set last NULL entry
		var[key + 1] = NULL;
	} else {
		free(var[key]);
	}

	var[key] = strdup(str);
	return 0;

}

char *getvar(const char *name){
	//first look in non exported var
	//this is what we are searching
	char search[strlen(name) + 2];
	strcpy(search,name);
	strcat(search,"=");

	size_t name_len = strlen(search);

	//try to find the key
	int key = 0;
	while(var[key]){
		//is it the good key ?
		if(strlen(var[key]) >= name_len && !memcmp(var[key],search,name_len)){
			break;
		}
		key++;
	}
	if(var[key])return var[key] + name_len;
	
	return getenv(name);
}
