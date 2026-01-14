#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector.h>
#include <fcntl.h>
#include <ctype.h>
#include <tsh.h>

typedef struct saved_fd {
	int original;
	int saved;
	int flags;
} saved_fd_t;

int exit_status = 0;
int break_depth = 0;
int continue_depth = 0;
int loop_depth = 0;

#define FLAG_NO_FORK 0x01
#define BREAK_CHECK if (break_depth > 0) {\
	break_depth--;\
	break;\
}
#define CONTINUE_CHECK if (continue_depth > 0) {\
	continue_depth--;\
	if (continue_depth > 0) break;\
	else continue;\
}

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

static int have_fd(int fd) {
	return fcntl(fd, F_GETFD, 0) >= 0;
}

static void flush_fd(int fd) {
	// cannot use a switch
	// cause on some system stdin == stdout
	if (fd == STDIN_FILENO) {
		fflush(stdin);
	} else if (fd == STDOUT_FILENO) {
		fflush(stdout);
	}
}

static int save_fd(int fd, saved_fd_t *saved) {
	saved->original = fd;
	if ((saved->flags = fcntl(fd, F_GETFD, 0)) < 0) {
		perror("save fd");
		return -1;
	}
#ifdef F_DUPFD_CLOEXEC
	if ((saved->saved = fcntl(fd, F_DUPFD_CLOEXEC, 10)) < 0) {
		perror("save fd");
		return -1;
	}
#else
	if ((saved->saved = dup(fd)) < 0) {
		perror("save fd");
		return -1;
	}
	if (fcntl(saved->saved, F_SETFD, FD_CLOEXEC) < 0) {
		perror("save fd");
		close(saved->saved);
		return -1;
	}
#endif
	return 0;
}

static void restore_fd(saved_fd_t *saved) {
	flush_fd(saved->original);
	if (dup2(saved->saved, saved->original) < 0) {
		perror("restore fd");
		close(saved->saved);
		return;
	}
	if (fcntl(saved->original, F_SETFD, saved->flags) < 0) {
		perror("restore fd");
	}
	close(saved->saved);
}

static void restore_fds(vector_t *save) {
	saved_fd_t *saved = save->data;
	for (size_t i=0; i<save->count; i++) {
		restore_fd(&saved[i]);
	}
	free_vector(save);
}

static int apply_redirs(redir_t *redirs, size_t count, vector_t *save) {
	saved_fd_t saved;
	for (size_t i=0; i<count; i++) {
		char **val = word_expansion(&redirs[i].dest, 1, 1);
		if (!val) {
			// expansion error
			goto error;
		}
		if (!val[0] || val[1]) {
			// ambiguous
			error("ambiguous redirection");
			free_args(val);
			goto error;
		}

		int src;
		int src_is_fd = 0;
		if (redirs[i].type & REDIR_DUP) {
			char *end;
			src = strtol(*val, &end, 10);
			src_is_fd = 1;
		} else {
			int flags;
			if (redirs[i].type & REDIR_IN) {
				flags = O_RDONLY;
			} else {
				flags = O_WRONLY | O_CREAT;
				if (redirs[i].type & REDIR_APPEND) {
					flags |= O_APPEND;
				} else {
					flags |= O_TRUNC;
				}
			}
			src = open(*val, flags, 0777);
			if (src < 0) {
				perror(*val);
				free_args(val);
				goto error;
			}
		}

		// save before apply if neccesary
		if (have_fd(redirs[i].fd)) {
			if (save_fd(redirs[i].fd, &saved) < 0) {
				goto error;
			}
			vector_push_back(save, &saved);
		}

		// flush to avoid a bunch of problem
		flush_fd(redirs[i].fd);

		// actually apply redir
		if (dup2(src, redirs[i].fd) < 0) {
			perror(*val);
			if (!src_is_fd) close(src);
			free_args(val);
			goto error;
		}
		if (!src_is_fd) close(src);
		free_args(val);
	}
	return 0;
error:
	restore_fds(save);
	exit_status = 1;
	return -1;
}

static int apply_assignements(assign_t *assigns, size_t count, int export) {
	for (size_t i=0; i<count; i++) {
		char **val = word_expansion(&assigns[i].value, 1, 0);
		if (!val) {
			// expansion error
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

	loop_depth++;
	for (char **current = strings; *current; current++) {
		putvar(node->for_loop.var_name.text, *current);
		if (current[1]) {
			execute(node->for_loop.body, flags & ~FLAG_NO_FORK);
		} else {
			execute(node->for_loop.body, flags);
		}
		BREAK_CHECK
		CONTINUE_CHECK
	}
	loop_depth--;
	free_args(strings);
}

// TODO : stop everything on SIGINT
void execute(node_t *node, int flags) {
	if (!node) return;
	if (break_depth > 0 || continue_depth > 0) return;
	vector_t redirs_save = {0};
	init_vector(&redirs_save, sizeof(saved_fd_t));
	if (apply_redirs(node->redirs, node->redirs_count, &redirs_save) < 0) return;
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
		loop_depth++;
		for (;;) {
			exit_status = 0;
			execute(node->loop.condition, flags & ~FLAG_NO_FORK);
			if (exit_status != 0) break;
			BREAK_CHECK
			CONTINUE_CHECK
			execute(node->loop.body, flags & ~FLAG_NO_FORK);
			BREAK_CHECK
			CONTINUE_CHECK
		}
		loop_depth--;
		break;
	case NODE_UNTIL:
		loop_depth++;
		for (;;) {
			exit_status = 0;
			execute(node->loop.condition, flags & ~FLAG_NO_FORK);
			if (exit_status == 0) break;
			BREAK_CHECK
			CONTINUE_CHECK
			execute(node->loop.body, flags & ~FLAG_NO_FORK);
			BREAK_CHECK
			CONTINUE_CHECK
		}
		loop_depth--;
		break;
	case NODE_NEGATE:
		execute(node->single.child, flags);
		exit_status = !exit_status;
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
	restore_fds(&redirs_save);
}
