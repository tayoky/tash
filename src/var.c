#include <stdlib.h>
#include <stdint.h>
#include "tsh.h"

extern char **environ;

size_t var_count = 0;
struct var *var = NULL;

void putvar(const char *name,const char *value){
	//see if it already exist
	for(size_t i=0; i<var_count; i++){
		if(!strcmp(name,var[i].name)){
			free(var[i].value);
			var[i].value = strdup(value);
			return;
		}
	}
	var = realloc(var,(var_count+1) * sizeof(struct var));
	var[var_count].name     = strdup(name);
	var[var_count].value    = strdup(value);
	var[var_count].exported = 0;
	var_count++;
}

char *getvar(const char *name){
	for(size_t i=0; i<var_count; i++){
		if(!strcmp(name,var[i].name)){
			return var[i].value;
		}
	}
	return NULL;
}

void export_var(const char *name){
	for(size_t i=0; i<var_count; i++){
		if(!strcmp(name,var[i].name)){
			var[i].exported = 1;
			return;
		}
	}
	var = realloc(var,(var_count+1) * sizeof(struct var));
	var[var_count].name     = strdup(name);
	var[var_count].value    = strdup("");
	var[var_count].exported = 1;
	var_count++;
}

void setup_environ(void){
	//FIXME : are we leaking memory
	environ[0] = NULL;
	for(size_t i=0; i<var_count; i++){
		if(!var[i].exported)continue;
		char *env = malloc(strlen(var[i].name) + strlen(var[i].value) + 2);
		sprintf(env,"%s=%s",var[i].name,var[i].value);
		putenv(env);
	}
}

void setup_var(void){
	for(size_t i=0; environ[i]; i++){
		char *name = strdup(environ[i]);
		char *value = strchr(name,'=');
		if(!value){
			error("inavlid environ string '%s'",name);
			free(name);
			continue;
		}
		*value = '\0';
		value++;
		putvar(name,value);
		export_var(name);
		free(name);
	}
}
