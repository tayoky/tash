#include <ctype.h>
#include <vector.h>
#include <tsh.h>

// do word expansion

static int is_dangerous(int c) {
	return c == ' ' || c == '\t' || c == '\n' || c == CTLESC || c == CTLQUOT || c == '*' || c == '~';
}

#define APPEND(c) vector_push_back(dest, (char[]){c})

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
			// TODO : bracket support
			src++;
			if (*src == '{') {
			} else {
				const char *start = src;
				while (isalnum(*src) || *src == '_') {
					src++;
				}
				if (src == start) {
					APPEND('$');
					continue;
				}
				char *var = strndup(start, src - start);
				const char *value = getvar(var);
				free(var);
				if (!value) continue;
				src--;
				while (*value) {
					if (in_quote && is_dangerous(*value)) APPEND(CTLESC);
					vector_push_back(dest, value);
					value++;
				}
			}
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
