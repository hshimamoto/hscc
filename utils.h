#ifndef UTILS_H
#define UTILS_H

typedef struct {
	void **data;
	int capa, len;
} Vector;

extern Vector *new_vector(void);
extern void vec_push(Vector *v, void *e);

typedef struct {
	Vector *keys;
	Vector *vals;
} Map;

extern Map *new_map(void);
extern void map_set(Map *m, char *key, void *val);
extern void *map_get(Map *m, char *key);

#endif // UTILS_H
