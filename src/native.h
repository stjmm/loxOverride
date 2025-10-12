#include <stdio.h>
#include <string.h>
#include <time.h>

#include "object.h"

static inline value_t clock_native(int arg_count, value_t *args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static inline value_t input_native(int arg_count, value_t *args)
{
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NIL_VAL;
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    return OBJ_VAL(allocate_string(buffer, (int)len));
}
