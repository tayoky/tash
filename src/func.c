#include <string.h>
#include <vector.h>
#include <tash.h>

// function manager

static vector_t funcs;

func_t *get_func(const char *name) {
	func_t *func = funcs.data;
	for (size_t i=0; i<funcs.count; i++) {
		if (!strcmp(func[i].name, name)) {
			return &func[i];
		}
	}
	return NULL;
}

void register_func(const char *name, node_t *node) {
	func_t *func = get_func(name);
	if (func) {
		free_node(func->node);
		func->node = node;
		return;
	}
	func_t new_func = {
		.name = xstrdup(name),
		.node = node,
	};
	vector_push_back(&funcs, &new_func);
}

void unregister_func(const char *name) {
	func_t *func = get_func(name);
	if (!func) return;
	if (func != (func_t*)vector_at(&funcs, funcs.count-1)) {
		func_t *last = vector_at(&funcs, funcs.count-1);
		memcpy(func, last, sizeof(func_t));
	}
	vector_pop_back(&funcs, NULL);
}

void setup_funcs(void) {
	init_vector(&funcs, sizeof(func_t));
}
