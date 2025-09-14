#include <stdio.h>

#include "value.h"
#include "memory.h"

void init_value_array(value_array_t *array)
{
    array->capacity = 0;
    array->count = 0;
    array->values = NULL;
}

void free_value_array(value_array_t *array)
{
    FREE_ARRAY(value_t, array->values, array->capacity);
    init_value_array(array);
}

void write_value_array(value_array_t *array, value_t value)
{
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(value_t, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void print_value(value_t value)
{
    printf("%g", value);
}
