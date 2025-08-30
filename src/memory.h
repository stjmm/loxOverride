#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, old_count, new_count) \
    (type*)reallocate(pointer, sizeof(type) *  (old_count), \
        sizeof(type) * (new_count))

#define FREE_ARRAY(type, pointer, old_count) \
    reallocate(pointer, sizeof(type) * (old_count), 0);

#define ENSURE_CAPACITY(array, type, count, capacity)                           \
    do {                                                                        \
        if ((capacity) < (count) + 1) {                                         \
            int old_capacity = (capacity);                                      \
            (capacity) = GROW_CAPACITY(old_capacity);                           \
            (array) = GROW_ARRAY(type, (array), old_capacity, (capacity));      \
        }                                                                       \
    } while (0)

void *reallocate(void *pointer, size_t old_size, size_t new_size);

#endif
