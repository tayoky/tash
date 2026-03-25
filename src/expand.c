#include <ctype.h>
#include <vector.h>
#include <tash.h>

// do word expansion

static char *expand_string_ctl(const char *str, int braces_stop, const char **end);

static int is_dangerous(int c) {
	return c == CTLESC || c == CTLQUOT;
}

static int is_special(int c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '*' || c == '?' || c == '~' || is_dangerous(c);
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

#define APPEND(c) vector_push_back(dest, (char[]){c})

// append a string safely, quoting char if necessary
static void append_safe_len(vector_t *dest, const char *str, size_t len, int in_quote) {
	while (len > 0) {
		if ((in_quote && is_special(*str)) || is_dangerous(*str)) APPEND(CTLESC);
		vector_push_back(dest, str);
		str++;
		len--;
	}
}

static void append_safe(vector_t *dest, const char *str, int in_quote) {
	return append_safe_len(dest, str, strlen(str), in_quote);
}

static int execute_subshell(vector_t *dest, int in_quote, node_t *node) {
#ifdef HAVE_PIPE
	int pipefd[2];
	if (pipe(pipefd) < 0) {
		perror("pipe");
		return -1;
	}
	
	group_t group;
	job_init_group(&group);
	if (job_fork(&group)) {
		// we are the parent
		close(pipefd[1]);

		// collect child output
		char buf[4096];
		ssize_t r;
		size_t nl_count = 0;
		while ((r = read(pipefd[0], buf, sizeof(buf))) > 0) {
			// we need to bring back newlines we have stripped before
			for (size_t i=0; i<nl_count; i++) {
				append_safe(dest, "\n", in_quote);
			}

			// strip newlines
			nl_count = 0;
			char *nl = buf + r - 1;
			while (nl >= buf && *nl == '\n') {
				r--;
				nl--;
				nl_count++;
			}
			append_safe_len(dest, buf, r, in_quote);
		}

		job_wait(&group);
		job_free_group(&group);
		close(pipefd[0]);
		return 0;
	}
	// we are the subshell
	job_free_group(&group);
	close(pipefd[0]);
	if (pipefd[1] != STDOUT_FILENO) {
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
	}
	execute(node, FLAG_NO_FORK);
	exit(exit_status);
#else
	(void)dest;
	(void)in_quote;
	(void)node;
	exit_status = 1;
	error("compiled without pipe support");
#endif
}

static int remove_prefix_suffix(vector_t *dest, int in_quote, const char *value, const char *pattern, int op, int remove_largest) {
	const char *start = value;
	const char *end   = value + strlen(value);
	size_t len = end - start;
	if (op == '%') {
		if (remove_largest) {
			for (const char *cur=value; *cur; cur++) {
				if (glob_match(pattern, cur)) {
					end = cur;
					break;
				}
			}
		} else {
			for (const char *cur=end; cur>=start; cur--) {
				if (glob_match(pattern, cur)) {
					end = cur;
					break;
				}
			}
		}
	} else {
		char *dup = xstrdup(value);
		if (remove_largest) {
			for (size_t cur=len; cur>0; cur--) {
				dup[cur] = '\0';
				if (glob_match(pattern, dup)) {
					start += cur;
					break;
				}
			}
		} else {
			size_t best = 0;
			for (size_t cur=len;; cur--) {
				dup[cur] = '\0';
				if (glob_match(pattern, dup)) {
					best = cur;
				}
				if (cur == 0) break;
			}
			start += best;
		}
		xfree(dup);
	}

	size_t size = end - start;
	append_safe_len(dest, start, size, in_quote);
	return 1;
}


#define ALREADY_HANDLED  1
#define UNSET_VARIABLE   2
#define REFRESH_VARIABLE 3
static int handle_unset_action(vector_t *dest, int in_quote, char *word, const char *var, int op, int is_unset) {
	switch (op) {
	case '-':
		if (is_unset) {
			vector_push_multiple_back(dest, word, strlen(word));
			return ALREADY_HANDLED;
		}
		break;
	case '=':
		if (is_unset) {
			remove_ctlesc(word);
			putvar(var, word);
			const char *value = getvar(var);
			append_safe(dest, value, in_quote);
			return ALREADY_HANDLED;
		}
		break;
	case '?':
		if (is_unset) {
			if (!*word) return UNSET_VARIABLE;
			remove_ctlesc(word);
			error("%s", word);
			return -1;
		}
		break;
	case '+':
		if (!is_unset) {
			vector_push_multiple_back(dest, word, strlen(word));
			return ALREADY_HANDLED;
		}
		break;
	}
	return 0;
}

static int handle_var(vector_t *dest, const char **ptr, int in_quote) {
	const char *src = *ptr;
	if (*src == '(') {
		// we got a subshell
		src++;
		const char *end;
		node_t *node = parse_list_buf(src, &end);
		if (!node) return -1;
		if (*end != ')') {
			error("bad substitution : %.*s", (int)(end - src + 2), src - 2);	
			free_node(node);
			return -1;
		}
		src = end;
		*ptr = src;
		int ret = execute_subshell(dest, in_quote, node);
		free_node(node);
		return ret;
	}
	int has_braces = 0;
	int has_op = 0;
	if (*src == '{') {
		has_braces = 1;
		src++;
		if (*src == '#') {
			has_op = '#';
			src++;
		}
	}
	const char *param = src;
	char buf[32];
	int already_handled = 0;
	const char *value = NULL;
	char *var = NULL;
	switch (*src) {
	case '$':
		sprintf(buf, "%ld", (long)shell_pid);
		value = buf;
		break;
	case '!':
		sprintf(buf, "%ld", (long)last_background);
		value = buf;
		break;
	case '?':
		sprintf(buf, "%d", exit_status);
		value = buf;
		break;
	case '#':
		sprintf(buf, "%d", _argc);
		value = buf;
		break;
	case '@':
		if (has_op == '#') {
			// we want to print len
			sprintf(buf, "%d", _argc);
			append_safe(dest, buf, in_quote);
			already_handled = 1;
			break;
		}

		for (int i=0; i<_argc; i++) {
			append_safe(dest, _argv[i], in_quote);
			if (i != _argc - 1) {
				// do not CTLESC it
				// so it get cut even in quote (which is intended behaviour)
				// it's a special case of posix
				APPEND(' ');
			}
		}
		already_handled = 1;
		break;
	case '*':
		for (int i=0; i<_argc; i++) {
			append_safe(dest, _argv[i], in_quote);
			if (i != _argc - 1) {
				if (in_quote) APPEND(CTLESC);
				APPEND(' ');
			}
		}
		already_handled = 1;
		break;
	case '0':
		value = _argv0;
		break;
	case '1': case '2': case '3':
	case '4': case '5': case '6':
	case '7': case '8': case '9':;
		int index = *src - '1';
		if (index >= _argc) value = NULL;
		else value = _argv[index];
		break;
	default:
		while (isalnum(*src) || *src == '_') {
			src++;
		}
		if (src == param) {
			if (has_braces) {
				goto bad_substitution;
			}
			src--;
			*ptr = src;
			APPEND('$');
			return 0;
		}
		var = xstrndup(param, src - param);
		value = getvar(var);
		src--;
		break;
	}

	const char *param_end = src;
	size_t param_len = param_end - param + 1;

	if (has_braces) {
		src++;
		if (!has_op) {
			if (*src == '#' || *src == '%') {
				has_op = *src;
				src++;
				int remove_largest = 0;
				if (*src == has_op) {
					remove_largest = 1;
					src++;
				}

				// we must ecpand first the pattern given after the operand
				const char *end;
				char *pattern = expand_string_ctl(src, 1, &end);
				src = end;
				if (!pattern) goto error;
				remove_prefix_suffix(dest, in_quote, value, pattern, has_op, remove_largest);
				already_handled = 1;
				xfree(pattern);
			} else {
				int is_unset = !value;
				if (*src == ':') {
					has_op = ':';
					src++;

					// the ":" prefix mean "handle empty like it's unset"
					if (value && !value[0]) {
						is_unset = 1;
					}
				}
				if (*src == '-' || *src == '=' || *src == '?' || *src == '+') {
					has_op = *src;
					src++;
	
					// we must expand first the word given after the operand
					const char *end;
					char *word = expand_string_ctl(src, 1, &end);
					if (!word) goto error;
					src = end;
					if (*src != '}') {
						xfree(word);
						goto bad_substitution;
					}
					int ret = handle_unset_action(dest, in_quote, word, var, has_op, is_unset);
					xfree(word);
					if (ret < 0) goto error;
					if (ret == ALREADY_HANDLED) already_handled = 1;
					if (ret == UNSET_VARIABLE) goto unset_variable;
				} else if (has_op == ':') {
					// we must have something after a ":"
					xfree(var);
					goto bad_substitution;
				}
			}
		}
		if (*src != '}') {
		bad_substitution:
			error("bad substitution");
		error:
			xfree(var);
			return -1;
		}
	}
	xfree(var);
	*ptr = src;
	if (already_handled) {
		return 0;
	}
	if (!value) {
		// var is unset
		if (flags & TASH_UNSET_EXIT) {
unset_variable:
			error("'%.*s' : variable unset", (int)param_len, param);
			return -1;
		}
		return 0;
	}
	if (has_op == '#') {
		sprintf(buf, "%zu", strlen(value));
		value = buf;
	}
	append_safe(dest, value, in_quote);
	return 0;
}

/**
 * @brief handle parameter, braces and tilde expansion
 * @param dest the destination vector/string
 * @param src the source lexed but unexpanded string
 * @param braces_stop stop on right brace
 * @param end set to the first non parsed char
 */
static int first_expansion(vector_t *dest, const char *src, int braces_stop, const char **end) {
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
		} else if (*src == '}' && braces_stop) {
			APPEND('\0');
			if (end) *end = src;
			return 0;
		} else {
			vector_push_back(dest, src);
		}
		src++;

	}
	if (end) *end = src;
	vector_push_back(dest, src);
	return 0;
}

static char *expand_string_ctl(const char *str, int braces_stop, const char **end) {
	// TODO : more expansion
	vector_t v1 = {0};
	vector_t v2 = {0};
	init_vector(&v1, sizeof(char));
	init_vector(&v2, sizeof(char));
	if (first_expansion(&v1, str, braces_stop, end) < 0) goto error;
	free_vector(&v2);
	return v1.data;
error:
	free_vector(&v1);
	free_vector(&v2);
	return NULL;
}

char *expand_word_ctl(word_t *word) {
	return expand_string_ctl(word->text, 0, NULL);
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
		char *expanded = expand_word_ctl(&words[i]);
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
