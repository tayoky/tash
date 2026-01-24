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
	const char *src = *ptr;
	int has_braces = 0;
	if (*src == '{') {
		has_braces = 1;
		src++;
	}
	const char *start = src;
	const char *value = NULL;
	char buf[32];
	int must_free = 0;
	switch (*src) {
	case '$':
		sprintf(buf, "%ld", (long)shell_pid);
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
	case '@':
	case '*':;
		// TODO : do the "$@" special case
		vector_t args = {0};
		init_vector(&args, sizeof(char));
		for (int i=1; i<_argc; i++) {
			vector_push_multiple_back(&args, _argv[i], strlen(_argv[i]));
			if (i != _argc - 1) {
				vector_push_back(&args, (char[]){' '});
			}
		}
		vector_push_back(&args, (char[]){'\0'});
		value = args.data;
		must_free = 1;
		break;
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
			if (has_braces) {
				error("bad substitution");
				return -1;
			}
			src--;
			*ptr = src;
			APPEND('$');
			return 0;
		}
		char *var = xstrndup(start, src - start);
		value = getvar(var);
		xfree(var);
		src--;
		break;
	}
	if (has_braces) {
		src++;
		if (*src != '}') {
			error("bad substitution");
			return -1;
		}
	}
	*ptr = src;
	if (!value) {
		// var is unset
		if (flags & TASH_UNSET_EXIT) {
			error("variable unset");
			return -1;
		}
		return 0;
	}
	for (const char *ptr=value; *ptr; ptr++) {
		if ((in_quote && is_special(*ptr)) || is_dangerous(*ptr)) APPEND(CTLESC);
		vector_push_back(dest, ptr);
	}
	if (must_free) {
		xfree((char*)value);
	}
	return 0;
}

// handle parameter, braces and tilde expansion
static int first_expansion(vector_t *dest, const char *src) {
	int in_quote = 0;
	// tilde expansion
	if (*src == '~') {
		src++;
		char *home = getvar("HOME");
		if (!home) home = "/";
		while (*home) {
			if (is_special(*home)) {
				APPEND(CTLESC);
			}
			vector_push_back(dest, home);
			home++;
		}
	}

	// others expansion
	while (*src) {
		if (*src == CTLESC) {
			vector_push_back(dest, src);
			src++;
			vector_push_back(dest, src);
		} else if (*src == CTLQUOT) {
			in_quote = 1 - in_quote;
		} else if (*src == '$') {
			src++;
			if (handle_var(dest, &src, in_quote) < 0) return -1;
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
	if (first_expansion(&v1, word->text) < 0) goto error;
	free_vector(&v2);
	return v1.data;
error:
	free_vector(&v1);
	free_vector(&v2);
	return NULL;
}

static void remove_ctlesc(char *str) {
	char *src = str;
	char *dest = str;
	while (*src) {
		if (*src != CTLESC) *(dest++) = *src;
		src++;
	}
	*dest = '\0';
}

// do a single split on a word and pathname expansion
static void split_word(vector_t *strings, vector_t *v, int *has_globing) {
	char *str;
	vector_push_back(v, (char[]){'\0'});
	if (*has_globing && !(flags & TASH_NO_GLOBING)) {
		char **files = glob_files(v->data);
		if (!*files) {
			xfree(files);
			goto no_globing;
		}
		for (char **file = files; *file; file++) {
			vector_push_back(strings, file);
		}
		xfree(files);
	} else {
no_globing:
		str = xstrdup(v->data);
		// remove CTLESC we don't need them anymore
		remove_ctlesc(str);
		vector_push_back(strings, &str);
	}
	v->count = 0;
	*has_globing = 0;
}

// handle world spliting and pathname expansion
static void word_spliting(word_t *word, vector_t *strings, const char *expanded, int split) {
	vector_t v = {0};
	init_vector(&v, sizeof(char));
	const char *ptr = expanded;
	int has_split = 0;
	int has_globing = 0;
	while (*ptr) {
		switch (*ptr) {
		case CTLESC:
			vector_push_back(&v, ptr);
			ptr++;
			vector_push_back(&v, ptr);
			break;
		case CTLQUOT:
			break;
		case ' ':
		case '\t':
		case '\n':
			// split
			if (!split) {
				vector_push_back(&v, ptr);
				break;
			}
			if (v.count == 0) break;
			split_word(strings, &v, &has_globing);
			has_split = 1;
			break;
		case '*':
		case '?':
			has_globing = 1;
			// fallthrough
		default:
			vector_push_back(&v, ptr);
			break;
		}
		ptr++;
	}
	if (v.count || !split) {
		split_word(strings, &v, &has_globing);
	} else if (!has_split && (word->flags & WORD_HAS_QUOTE)) {
		// handle stuff like ""
		char *str = xstrdup("");
		vector_push_back(strings, &str);
	}
	free_vector(&v);
}


char **word_expansion(word_t *words, size_t words_count, int split) {
	vector_t strings = {0};
	init_vector(&strings, sizeof(char*));

	for (size_t i=0; i<words_count; i++) {
		char *expanded = expand_word(&words[i]);
		if (!expanded) goto error;

		// now do spliting + pathname expansion
		word_spliting(&words[i], &strings, expanded, split);
		xfree(expanded);
	}

	char *str = NULL;
	vector_push_back(&strings, &str);
	return strings.data;
error:
	for (size_t i=0; i<strings.count; i++) {
		xfree(*(char**)vector_at(&strings, i));
	}
	free_vector(&strings);
	return NULL;
}
