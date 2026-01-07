#include <tsh.h>
#include <string.h>
#include <stdlib.h>

int peek_char(source_t *src) {
	int c = src->get_char(src->data);
	src->unget_char(c, src->data);
	return c;
}

int get_char(source_t *src) {
	return src->get_char(src->data);
}

int unget_char(source_t *src, int c) {
	return src->unget_char(c, src->data);
}

const char *token_name(token_t *token) {
	switch (token->type) {
	case T_WORD:
		return "<word>";
	case T_NEWLINE:
		return "<newline>";
	case T_PIPE:
		return "<pipe>";
	case T_AND:
		return "<and>";
	case T_IF:
		return "if";
	case T_THEN:
		return "then";
	case T_ELIF:
		return "elif";
	case T_ELSE:
		return "else";
	case T_FI:
		return "fi";
	default:
		return "unknow";
	}
}

static int is_delimiter(int c) {
	if (c == EOF) return 1;
	static char delimiters[] = " \t&|<>()\n;";
	return !!strchr(delimiters, c);
}

// FIXME : make this memory safe
static void handle_subshell(source_t *src, char *buf, int *i, int is_sub) {
	if (is_sub > 1024) {
		// wow the user is crazy
		return;
	}

	// TODO : handles quotes here
	int quote = 0;
	int c = get_char(src);
	for (;;) {
		if (quote == 0 && is_delimiter(c) && !is_sub) {
			unget_char(src, c);
			return;
		}
		buf[(*i)++] = c;
		switch (c) {
		case '\'':
			if (quote == '"') break;
			quote = quote == '\'' ? 0 : '\'';
			break;
		case '"':
			if (quote == '\'') break;
			quote = quote == '"' ? 0 : '"';
			break;
		case '\\':
			if (quote == '\'') break;
			c = get_char(src);
			if (c == '\n') {
				// ignore backslash and new line
				(*i)--;
				break;
			}
			if (c != EOF) buf[(*i)++] = c;
			break;
		case '$':
			if (quote == '\'') break;
			if (peek_char(src) == '(') {
				handle_subshell(src, buf, i, is_sub + 1);
			}
			break;
		case ')':
			if (quote == 0) return;
			break;
		}
		c = get_char(src);
		if (c == EOF) return;
	}

}

void unget_token(source_t *src, token_t *token) {
	src->lexer.putback = token;
}

token_t *next_token(source_t *src) {
	if (src->lexer.putback) {
		token_t *token = src->lexer.putback;
		src->lexer.putback = NULL;
		return token;
	}
	token_t *token = malloc(sizeof(token_t));
	if (!token) return NULL;
	token->value = NULL;
	token->type  = T_NULL;

	// skip spaces
	int c = get_char(src);
	int in_comment = 0;
	while (c == ' ' || c == '\t' || c == '#' || in_comment) {
		if (c == '#') in_comment = 1;
		c = get_char(src);
		if (c == '\n' && in_comment) in_comment = 0;
	}

	// operator that don't need peek
	switch (c) {
	case EOF:
		token->type = T_EOF;
		return token;
	case '<':
		token->type = T_INFERIOR;
		return token;
	case '(':
		token->type = T_OPEN_PAREN;
		return token;
	case ')':
		token->type = T_CLOSE_PAREN;
		return token;
	case '\n':
		token->type = T_NEWLINE;
		return token;
	case ';':
		token->type = T_SEMI_COLON;
		return token;
	}

	// operator thay might need peek
	int c2 = peek_char(src);
	switch (c) {
	case '&':
		if (c2 == '&') {
			token->type = T_AND;
			get_char(src);
		} else {
			token->type = T_BG;
		}
		return token;
	case '|':
		if (c2 == '|') {
			token->type = T_OR;
			get_char(src);
		} else {
			token->type = T_PIPE;
		}
		return token;
	case '>':
		if (c2 == '>') {
			token->type = T_APPEND;
			get_char(src);
		} else {
			token->type = T_SUPERIOR;
		}
		return token;
	case '\r':
		if (c2 == '\n') get_char(src);
		token->type = T_NEWLINE;
		return token;
	// TODO : do more
	}

	// we have a word
	char buf[512];
	int i=0;
	unget_char(src, c);
	handle_subshell(src, buf, &i, 0);
	buf[i] = '\0';
	token->value = strdup(buf);

	// is it a reserved word ?
	switch (src->lexer.hint) {
	case LEXER_COMMAND:
		if (!strcmp(buf, "if")) {
			token->type = T_IF;
		} else if (!strcmp(buf, "then")) {
			token->type = T_THEN;
		} else if (!strcmp(buf, "else")) {
			token->type = T_ELSE;
		} else if (!strcmp(buf, "elif")) {
			token->type = T_ELIF;
		} else if (!strcmp(buf, "fi")) {
			token->type = T_FI;
		} else if (!strcmp(buf, "for")) {
			token->type = T_FOR;
		} else if (!strcmp(buf, "in")) {
			token->type = T_IN;
		} else if (!strcmp(buf, "do")) {
			token->type = T_DO;
		} else if (!strcmp(buf, "done")) {
			token->type = T_DONE;
		} else if (!strcmp(buf, "while")) {
			token->type = T_WHILE;
		} else if (!strcmp(buf, "until")) {
			token->type = T_UNTIL;
		} else if (!strcmp(buf, "for")) {
			token->type = T_FOR;
		} else if (!strcmp(buf, "case")) {
			token->type = T_CASE;
		} else if (!strcmp(buf, "esac")) {
			token->type = T_ESAC;
		} else if (!strcmp(buf, "{")) {
			token->type = T_OPEN_BRACKET;
		} else if (!strcmp(buf, "}")) {
			token->type = T_CLOSE_BRACKET;
		} else if (!strcmp(buf, "!")) {
			token->type = T_BANG;
		} else {
			token->type = T_WORD;
		}
		break;
	case LEXER_ARGS:
		token->type = T_WORD;
		break;
	case LEXER_FILE:
		token->type = T_WORD;
		break;
	}

	return token;
}

void destroy_token(token_t *token) {
	if (!token) return;
	free(token->value);
	free(token);
}
