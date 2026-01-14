#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <tsh.h>

pid_t shell_pid;
pid_t shell_pgid;

static void ex_script(const char *name){
	char path[256];
	struct stat st;

	if (getenv("HOME")) {
		snprintf(path, sizeof(path), "%s/%s", getenv("HOME"), name);
		if (stat(path, &st) >= 0) {
			eval_script(path);
			return;
		}
	}

	snprintf(path, sizeof(path), "/etc/%s", name);
	if (stat(path, &st) >= 0){
		eval_script(path);
	}
}

void init(void){
	shell_pid = getpid();
	shell_pgid = getpgid(0);
	setup_var();

	if (flags & TASH_LOGIN) {
		// source .profile
		ex_script(".profile");
	} else if (flags & TASH_INTERACTIVE) {
		// source .tashrc
		ex_script(".tashrc");
	}

	if (flags & TASH_INTERACTIVE) {
		flags |= TASH_JOB_CONTROL;
	}
}
