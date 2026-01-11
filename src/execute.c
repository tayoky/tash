#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector.h>
#include <ctype.h>
#include <tsh.h>

int exit_status = 0;

#define FLAG_NO_FORK 0x01


static void free_args(char **args) {
	char **arg = args;
	while (*arg) {
		xfree(*arg);
		arg++;
	}
	xfree(args);
}

static int args_count(char **args) {
	int argc = 0;
	while (*args) {
		argc++;
		args++;
	}
	return argc;
}

static int apply_assignements(assign_t *assigns, size_t count, int export) {
	for (size_t i=0; i<count; i++) {
		char **val = word_expansion(&assigns[i].value, 1, 0);
		if (!val) {
			exit_status = 1;
			return -1;
		}
		putvar(assigns[i].var, *val);
		if (export) export_var(assigns[i].var);
		free_args(val);
	}
	return 0;
}

static void execute_cmd(node_t *node, int flags) {
	char **args = word_expansion(node->cmd.args, node->cmd.args_count, 1);
	if (!args) {
		// expansion error
		exit_status = 1;
		return;
	}
	if (!*args) {
		// empty command
		// somtimes used to create files or do simple assignements
		free_args(args);
		if (apply_assignements(node->cmd.assigns, node->cmd.assigns_count, 0) < 0) return;
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
		if (job_single()) {
			free_args(args);
			return;
		}
	}
	if (apply_assignements(node->cmd.assigns, node->cmd.assigns_count, 1) < 0) return;
	setup_environ();
	execvp(args[0], args);
	perror(args[0]);
	free_args(args);
	if (!(flags & FLAG_NO_FORK)) exit(1);
	return;
}

static void execute_pipe(node_t *node, int flags) {
	int in = -1;
	group_t group;
	job_init_group(&group);
	while (node->type == NODE_PIPE) {
		node_t *left  = node->binary.left;
		node_t *right = node->binary.right;
		int pipefd[2];
		pipe(pipefd);

		// left child
		pid_t child = job_fork(&group);
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

		if (in >= 0) {
			close(in);
		}
		close(pipefd[1]);
		in = pipefd[0];

		node = right;
	}

	// last child
	pid_t child = job_fork(&group);
	if (!child) {
		dup2(in, STDIN_FILENO);
		close(in);
		execute(node, flags | FLAG_NO_FORK);
		exit(exit_status);
	}
	close(in);

	job_wait(&group);
	job_free_group(&group);
}

static void execute_for(node_t *node, int flags) {
	if ((node->for_loop.var_name.flags & WORD_HAS_QUOTE) || strchr(node->for_loop.var_name.text, CTLESC) || !isalpha(node->for_loop.var_name.text[0])) {
		error("invalid indentifier");
		exit_status = 1;
		return;
	}

	char **strings = word_expansion(node->for_loop.words, node->for_loop.words_count, 1);
	if (!strings) {
		// expansion error
		exit_status = 1;
		return;
	}

	for (char **current = strings; *current; current++) {
		putvar(node->for_loop.var_name.text, *current);
		if (current[1]) {
			execute(node->for_loop.body, flags & ~FLAG_NO_FORK);
		} else {
			execute(node->for_loop.body, flags);
		}
	}
	free_args(strings);
}

void execute(node_t *node, int flags) {
	if (!node) return;
	switch (node->type) {
	case NODE_CMD:
		execute_cmd(node, flags);
		break;
	case NODE_PIPE:
		execute_pipe(node, flags);
		break;
	case NODE_SEP:
		execute(node->binary.left , flags & ~FLAG_NO_FORK);
		execute(node->binary.right, flags);
		break;
	case NODE_AND:
		execute(node->binary.left, flags & ~FLAG_NO_FORK);
		if (exit_status == 0) execute(node->binary.right, flags);
		break;
	case NODE_OR:
		execute(node->binary.left, flags & ~FLAG_NO_FORK);
		if (exit_status != 0) execute(node->binary.right, flags);
		break;
	case NODE_FOR:
		execute_for(node, flags);
		break;
	case NODE_IF:
		exit_status = 0;
		execute(node->_if.condition, flags & ~FLAG_NO_FORK);
		if (exit_status == 0) {
			execute(node->_if.body, flags);
		} else {
			execute(node->_if.else_body, flags);
		}
		break;
	case NODE_WHILE:
		for (;;) {
			exit_status = 0;
			execute(node->loop.condition, flags & ~FLAG_NO_FORK);
			if (exit_status != 0) break;
			execute(node->loop.body, flags & ~FLAG_NO_FORK);
		}
		break;
	case NODE_UNTIL:
		for (;;) {
			exit_status = 0;
			execute(node->loop.condition, flags & ~FLAG_NO_FORK);
			if (exit_status == 0) break;
			execute(node->loop.body, flags & ~FLAG_NO_FORK);
		}
		break;
	case NODE_GROUP:
		execute(node->single.child, flags);
		break;
	case NODE_SUBSHELL:
		if (!(flags & FLAG_NO_FORK)) {
			if (job_single()) {
				break;
			}
		}
		execute(node->single.child, FLAG_NO_FORK);
		if (!(flags & FLAG_NO_FORK)) exit(exit_status);
		break;
	}
}
