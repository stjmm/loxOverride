#include "chunk.h"
#include "memory.h"

void init_chunk(chunk_t *chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    init_value_array(&chunk->constants);
}

void free_chunk(chunk_t *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    free_value_array(&chunk->constants);
    init_chunk(chunk);
}

void write_chunk(chunk_t *chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

bool write_constant(chunk_t *chunk, value_t value, int line)
{
    int constant = add_constant(chunk, value);

    if (constant <= UINT8_MAX) {
        write_chunk(chunk, OP_CONSTANT, line);
        write_chunk(chunk, constant, line);

        return true;
    } else if (constant <= UINT16_MAX) {
        // 16-bit index
        write_chunk(chunk, OP_CONSTANT_16, line);
        write_chunk(chunk, (constant >> 0) & 0xFF, line);
        write_chunk(chunk, (constant >> 8) & 0xFF, line);

        return true;
    } else {
        return false;
    }
}

int add_constant(chunk_t *chunk, value_t value)
{
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1;
}
