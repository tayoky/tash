#include <errno.h>
#include <tsh.h>

// stubs for various functions

#ifndef HAVE_PIPE
int pipe(int pipefd[2]) {
	(void)pipefd;
	errno = ENOSYS;
	return -1;
}
#endif
