#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_STRING(value) ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

// Flexible array member allocation
#define ALLOCATE_FAM(type, extra_bytes, object_type) \
    (type*)allocate_object(sizeof(type) + extra_bytes, object_type)

typedef enum {
    OBJ_STRING,
} obj_type_t;

struct obj_t {
    obj_type_t type;
    struct obj_t *next;
};

struct obj_string_t {
    obj_t obj;
    int length;
    char chars[];
};

obj_string_t *allocate_string(const char *chars, int length);
obj_t *allocate_object(size_t size, obj_type_t type);
void print_object(value_t value);

static inline bool is_obj_type(value_t value, obj_type_t type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
