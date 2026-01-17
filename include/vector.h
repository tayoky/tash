#ifndef _LIBUTILS_VECTOR_H
#define _LIBUTILS_VECTOR_H

#include <stdlib.h>
#include <string.h>
#include <mem.h>

typedef struct vector {
	size_t count;
	size_t capacity;
	size_t element_size;
	void *data;
} vector_t;

static inline void init_vector(vector_t *vector, size_t element_size){
	if (vector->data) return;
	vector->data = xmalloc(element_size);
	vector->capacity = 1;
	vector->count = 0;
	vector->element_size = element_size;
}

static inline void free_vector(vector_t *vector){
	if (!vector->data) return;
	xfree(vector->data);
	vector->data = NULL;
	vector->count = 0;
}

static inline void *vector_at(vector_t *vector, size_t index){
	return (index < vector->count) ? (char*)vector->data + index * vector->element_size : NULL;
}

static inline void vector_push_back(vector_t *vector, const void *element){
	if (vector->count == vector->capacity) {
		vector->capacity *= 2;
		vector->data = xrealloc(vector->data, vector->capacity * vector->element_size);
	}

	void *ptr = (char*)vector->data + ((vector->count++)  * vector->element_size);
	memcpy(ptr, element, vector->element_size);
}

static inline void vector_push_multiple_back(vector_t *vector, const void *elements, size_t count){
	if (vector->count + count > vector->capacity) {
		while (vector->count + count > vector->capacity) {
			vector->capacity *= 2;
		}
		vector->data = xrealloc(vector->data, vector->capacity * vector->element_size);
	}

	void *ptr = (char*)vector->data + (vector->count * vector->element_size);
	memcpy(ptr, elements, vector->element_size * count);
	vector->count += count;
}

static inline int vector_pop_back(vector_t *vector, void *element){
	if (vector->count == 0) return -1;
		
	void *ptr = (char*)vector->data + ((--vector->count) * vector->element_size);
	if (element) memcpy(element, ptr, vector->element_size);
	return 0;
}
#endif
