#include <string.h>
#include <stdio.h>

#include "object.h"
#include "memory.h"
#include "vm.h"

obj_t *allocate_object(size_t size, obj_type_t type)
{
    obj_t *object = (obj_t*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;

    return object;
}

obj_string_t *allocate_string(const char *chars, int length)
{
    obj_string_t *string = ALLOCATE_FAM(obj_string_t, length, OBJ_STRING);
    string->length = length;
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    return string;
}

void print_object(value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
    }
}
