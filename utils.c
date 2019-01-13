#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "utils.h"

int BUFSZ = 32;

Vector *new_vector(void)
{
	Vector *v = malloc(sizeof(Vector));

	v->data = malloc(sizeof(void *) * BUFSZ);
	v->capa = BUFSZ;
	v->len = 0;

	return v;
}

void vec_push(Vector *v, void *e)
{
	if (v->capa >= v->len) {
		v->capa += BUFSZ;
		v->data = realloc(v->data, sizeof(void *) * v->capa);
	}
	v->data[v->len++] = e;
}

Map *new_map(void)
{
	Map *m = malloc(sizeof(Map));

	m->keys = new_vector();
	m->vals = new_vector();

	return m;
}

void map_set(Map *m, char *key, void *val)
{
	for (int i = 0; i < m->keys->len; i++) {
		if (strcmp(m->keys->data[i], key))
			continue;
		m->vals[i].data[i] = val;
		return;
	}
	vec_push(m->keys, key);
	vec_push(m->vals, val);
}

void *map_get(Map *m, char *key)
{
	for (int i = 0; i < m->keys->len; i++) {
		if (strcmp(m->keys->data[i], key))
			continue;
		return m->vals->data[i];
	}
	return NULL;
}
