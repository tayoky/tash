#include <ctype.h>
#include <vector.h>
#include <tsh.h>

// do word expansion

static int is_dangerous(int c) {
	return c == CTLESC || c == CTLQUOT;
}

static int is_special(int c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '*' || c == '~' || is_dangerous(c);
}

#define APPEND(c) vector_push_back(dest, (char[]){c})

static int handle_var(vector_t *dest, const char **ptr, int in_quote) {
	// TODO : bracket support
	const char *start = *ptr;
	const char *src = start;
	const char *value = NULL;
	char buf[32];
	switch (*src) {
	case '$':
		sprintf(buf, "%ld", (long)getpid());
		value = buf;
		break;
	case '?':
		sprintf(buf, "%d", exit_status);
		value = buf;
		break;
	case '#':
		sprintf(buf, "%d", _argc - 1);
		value = buf;
		break;
	// TODO : $@ and $*
	case '0':
	case '1': case '2': case '3':
	case '4': case '5': case '6':
	case '7': case '8': case '9':;
		int index = *src - '0';
		if (index >= _argc) value = NULL;
		value = _argv[index];
		break;
	default:
		while (isalnum(*src) || *src == '_') {
			src++;
		}
		if (src == start) {
			APPEND('$');
			return 0;
		}
		char *var = strndup(start, src - start);
		value = getvar(var);
		free(var);
		src--;
		break;
	}
	*ptr = src;
	if (!value) {
		// var is unset
		return 0;
	}
	while (*value) {
		if ((in_quote && is_special(*value)) || is_dangerous(*value)) APPEND(CTLESC);
		vector_push_back(dest, value);
		value++;
	}
	return 0;
}

static int var_expansion(vector_t *dest, const char *src) {
	int in_quote = 0;
	while (*src) {
		if (*src == CTLESC) {
			vector_push_back(dest, src);
			src++;
			vector_push_back(dest, src);
		} else if (*src == CTLQUOT) {
			in_quote = 1 - in_quote;
		} else if (*src == '$') {
			src++;
			handle_var(dest, &src, in_quote);
		} else {
			vector_push_back(dest, src);
		}
		src++;

	}
	vector_push_back(dest, src);
	return 0;
}

static char *expand_word(word_t *word) {
	// TODO : more expansion
	vector_t v1 = {0};
	vector_t v2 = {0};
	init_vector(&v1, sizeof(char));
	init_vector(&v2, sizeof(char));
	var_expansion(&v1, word->text);
	free_vector(&v2);
	return v1.data;
}

char **word_expansion(word_t *words, size_t words_count) {
	vector_t strings = {0};
	init_vector(&strings, sizeof(char*));
	char *str;

	for (size_t i=0; i<words_count; i++) {
		char *expanded = expand_word(&words[i]);
		// split the field
		vector_t v = {0};
		init_vector(&v, sizeof(char));
		char *ptr = expanded;
		int has_char = 0;
		while (*ptr) {
			switch (*ptr) {
			case CTLESC:
				ptr++;
				vector_push_back(&v, ptr);
				break;
			case CTLQUOT:
				break;
			case ' ':
			case '\t':
			case '\n':
				// split
				if (v.count == 0) break;
				vector_push_back(&v, (char[]){'\0'});
				str = strdup(v.data);
				vector_push_back(&strings, &str);
				v.count = 0;
				break;
			default:
				vector_push_back(&v, ptr);
				break;
			}
			ptr++;
		}
		if (v.count) {
			vector_push_back(&v, (char[]){'\0'});
			str = strdup(v.data);
			vector_push_back(&strings, &str);
		} else if (ptr == expanded && words[i].flags & WORD_HAS_QUOTE) {
			// handle stuff like ""
			str = strdup("");
			vector_push_back(&strings, &str);
		}
		free(expanded);
	}

	str = NULL;
	vector_push_back(&strings, &str);
	return strings.data;
}
