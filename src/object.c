#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "object.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type, extra_size) \
    (type*)allocate_object(sizeof(type) + (extra_size), object_type)

static obj_t *allocate_object(size_t size, obj_type_e type)
{
    obj_t *object = (obj_t*)reallocate(NULL, 0, size);
    object->type = type;
    object->is_marked = false;

    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

static uint32_t hash_string(const char *key, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

static void print_function(obj_function_t *function)
{
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

obj_closure_t *new_closure(obj_function_t *function)
{
    obj_upvalue_t **upvalues = ALLOCATE(obj_upvalue_t*, function->upvalue_count);
    for (int i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    obj_closure_t *closure = ALLOCATE_OBJ(obj_closure_t, OBJ_CLOSURE, 0);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

obj_upvalue_t *new_upvalue(value_t *slot)
{
    obj_upvalue_t *upvalue = ALLOCATE_OBJ(obj_upvalue_t, OBJ_UPVALUE, 0);
    upvalue->location = slot;
    upvalue->closed = NIL_VAL;
    upvalue->next = NULL;
    return upvalue;
}

obj_native_t *new_native(native_fn function)
{
    obj_native_t *native = ALLOCATE_OBJ(obj_native_t, OBJ_NATIVE, 0);
    native->function = function;
    return native;
}

obj_function_t *new_function(void)
{
    obj_function_t *function = ALLOCATE_OBJ(obj_function_t, OBJ_FUNCTION, 0);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    init_chunk(&function->chunk);
    return function;
}

obj_class_t *new_class(obj_string_t *name)
{
    // klass because c++ compiler
    obj_class_t *klass = ALLOCATE_OBJ(obj_class_t, OBJ_CLASS, 0);
    klass->name = name;
    return klass;
}

obj_string_t *allocate_string(const char *chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;

    obj_string_t *string = ALLOCATE_OBJ(obj_string_t, OBJ_STRING, length + 1);
    string->length = length;
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->hash = hash;

    push(OBJ_VAL(string));
    table_set(&vm.strings, string, NIL_VAL);
    pop();

    return string;
}

obj_string_t *concatenate_strings(obj_string_t *a, obj_string_t *b)
{
    int length = a->length + b->length;

    char *temp_chars = malloc(length + 1);
    memcpy(temp_chars, a->chars, a->length);
    memcpy(temp_chars + a->length, b->chars, b->length);
    temp_chars[length] = '\0';

    obj_string_t *result = allocate_string(temp_chars, length);
    free(temp_chars);

    return result;
}

obj_string_t *number_to_string(double number) {
    char buffer[32]; 
    int length = snprintf(buffer, sizeof(buffer), "%.15g", number);

    return allocate_string(buffer, length);
}

void print_object(value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_FUNCTION:
            print_function(AS_FUNCTION(value));
            break;
        case OBJ_CLOSURE:
            print_function(AS_CLOSURE(value)->function);
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;
    }
}
