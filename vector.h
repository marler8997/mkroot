struct vector
{
  void **items;
  size_t size;
  size_t capacity;
};

err_t vector_alloc(struct vector *, unsigned capacity);
static inline void vector_free(struct vector *v)
{
  free(v->items);
}
static inline size_t vector_size(struct vector *v)
{
  return v->size;
}
static inline void vector_set(struct vector *v, size_t index, void *item)
{
  v->items[index] = item;
}
static inline void *vector_get(struct vector *v, size_t index)
{
  return v->items[index];
}
err_t vector_add(struct vector *v, void *item);
//void vector_delete(struct vector *, int);

#define DEFINE_TYPED_VECTOR(prefix, type)                               \
  struct prefix##_vector                                                \
  {                                                                     \
    type **items;                                                       \
    size_t size;                                                        \
    size_t capacity;                                                    \
  };                                                                    \
  static inline err_t prefix##_vector_alloc(struct prefix##_vector *v, unsigned capacity) \
  {                                                                     \
    return vector_alloc((struct vector*)v, capacity);                   \
  }                                                                     \
  static inline void prefix##_vector_free(struct prefix##_vector *v)    \
  {                                                                     \
    vector_free((struct vector*)v);                                     \
  }                                                                     \
  static inline size_t prefix##_vector_size(struct prefix##_vector *v)  \
  {                                                                     \
    return vector_size((struct vector*)v);                              \
  }                                                                     \
  static inline void prefix##_vector_set(struct prefix##_vector *v, size_t index, type *item) \
  {                                                                     \
    vector_set((struct vector*)v, index, (void*)item);                  \
  }                                                                     \
  static inline type *prefix##_vector_get(struct prefix##_vector *v, size_t index) \
  {                                                                     \
    return (type*)vector_get((struct vector*)v, index);                 \
  }                                                                     \
  static inline err_t prefix##_vector_add(struct prefix##_vector *v, type *item) \
  {                                                                     \
    return vector_add((struct vector*)v, (void*)item);                  \
  }
