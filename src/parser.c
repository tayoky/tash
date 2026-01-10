#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector.h>
#include <setjmp.h>
#include <tsh.h>

int parser_error = 0;
static jmp_buf parser_buf;

#define syntax_error(...) parser_error = 1;error(__VA_ARGS__)

static void parse_exit(void) {
	longjmp(parser_buf, 0);
}


static node_t *new_node(int type) {
	node_t *node = malloc(sizeof(node_t));
	memset(node, 0, sizeof(node_t));
	node->type = type;
	return node;
}

void free_node(node_t *node) {
	if (!node) return;
	switch (node->type) {
	case NODE_NEGATE:
		free_node(node->single.child);
		break;
	case NODE_PIPE:
	case NODE_OR:
	case NODE_AND:
	case NODE_SEP:
	case NODE_BG:
		free_node(node->binary.left);
		free_node(node->binary.right);
		break;
	case NODE_IF:
		free_node(node->_if.condition);
		free_node(node->_if.body);
		free_node(node->_if.else_body);
		break;
	case NODE_WHILE:
	case NODE_UNTIL:
		free_node(node->loop.condition);
		free_node(node->loop.body);
		break;
	}
	free(node);
}

static node_t *parse_list(source_t *src, int multi_lines);

static node_t *parse_loop(source_t *src, int type) {
	// we already parsed the while/until
	node_t *condition = parse_list(src, 1);
	if (!condition) return NULL;

	// we need a do
	token_t *token = next_token(src);
	if (token->type == T_DO) {
		destroy_token(token);
	} else {
		// WTF
		free_node(condition);
		syntax_error("unexpected token '%s' (expected 'do')", token_name(token));
		destroy_token(token);
		parse_exit();
	}

	node_t *body = parse_list(src, 1);
	if (!body) {
		free_node(condition);
		return NULL;
	}

	// we need a done
	token = next_token(src);
	if (token->type == T_DONE) {
		destroy_token(token);
	} else {
		// WTF
		free_node(condition);
		free_node(body);
		syntax_error("unexpected token '%s' (expected 'done')", token_name(token));
		destroy_token(token);
		parse_exit();
	}

	node_t *node = new_node(type);
	node->loop.condition = condition;
	node->loop.body      = body;
	return node;
}

static node_t *parse_if(source_t *src) {
	// we already parsed the if
	node_t *condition = parse_list(src, 1);
	if (!condition) return NULL;

	// we need a then
	token_t *token = next_token(src);
	if (token->type == T_THEN) {
		destroy_token(token);
	} else {
		// WTF
		free_node(condition);
		syntax_error("unexpected token '%s' (expected 'then')", token_name(token));
		destroy_token(token);
		parse_exit();
	}

	node_t *body = parse_list(src, 1);
	node_t *else_body = NULL;

	// we need a fi or elif (TODO : else)
	token = next_token(src);
	if (token->type == T_FI) {
		destroy_token(token);
	} else if (token->type == T_ELIF) {
		destroy_token(token);
		else_body = parse_if(src);
		if (!else_body) {
			free_node(body);
			free_node(condition);
			return NULL;
		}
	} else {
		// WTF
		free_node(body);
		free_node(condition);
		syntax_error("unexpected token '%s'", token_name(token));
		destroy_token(token);
		parse_exit();
	}

	node_t *node = new_node(NODE_IF);
	node->_if.condition = condition;
	node->_if.body      = body;
	node->_if.else_body = else_body;
	return node;
}

static void word_from_token(word_t *word, token_t *token) {
	word->text = strdup(token->value);
}

static node_t *parse_simple_command(source_t *src) {
	src->lexer.hint = LEXER_COMMAND;
	token_t *token = next_token(src);
	
	// skip newlines
	while (token->type == T_NEWLINE) {
		destroy_token(token);
		token = next_token(src);
	}
	src->lexer.hint = LEXER_ARGS;

	switch (token->type) {
	case T_IF:
		destroy_token(token);
		return parse_if(src);
	case T_WHILE:
		destroy_token(token);
		return parse_loop(src, NODE_WHILE);
	case T_UNTIL:
		destroy_token(token);
		return parse_loop(src, NODE_UNTIL);
	case T_WORD:
		// regular command
		break;
	default:
		unget_token(src, token);
		return NULL;
	}

	// we have the first string
	word_t word;
	word_from_token(&word, token);
	vector_t args = {0};
	init_vector(&args, sizeof(word_t));
	vector_push_back(&args, &word);

	for (;;) {
		destroy_token(token);
		token = next_token(src);
		switch (token->type) {
		case T_WORD:
			word_from_token(&word, token);
			vector_push_back(&args, &word);
			continue;
		default:
			break;
		}
		break;
	}
	unget_token(src, token);
	// we reached the end of the command
	
	node_t *node = new_node(NODE_CMD);
	node->cmd.args       = args.data;
	node->cmd.args_count = args.count;
	return node;
}

// basicly a pipeline but without a !before it
static node_t *parse_simple_pipeline(source_t *src) {
	node_t *left_cmd = parse_simple_command(src);
	if (!left_cmd) return NULL;

	// are we a pipe ?
	token_t *token = next_token(src);
	if (token->type == T_PIPE) {
		destroy_token(token);
	} else {
		unget_token(src, token);
		return left_cmd;
	}

	node_t *right_cmd = parse_simple_pipeline(src);
	if (!right_cmd) {
		free_node(left_cmd);
		token = next_token(src);
		syntax_error("unexpected token '%s'", token_name(token));
		destroy_token(token);
		parse_exit();
	}
	node_t *node = new_node(NODE_PIPE);
	node->binary.left  = left_cmd;
	node->binary.right = right_cmd;
	return node;
}

static node_t *parse_pipeline(source_t *src) {
	node_t *node = parse_simple_pipeline(src);
	if (node) return node;
	token_t *token = next_token(src);
	if (token->type != T_BANG) {
		unget_token(src, token);
		return NULL;
	}
	destroy_token(token);
	node_t *child = parse_simple_pipeline(src);
	if (!child) {
		token = next_token(src);
		syntax_error("unexpected token '%s'", token_name(token));
		destroy_token(token);
		parse_exit();
	}
	node = new_node(NODE_NEGATE);
	node->single.child = child;
	return node;
}

// parses a list of pipeplines separated by && and ||
static node_t *parse_logic_list(source_t *src) {
	node_t *node = parse_pipeline(src);
	if (!node) return NULL;

	for (;;) {
		token_t *token = next_token(src);
		if (!token) break;
		if (token->type != T_AND && token->type != T_OR) {
			unget_token(src, token);
			break;
		}
		node_t *child = parse_pipeline(src);
		if (!child) {
			destroy_token(token);
			free_node(node);
			token = next_token(src);
			syntax_error("unexpected token '%s'", token_name(token));
			destroy_token(token);
			parse_exit();
		}
		node_t *op_node = new_node(token->type == T_AND ? NODE_AND : NODE_OR);
		op_node->binary.left  = node;
		op_node->binary.right = child;
		node = op_node;
		destroy_token(token);
	}
	return node;
}

static node_t *parse_list(source_t *src, int multi_lines) {
	node_t *node = parse_logic_list(src);
	if (!node) return NULL;

	for (;;) {
		token_t *token = next_token(src);
		if (!token) break;
		if (token->type != T_NEWLINE && token->type != T_SEMI_COLON) {
			unget_token(src, token);
			break;
		}
		if (token->type == T_NEWLINE && !multi_lines) {
			destroy_token(token);
			break;
		}
		node_t *child = parse_logic_list(src);
		if (!child) {
			destroy_token(token);
			token = next_token(src);
			if (token->type == T_THEN || token->type == T_ELIF
					|| token->type == T_ELSE
					|| token->type == T_FI
					|| token->type == T_DO
					|| token->type == T_DONE) {
				unget_token(src, token);
				break;
			}
			free_node(node);
			syntax_error("unexpected token '%s'", token_name(token));
			destroy_token(token);
			parse_exit();
		}
		node_t *sep_node = new_node(token->type == T_BG ? NODE_BG : NODE_SEP);
		sep_node->binary.left  = node;
		sep_node->binary.right = child;
		node = sep_node;
		destroy_token(token);
	}
	return node;
}

static node_t *parse_line(source_t *src) {
	node_t *node = parse_list(src, 0);
	if (node) return node;
	token_t *token = next_token(src);
	if (token->type == T_EOF) {
		destroy_token(token);
	} else {
		syntax_error("unexpected token '%s'", token_name(token));
		destroy_token(token);
		parse_exit();
	}
	return NULL;
}

static void print_depth(int depth) {
	for (int i=0; i<depth; i++) {
		fprintf(stderr, "  ");
	}
}

void print_node(node_t *node, int depth) {
	print_depth(depth);
	switch (node->type) {
	case NODE_CMD:
		fputs("cmd args : \n", stderr);
		for (size_t i=0; i<node->cmd.args_count; i++) {
			print_depth(depth + 1);
			fprintf(stderr, "arg%zu : %s\n", i, node->cmd.args[i].text);
		}
		break;
	case NODE_IF:
		fputs("if\n", stderr);
		print_depth(depth);
		fputs("condition :\n", stderr);
		print_node(node->_if.condition , depth + 1);
		print_depth(depth);
		fputs("body :\n", stderr);
		print_node(node->_if.body , depth + 1);
		if (node->_if.else_body) {
			print_depth(depth);
			fputs("else body :\n", stderr);
			print_node(node->_if.else_body , depth + 1);
		}
		break;
	case NODE_WHILE:
	case NODE_UNTIL:
		fputs(node->type == NODE_WHILE ? "while\n" : "until\n", stderr);

		print_depth(depth);
		fputs("condition :\n", stderr);
		print_node(node->loop.condition , depth + 1);
		print_depth(depth);
		fputs("body :\n",stderr);
		print_node(node->loop.body , depth + 1);
		break;
	case NODE_NEGATE:
		fputs("negate\n", stderr);
		print_node(node->single.child , depth + 1);
		break;
	case NODE_PIPE:
		fputs("pipe\n", stderr);
		print_node(node->binary.left , depth + 1);
		print_node(node->binary.right, depth + 1);
		break;
	case NODE_OR:
		fputs("or\n", stderr);
		print_node(node->binary.left , depth + 1);
		print_node(node->binary.right, depth + 1);
		break;
	case NODE_AND:
		fputs("and\n", stderr);
		print_node(node->binary.left , depth + 1);
		print_node(node->binary.right, depth + 1);
		break;
	case NODE_SEP:
		fputs("sep\n", stderr);
		print_node(node->binary.left , depth + 1);
		print_node(node->binary.right, depth + 1);
		break;
	case NODE_BG:
		fputs("bg\n", stderr);
		print_node(node->binary.left , depth + 1);
		print_node(node->binary.right, depth + 1);
		break;
	}
}



int interpret(source_t *src) {
	if (setjmp(parser_buf)) {
		return 1;
	}
	for (;;) {
		node_t *node = parse_line(src);
		if (!node) {
			// we hit EOF
			return exit_status;
		}
#ifdef DEBUG
		print_node(node, 0);
#endif
		execute(node, STDIN_FILENO, STDOUT_FILENO, 0);
		free_node(node);
	}

	return exit_status;
}

typedef struct buf {
	size_t size;
	size_t ptr;
	char *data;
} buf_t;

static int buf_getc(void *data) {
	buf_t *buf = data;
	if (buf->ptr == buf->size) return EOF;
	return buf->data[buf->ptr++];
}

static int buf_ungetc(int c, void *data) {
	buf_t *buf = data;
	if (c ==  EOF) return EOF;
	buf->ptr--;
	return c;
}

int eval(const char *str) {
	buf_t buf = {
		.size = strlen(str),
		.data = (void*)str,
	};
	source_t src = {
		.get_char   = buf_getc,
		.unget_char = buf_ungetc,
		.data       = &buf,
	};
	return interpret(&src);
}
