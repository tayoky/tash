#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <tsh.h>

int glob_match(const char *glob, const char *str) {
	const char *str_start  = NULL;
	const char *glob_start = NULL;

	while (*glob || *str) {
		switch (*glob) {
		case '\0':
			break;
		case '?':
			if (*str) {
				str++;
				glob++;
				continue;
			}
			break;
		case '*':
			// try to find the seach
			// if not found retry one char later
			glob_start = glob;
			str_start  = str;
			glob++;
			continue;
		case CTLQUOT:
			break;
		case CTLESC:
			glob++;
			// fallthrough
		default:
			if (*str == *glob){
				str++;
				glob++;
				continue;
			}
			break;
		}
		if (str_start && *str_start) {
			glob = glob_start;
			str  = ++str_start;
			continue;
		}
		return 0;
	}
	return 1;
}

static void glob_file_recur(vector_t *found, vector_t *prefix, const char *glob) {
	size_t previous_count = prefix->count;
	// we need to get the prefix the globed part and the suffix
	const char *prefix_start = glob;
	const char *glob_start = glob;
	const char *glob_end = NULL;
	int found_wildcard = 0;
	for (; *glob; glob++) {
		if (*glob == '/') {
			if (found_wildcard) {
				if (!glob_end) {
					glob_end = glob;
				}
			} else {
				glob_start = glob + 1;
			}
		} else if(*glob == CTLESC) {
			if (glob[1] == '*' || glob[1] == '?') glob++;
		} else if (*glob == '*' || *glob == '?') {
			// we found a component with a *
			found_wildcard = 1;
		}
	}
	
	if (!found_wildcard) glob_start = glob;

	// make the full prefix
	for (const char *ptr = prefix_start; ptr < glob_start; ptr++) {
		if (*ptr == CTLQUOT) continue;
		if (*ptr == CTLESC) ptr++;
		vector_push_back(prefix, ptr);
	}

	if (!found_wildcard) {
		struct stat st;
		vector_push_back(prefix, (char[]){'\0'});
		if (stat(prefix->data, &st) >= 0) {
			char *path = xstrdup(prefix->data);
			vector_push_back(found, &path);
		}
		prefix->count = previous_count;
		return;
	}

	if (!glob_end) glob_end = glob;

	char *globing = xstrndup(glob_start, glob_end - glob_start);

	vector_push_back(prefix, (char[]){'\0'});
	// if we have a null prefix we need to open cwd
	DIR *dir = opendir(*(char*)prefix->data ? prefix->data : ".");
	prefix->count--;
	if (!dir) {
		xfree(globing);
		prefix->count = previous_count;
		return;
	}
	for (;;) {
		struct dirent *entry = readdir(dir);
		if (!entry) break;
		// by default ignore entry starting with .
		if (entry->d_name[0] == '.' && globing[0] != '.') continue;
		// always ingore . and ..
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
		if (!glob_match(globing, entry->d_name)) continue;
		size_t prev_count = prefix->count;
		vector_push_multiple_back(prefix, entry->d_name, strlen(entry->d_name));
		glob_file_recur(found, prefix, glob_end);
		prefix->count = prev_count;
	}
	closedir(dir);

	xfree(globing);
	prefix->count = previous_count;
}

char **glob_files(const char *glob) {
	vector_t prefix = {0};
	init_vector(&prefix, sizeof(char));
	vector_t found = {0};
	init_vector(&found, sizeof(char*));

	glob_file_recur(&found, &prefix, glob);

	free_vector(&prefix);
	vector_push_back(&found, (char*[]){NULL});
	return found.data;
}

void destroy_glob_files(char **files) {
	for (char **file = files; *file; file++) {
		xfree(*file);
	}
	xfree(files);
}
