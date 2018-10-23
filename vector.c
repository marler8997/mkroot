#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "vector.h"

static err_t vector_resize(struct vector *v, int new_capacity)
{
  void **new_items = realloc(v->items, sizeof(void*) * new_capacity);
  if (!new_items)
    return err_fail;
  v->items = new_items;
  v->capacity = new_capacity;
  return err_pass;
}
static err_t make_bigger(struct vector *v)
{
  return vector_resize(v, (v->capacity == 0) ? 8 : v->capacity * 2);
}

err_t vector_alloc(struct vector *v, unsigned capacity)
{
  memset(v, 0, sizeof(*v));
  return vector_resize(v, capacity);
}

err_t vector_add(struct vector *v, void *item)
{
  if (v->size == v->capacity) {
    if (make_bigger(v))
      return err_fail;
  }
  v->items[v->size++] = item;
  return err_pass;
}

/*
void vector_delete(struct vector *v, int index)
{
  for (int i = index; i < v->size - 1; i++) {
    v->items[i] = v->items[i + 1];
  }
  v->size--;
}
*/
