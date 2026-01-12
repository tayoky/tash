#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <vector.h>
#include <stdio.h>
#include <tsh.h>

// process manipulation


static void report_termination(int status) {
	if (WIFEXITED(status)) {
		exit_status = WEXITSTATUS(status);
	} else if(WIFSIGNALED(status)) {
		exit_status = WTERMSIG(status) + 127;
		fprintf(stderr, "terminated on %s\n", strsignal(WTERMSIG(status)));
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

pid_t job_fork(group_t *group) {
	pid_t child = fork();
	if (!child) {
		setpgid(0, group->pid);
		return 0;
	}
	if (child < 0) return child;
	if (!group->pid) group->pid = child;
	setpgid(child, group->pid);
	vector_push_back(&group->childs, &child);
	return child;
}

int job_wait(group_t *group) {
	int status;
	for (size_t i=0; i<group->childs.count; i++) {
		waitpid(*(pid_t*)vector_at(&group->childs, i), &status, 0);
		report_termination(status);
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
