#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "vm.h"
#include "compiler.h"
#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

void *reallocate(void *pointer, size_t old_size, size_t new_size)
{
    if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
        collect_garbage();
#endif
    }
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, new_size);
    if (result == NULL) exit(1);
    return result;
}

void mark_object(obj_t *object)
{
    if (object == NULL) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    print_value(OBJ_VAL(object));
    printf("\n");
#endif
    object->is_marked = true;
}

void mark_value(value_t value)
{
    if (IS_OBJ(value)) mark_object(AS_OBJ(value));
}

static void free_object(obj_t *object)
{
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

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
        case OBJ_CLOSURE: {
            FREE(obj_closure_t, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(obj_upvalue_t, object);
            break;
        case OBJ_NATIVE: {
            obj_native_t *native = (obj_native_t*)object;
            FREE(obj_native_t, native);
            break;
        }
    }
}

static void mark_roots(void)
{
    for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
        mark_value(*slot);
    }

    for (int i = 0; i < vm.frame_count; i++) {
        mark_object((obj_t*)vm.frames[i].closure);
    }

    for (obj_upvalue_t *upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object((obj_t*)upvalue);
    }

    mark_table(&vm.globals);
    mark_compiler_roots();
}

void collect_garbage(void)
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif

    mark_roots();

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
#endif
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
