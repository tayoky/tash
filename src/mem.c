#include <stdlib.h>
#include <string.h>
#include <tsh.h>

#ifdef DEBUG
static size_t allocs_count = 0;
void xmem_stat(void) {
	error("leaked %zu allocs", allocs_count);
}
void xmem_reset(void) {
	allocs_count = 0;
}
#else
void xmem_stat(void) {}
void xmem_reset(void) {}
#endif

void *xmalloc(size_t size) {
	if (!size) return NULL;
	void *ptr = malloc(size);
	if (!ptr) {
		error("out of memory");
		abort();
	}
#ifdef DEBUG
	allocs_count++;
#endif
	return ptr;
}

void *xrealloc(void *p, size_t size) {
	if (!size) {
		xfree(p);
		return NULL;
	}
	void *ptr = realloc(p, size);
	if (!ptr) {
		error("out of memory");
		abort();
	}
#ifdef DEBUG
	if (!p) allocs_count++;
#endif
	return ptr;
}

char *xstrdup(const char *str) {
	if (!str) return NULL;
	char *ptr = strdup(str);
	if (!ptr) {
		error("out of memory");
		abort();
	}
#ifdef DEBUG
	allocs_count++;
#endif
	return ptr;
}

char *xstrndup(const char *str, size_t n) {
	if (!str) return NULL;
	char *ptr = strndup(str, n);
	if (!ptr) {
		error("out of memory");
		abort();
	}
#ifdef DEBUG
	allocs_count++;
#endif
	return ptr;
}

void xfree(void *ptr) {
	if (!ptr) return;
	free(ptr);
#ifdef DEBUG
	allocs_count--;
#endif
}
