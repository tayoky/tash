#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <stub.h>

// stub for a few functions

#ifndef HAVE_ISATTY
int isatty(int fd) {
#ifdef HAVE_TERMIOS_H
	struct termios term;
	return tcgetattr(fd, &term) >= 0;
#else
	// always assume it's tty
	// this can get weird if we pipe a script into tash
	return 1;
#endif
}
#endif
