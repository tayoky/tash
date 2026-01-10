#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector.h>
#include <tsh.h>

int exit_status = 0;

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

static void execute_cmd(node_t *node, int flags) {
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

static void execute_pipe(node_t *node, int flags) {
	int in = -1;
	vector_t childs = {0};
	init_vector(&childs, sizeof(pid_t));
	while (node->type == NODE_PIPE) {
		node_t *left  = node->binary.left;
		node_t *right = node->binary.right;
		int pipefd[2];
		pipe(pipefd);

		// left child
		pid_t child = fork();
		if (!child) {
			if (in >= 0) {
				dup2(in, STDIN_FILENO);
				close(in);
			}
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[0]);
			close(pipefd[1]);
			execute(left, flags | FLAG_NO_FORK);
			exit(exit_status);
		}
		vector_push_back(&childs, &child);

		if (in >= 0) {
			close(in);
		}
		close(pipefd[1]);
		in = pipefd[0];

		node = right;
	}

	// last child
	pid_t child = fork();
	if (!child) {
		dup2(in, STDIN_FILENO);
		close(in);
		execute(node, flags | FLAG_NO_FORK);
		exit(exit_status);
	}
	close(in);
	vector_push_back(&childs, &child);

	int status;
	for (size_t i=0; i<childs.count; i++) {
		waitpid(-1, &status, 0);
		set_exit_status(status);
	}
}

void execute(node_t *node, int flags) {
	if (!node) return;
	pid_t child;
	switch (node->type) {
	case NODE_CMD:
		execute_cmd(node, flags);
		break;
	case NODE_PIPE:
		execute_pipe(node, flags);
		break;
	case NODE_SEP:
		execute(node->binary.left , flags);
		execute(node->binary.right, flags);
		break;
	case NODE_AND:
		execute(node->binary.left, flags);
		if (exit_status == 0) execute(node->binary.right, flags);
		break;
	case NODE_OR:
		execute(node->binary.left, flags);
		if (exit_status != 0) execute(node->binary.right, flags);
		break;
	case NODE_IF:
		exit_status = 0;
		execute(node->_if.condition, flags);
		if (exit_status == 0) {
			execute(node->_if.body, flags);
		} else {
			execute(node->_if.else_body, flags);
		}
		break;
	case NODE_WHILE:
		for (;;) {
			exit_status = 0;
			execute(node->loop.condition, flags);
			if (exit_status != 0) break;
			execute(node->loop.body, flags);
		}
		break;
	case NODE_UNTIL:
		for (;;) {
			exit_status = 0;
			execute(node->loop.condition, flags);
			if (exit_status == 0) break;
			execute(node->loop.body, flags);
		}
		break;
	case NODE_GROUP:
		execute(node->single.child, flags);
		break;
	case NODE_SUBSHELL:
		child = fork();
		if (!child) {
			execute(node->single.child, flags);
			exit(exit_status);
		} else if (child < 0) {
			perror("fork");
		} else {
			int status;
			waitpid(child, &status, 0);
			exit_status = WEXITSTATUS(status);
		}
		break;
	}
}
