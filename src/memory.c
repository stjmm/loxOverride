#include <stdlib.h>
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

void *reallocate(void *pointer, size_t old_size, size_t new_size)
{
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, new_size);
    if (result == NULL) exit(1);
    return result;
}

static void free_object(obj_t *object)
{
    switch (object->type) {
        case OBJ_STRING: {
            obj_string_t *string = (obj_string_t*)object;
            FREE(obj_string_t, string);
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = (obj_function_t*)object;
            free_chunk(&function->chunk);
            FREE(obj_function_t, function);
            break;
        }
        case OBJ_NATIVE: {
            obj_native_t *native = (obj_native_t*)object;
            FREE(obj_native_t, native);
            break;
        }
    }
}

void free_objects(void)
{
    obj_t *object = vm.objects;
    while (object != NULL) {
        obj_t *next = object->next;
        free_object(object);
        object = next;
    }
}
