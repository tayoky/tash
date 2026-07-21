#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <tash.h>
#include <unistd.h>

pid_t shell_pid;
pid_t shell_pgid;

static void ex_script(const char *path) {
	struct stat st;
	if (stat(path, &st) >= 0) {
		eval_script(path);
	}
}

static void ex_script_home(const char *name) {
	char path[256];
	if (getenv("HOME")) {
		snprintf(path, sizeof(path), "%s/%s", getenv("HOME"), name);
		ex_script(path);
	}
}

void init(void) {
	shell_pid = getpid();
	shell_pgid = getpgid(0);
	setup_var();
	setup_funcs();
	putvar("TASH", tash_cmd);

	job_control_setup();

	if (flags & TASH_LOGIN) {
		// source /etc/profile and .profile
		ex_script(PREFIX"/etc/profile");
		ex_script_home(".profile");
	} else if (flags & TASH_INTERACTIVE) {
		// source .tashrc
		ex_script(PREFIX"/etc/tashrc");
		ex_script_home(".tashrc");
	}

	if (flags & TASH_INTERACTIVE) {
		flags |= TASH_JOB_CONTROL;
		job_control_setup();
	}
}
