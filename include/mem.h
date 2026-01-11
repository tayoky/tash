#ifndef MEM_H
#define MEM_H

#include <stddef.h>


void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *str);
char *xstrndup(const char *str, size_t n);
void xfree(void *ptr);

void xmem_stat(void);
void xmem_reset(void);

#endif
