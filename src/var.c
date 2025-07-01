#include <stdlib.h>
#include "tsh.h"

char *getvar(const char *name){
	return getenv(name);
}
