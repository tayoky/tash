#include <sys/wait.h>
#include <unistd.h>
#include <tsh.h>

int exit_status = 0;
pid_t current_group = 0;

#define FLAG_IN_CHILD 0x01

static void execute_cmd(node_t *node, int in_fd, int out_fd, int flags) {
	if (!(flags & FLAG_IN_CHILD)) {
		pid_t child = fork();
		if (child) {
			waitpid(child, NULL, 0);
			return;
		}
	}
	char **args = word_expansion(node->cmd.args, node->cmd.args_count);
	execvp(args[0], args);
	perror(args[0]);
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
	}
}
