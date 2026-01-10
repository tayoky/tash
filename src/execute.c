#include <sys/wait.h>
#include <unistd.h>
#include <tsh.h>

int exit_status = 0;
pid_t current_group = 0;

#define FLAG_NO_FORK 0x01

static void set_exit_status(int status) {
	if (WIFEXITED(status)) {
		exit_status = WEXITSTATUS(status);
	} else if(WIFSIGNALED(status)) {
		exit_status = WTERMSIG(status) + 128;
	}
}

static void free_args(char **args) {
	char **arg = args;
	while (*arg) {
		free(*arg);
		arg++;
	}
	free(args);
}

static int args_count(char **args) {
	int argc = 0;
	while (*args) {
		argc++;
		args++;
	}
	return argc;
}

static void execute_cmd(node_t *node, int in_fd, int out_fd, int flags) {
	char **args = word_expansion(node->cmd.args, node->cmd.args_count);
	if (!args) {
		// expansion error
		exit_status = 1;
		return;
	}
	if (!*args) {
		// empty command
		// somtimes used to create files
		exit_status = 0;
		return;
	}
	int status;
	if ((status = try_builtin(args_count(args), args)) >= 0) {
		exit_status = status;
		free_args(args);
		return;
	}
	if (!(flags & FLAG_NO_FORK)) {
		pid_t child = fork();
		if (child) {
			free_args(args);
			waitpid(child, &status, 0);
			set_exit_status(status);
			return;
		}
	}
	execvp(args[0], args);
	perror(args[0]);
	free_args(args);
	return;
}

static void execute_pipe(node_t *node, int in_fd, int out_fd, int flags) {
	int pipefd[2];
	pipe(pipefd);
}

void execute(node_t *node, int in_fd, int out_fd, int flags) {
	if (!node) return;
	switch (node->type) {
	case NODE_CMD:
		execute_cmd(node, in_fd, out_fd, flags);
		break;
	case NODE_SEP:
		execute(node->binary.left , in_fd, out_fd, flags);
		execute(node->binary.right, in_fd, out_fd, flags);
		break;
	case NODE_AND:
		execute(node->binary.left, in_fd, out_fd, flags);
		if (exit_status == 0) execute(node->binary.right, in_fd, out_fd, flags);
		break;
	case NODE_OR:
		execute(node->binary.left, in_fd, out_fd, flags);
		if (exit_status != 0) execute(node->binary.right, in_fd, out_fd, flags);
		break;
	case NODE_IF:
		exit_status = 0;
		execute(node->_if.condition, in_fd, out_fd, flags);
		if (exit_status == 0) {
			execute(node->_if.body, in_fd, out_fd, flags);
		} else {
			execute(node->_if.else_body, in_fd, out_fd, flags);
		}
		break;
	case NODE_WHILE:
		for (;;) {
			exit_status = 0;
			execute(node->loop.condition, in_fd, out_fd, flags);
			if (exit_status != 0) break;
			execute(node->loop.body, in_fd, out_fd, flags);
		}
		break;
	case NODE_UNTIL:
		for (;;) {
			exit_status = 0;
			execute(node->loop.condition, in_fd, out_fd, flags);
			if (exit_status == 0) break;
			execute(node->loop.body, in_fd, out_fd, flags);
		}
		break;
	case NODE_GROUP:
		execute(node->single.child, in_fd, out_fd, flags);
		break;
	}
}
