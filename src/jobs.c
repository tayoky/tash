#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <vector.h>
#include <stdio.h>
#include <tsh.h>

// process manipulation


void job_report_termination(int status, int bg) {
	if (WIFEXITED(status)) {
		exit_status = WEXITSTATUS(status);
	} else if(WIFSIGNALED(status)) {
		exit_status = WTERMSIG(status) + 128;
		// only interactive shell print messages
		if ((flags & TASH_INTERACTIVE) && !bg && (flags & TASH_JOB_CONTROL)) fprintf(stderr, "terminated on %s\n", strsignal(WTERMSIG(status)));
		if (!bg && WTERMSIG(status) == SIGINT && (flags & TASH_JOB_CONTROL)) {
			sigint_break = 1;
		}
	}
}

void job_control_setup(void) {
	if (flags & TASH_JOB_CONTROL) {
		signal(SIGTTOU, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGINT , SIG_IGN);
	} else {
		signal(SIGTTOU, SIG_DFL);
		signal(SIGTTIN, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);
		signal(SIGINT , SIG_DFL);
	}
}

void job_init_group(group_t *group) {
	group->pid = 0;
	group->childs.data = NULL;
	init_vector(&group->childs, sizeof(pid_t));
}

void job_free_group(group_t *group) {
	free_vector(&group->childs);
}

pid_t job_fork_async(group_t *group) {
	// flush to avoid double flusing
	fflush(NULL);
	pid_t child = fork();
	if (!child) {
		// do not make a new group if already in a new one
		if (flags & TASH_JOB_CONTROL) setpgid(0, group->pid);
		// disable job control and interactive to avoid chaos
		flags &= ~TASH_INTERACTIVE;
		if (flags & TASH_JOB_CONTROL) {
			flags &= ~TASH_JOB_CONTROL;
			job_control_setup();
		}
		return 0;
	}
	if (child < 0) return child;
	if (flags & TASH_JOB_CONTROL) {
		setpgid(child, group->pid);
	}
	if (!group->pid) {
		group->pid = child;
	}
	vector_push_back(&group->childs, &child);
	return child;
}

pid_t job_fork(group_t *group) {
	pid_t child = job_fork_async(group);
	if (child <= 0) return child;
		
	if (group->pid == child) {
		// it's the group leader
		// se we need to setup foreground group
		if (flags & TASH_JOB_CONTROL) {
			tcsetpgrp(STDIN_FILENO, group->pid);
		}
	}
	return child;
}

int job_wait_pid(pid_t pid) {
	int status;
	pid_t child = waitpid(pid, &status, 0);
	if (child < 0) return child;
	job_report_termination(status, 0);
	return 0;
}

int job_wait(group_t *group) {
	int status;
	for (size_t i=0; i<group->childs.count; i++) {;
		waitpid(*(pid_t*)vector_at(&group->childs, i), &status, 0);
		job_report_termination(status, 0);
	}
	if (flags & TASH_JOB_CONTROL) {
		tcsetpgrp(STDIN_FILENO, getpgid(0));
	}
	return 0;
}

int job_single(void) {
	group_t group;
	job_init_group(&group);
	pid_t child = job_fork(&group);
	if (child <= 0) {
		job_free_group(&group);
		return child;
	}
	int ret = job_wait(&group);
	job_free_group(&group);
	return ret == 0 ? 1 : ret;
}
