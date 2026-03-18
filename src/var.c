#include <stdlib.h>
#include <stdint.h>
#include <tash.h>

extern char **environ;

size_t vars_count = 0;
var_t *vars = NULL;

static var_t *var_from_name(const char *name) {
	for (size_t i=0; i<vars_count; i++) {
		if (!strcmp(name,vars[i].name)) {
			return &vars[i];
		}
	}
	return NULL;
}

void putvar(const char *name,const char *value){
	//see if it already exist
	var_t *variable = var_from_name(name);
	if (variable) {
		xfree(variable->value);
		variable->value = xstrdup(value);
		return;
	}

	vars = xrealloc(vars,(vars_count+1) * sizeof(var_t));
	vars[vars_count].name     = xstrdup(name);
	vars[vars_count].value    = xstrdup(value);
	vars[vars_count].exported = 0;
	vars_count++;
}

char *getvar(const char *name){
	var_t *variable = var_from_name(name);
	return variable ? variable->value : NULL;
}

int unset_var(const char *name){
	var_t *variable = var_from_name(name);
	if (!variable) return 0;
	xfree(variable->name);
	xfree(variable->value);

	// do the swap with last trick
	var_t *last = &vars[vars_count-1];
	if (last != variable) {
		memcpy(variable, last, sizeof(var_t));
	}
	vars_count--;
	return 1;
}

void export_var(const char *name){
	var_t *variable = var_from_name(name);
	if (variable) {
		variable->exported = 1;
		return;
	}
	vars = xrealloc(vars,(vars_count+1) * sizeof(var_t));
	vars[vars_count].name     = xstrdup(name);
	vars[vars_count].value    = xstrdup("");
	vars[vars_count].exported = 1;
	vars_count++;
}

void setup_environ(void){
	//FIXME : are we leaking memory?
	environ[0] = NULL;
	for(size_t i=0; i<vars_count; i++){
		if(!vars[i].exported)continue;
		char *env = xmalloc(strlen(vars[i].name) + strlen(vars[i].value) + 2);
		sprintf(env,"%s=%s",vars[i].name,vars[i].value);
		putenv(env);
	}
}

void setup_var(void){
	for(size_t i=0; environ[i]; i++){
		char *name = xstrdup(environ[i]);
		char *value = strchr(name,'=');
		if(!value){
			error("inavlid environ string '%s'",name);
			xfree(name);
			continue;
		}
		*value = '\0';
		value++;
		putvar(name,value);
		export_var(name);
		xfree(name);
	}

	// setup PWD
	char buf[256];
	if (getcwd(buf, sizeof(buf))) {
		putvar("PWD", buf);
	}
	export_var("PWD");
}
