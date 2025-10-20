#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)

#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION);
#define IS_CLOSURE(value)  is_obj_type(value, OBJ_CLOSURE);
#define IS_NATIVE(value)   is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value)   is_obj_type(value, OBJ_STRING)

#define AS_FUNCTION(value)((obj_function_t*)AS_OBJ(value))
#define AS_CLOSURE(value) ((obj_closure_t*)AS_OBJ(value))
#define AS_NATIVE(value)  (((obj_native_t*)AS_OBJ(value))->function)
#define AS_STRING(value)  ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
    OBJ_NATIVE,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
} obj_type_e;

struct obj_t {
    obj_type_e type;
    bool is_marked;
    struct obj_t *next;
};

struct obj_string_t {
    obj_t obj;
    int length;
    uint32_t hash;
    char chars[];
};

typedef struct obj_upvalue_t {
    obj_t obj;
    value_t *location;
    value_t closed;
    struct obj_upvalue_t *next;
} obj_upvalue_t;

typedef struct {
    obj_t obj;
    int arity;
    int upvalue_count;
    chunk_t chunk;
    obj_string_t *name;
} obj_function_t;

typedef struct {
    obj_t obj;
    obj_function_t *function;
    obj_upvalue_t **upvalues;
    int upvalue_count;
} obj_closure_t;

typedef value_t (*native_fn)(int arg_count, value_t *args);

typedef struct {
    obj_t obj;
    native_fn function;
} obj_native_t;

static inline bool is_obj_type(value_t value, obj_type_e type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

obj_function_t *new_function(void);
obj_closure_t *new_closure(obj_function_t *function);
obj_upvalue_t *new_upvalue(value_t *slot);
obj_native_t *new_native(native_fn function);
obj_string_t *allocate_string(const char *chars, int length);
obj_string_t *concatenate_strings(obj_string_t *a, obj_string_t *b);
obj_string_t *number_to_string(double number);
void print_object(value_t value);

#endif
