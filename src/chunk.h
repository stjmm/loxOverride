#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "value.h"

// Bytecode operations
typedef enum {
    OP_CONSTANT, // [OP_CODE, CONSTANT INDEX]
    OP_CONSTANT_16, // [OP_CODE, 2 BYTE INDEX]
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_RETURN,
} op_code_e;

// Holds bytecode stuff
typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    value_array_t constants;
} chunk_t;

void init_chunk(chunk_t *chunk);
void write_chunk(chunk_t *chunk, uint8_t byte, int line);
bool write_constant(chunk_t *chunk, value_t value, int lines);
void free_chunk(chunk_t *chunk);
int add_constant(chunk_t *chunk, value_t value);

#endif
