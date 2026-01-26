#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <vector.h>
#include <tsh.h>

int get_char(source_t *src) {
	if (src->unget != EOF) {
		int c = src->unget;
		src->unget = EOF;
		return c;
	}
	return src->get_char(src->data);
}

int unget_char(source_t *src, int c) {
	if (c != EOF) src->unget = c;
	return c;
}

int peek_char(source_t *src) {
	int c = get_char(src);
	unget_char(src, c);
	return c;
}

const char *token_name(token_t *token) {
	switch (token->type) {
	case T_WORD:
		return "<word>";
	case T_NEWLINE:
		return "<newline>";
	case T_EOF:
		return "<eof>";
	case T_BG:
		return "&";
	case T_PIPE:
		return "|";
	case T_SEMI_COLON:
		return ";";
	case T_AND:
		return "&&";
	case T_OR:
		return "||";
	case T_DSEMI:
		return ";;";
	case T_INFERIOR:
		return "<";
	case T_SUPERIOR:
		return ">";
	case T_APPEND:
		return ">>";
	case T_DUP_IN:
		return "<&";
	case T_DUP_OUT:
		return ">&";
	case T_OPEN_PAREN:
		return "(";
	case T_CLOSE_PAREN:
		return ")";
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
	case T_WHILE:
		return "while";
	case T_UNTIL:
		return "until";
	case T_DO:
		return "do";
	case T_DONE:
		return "done";
	case T_IN:
		return "in";
	case T_CASE:
		return "case";
	case T_ESAC:
		return "esac";
	case T_OPEN_BRACES:
		return "{";
	case T_CLOSE_BRACES:
		return "}";
	case T_BANG:
		return "!";
	default:
		return "unknow";
	}
}

static int is_delimiter(int c) {
	if (c == EOF) return 1;
	static char delimiters[] = " \t&|<>()\n;";
	return !!strchr(delimiters, c);
}

#define APPEND(c) vector_push_back(buf, (char[]){c})

static void get_word_helper(source_t *src, vector_t *buf, int *flags, int c, int end) {
	int quote = 0;
	for (;;) {
		if (quote == 0 && is_delimiter(c) && !end) {
			unget_char(src, c);
			return;
		}
		if (c == end) {
			APPEND(c);
			return;
		}
		switch (c) {
		case '\'':
			if (quote == '"') {
				APPEND(CTLESC);
				APPEND(c);
			} else {
				*flags |= WORD_HAS_QUOTE;
				quote = quote == '\'' ? 0 : '\'';
				APPEND(CTLQUOT);
			}
			break;
		case '"':
			if (quote == '\'') {
				APPEND(CTLESC);
				APPEND(c);
			} else {
				quote = quote == '"' ? 0 : '"';
				*flags |= WORD_HAS_QUOTE;
				APPEND(CTLQUOT);
			}
			break;
		case '\\':
			if (quote == '\'') {
				APPEND(c);
				break;
			}
			if (quote == '"') {
				c = peek_char(src);
				if (c == '$' || c == '"' || c == '`' || c == '\\') {
					APPEND('\\');
					break;
				}
			}
			c = get_char(src);
			if (c == '\n' || c == EOF) {
				break;
			}
			APPEND(CTLESC);
			APPEND(c);
			break;
		case '$':
			if (quote == '\'') {
				APPEND(CTLESC);
				APPEND(c);
				break;
			}
			APPEND(c);
			int next_c = peek_char(src);
			if (next_c == '(') {
				c = get_char(src);
				if (c == EOF) return;
				get_word_helper(src, buf, flags, c, ')');
				break;
			}
			if (next_c == '{') {
				c = get_char(src);
				if (c == EOF) return;
				get_word_helper(src, buf, flags, c, '}');
				break;
			}
			// prevent the '*' from being quoted in "$*"
			if (next_c == '*') {
				APPEND(get_char(src));
				break;
			}
			break;
		case '*':
		case ' ':
		case '\t':
		case '\n':
			if (quote) APPEND(CTLESC);
			APPEND(c);
			break;
		case CTLESC:
		case CTLQUOT:
			APPEND(CTLESC);
			APPEND(c);
			break;
		default:
			APPEND(c);
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
	token_t *token = xmalloc(sizeof(token_t));
	token->value   = NULL;
	token->type    = T_NULL;
	token->digit   = -1;
	token->flags   = 0;

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
	case '(':
		token->type = T_OPEN_PAREN;
		return token;
	case ')':
		token->type = T_CLOSE_PAREN;
		return token;
	case '\n':
		token->type = T_NEWLINE;
		return token;
	}

	// operator thay might need peek
	int c2 = peek_char(src);
	if (isdigit(c) && (c2 == '>' || c2 == '<')) {
		token->digit = c - '0';
		c = get_char(src);
		c2 = peek_char(src);
	}
	switch (c) {
	case ';':
		if (c2 == ';') {
			token->type = T_DSEMI;
			get_char(src);
		} else {
			token->type = T_SEMI_COLON;
		}
		return token;
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
	case '<':
		if (c2 == '&') {
			token->type = T_DUP_IN;
			get_char(src);
		} else {
			token->type = T_INFERIOR;
		}
		return token;
	case '>':
		if (c2 == '>') {
			token->type = T_APPEND;
			get_char(src);
		} else if (c2 == '&') {
			token->type = T_DUP_OUT;
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
	vector_t buf = {0};
	init_vector(&buf, sizeof(char));
	get_word_helper(src, &buf, &token->flags, c, 0);
	vector_push_back(&buf, (char[]){'\0'});
	token->value = buf.data;

	// is it a reserved word ?
	char *str = buf.data;
	if (!strcmp(str, "if")) {
		token->type = T_IF;
	} else if (!strcmp(str, "then")) {
		token->type = T_THEN;
	} else if (!strcmp(str, "else")) {
		token->type = T_ELSE;
	} else if (!strcmp(str, "elif")) {
		token->type = T_ELIF;
	} else if (!strcmp(str, "fi")) {
		token->type = T_FI;
	} else if (!strcmp(str, "for")) {
		token->type = T_FOR;
	} else if (!strcmp(str, "in")) {
		token->type = T_IN;
	} else if (!strcmp(str, "do")) {
		token->type = T_DO;
	} else if (!strcmp(str, "done")) {
		token->type = T_DONE;
	} else if (!strcmp(str, "while")) {
		token->type = T_WHILE;
	} else if (!strcmp(str, "until")) {
		token->type = T_UNTIL;
	} else if (!strcmp(str, "for")) {
		token->type = T_FOR;
	} else if (!strcmp(str, "case")) {
		token->type = T_CASE;
	} else if (!strcmp(str, "esac")) {
		token->type = T_ESAC;
	} else if (!strcmp(str, "{")) {
		token->type = T_OPEN_BRACES;
	} else if (!strcmp(str, "}")) {
		token->type = T_CLOSE_BRACES;
	} else if (!strcmp(str, "!")) {
		token->type = T_BANG;
	} else {
		token->type = T_WORD;
	}

	return token;
}

int token_is_word(token_t *token) {
	return token->type >= T_KEYWORD || token->type == T_WORD;
}

void destroy_token(token_t *token) {
	if (!token) return;
	xfree(token->value);
	xfree(token);
}
