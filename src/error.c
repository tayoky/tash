#include <stdarg.h>
#include <stdio.h>
#include <tash.h>

void error(const char *fmt,...){
	va_list args;
	va_start(args,fmt);
	fputs("tash : ",stderr);
	vfprintf(stderr,fmt,args);
	fputc('\n',stderr);
	va_end(args);
}
