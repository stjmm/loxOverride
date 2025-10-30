#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "value.h"

// Bytecode operations
typedef enum {
    OP_CONSTANT, // [OP_CODE, CONSTANT INDEX]
    OP_CONSTANT_16, // [OP_CODE, 2 BYTE INDEX]
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DUP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_16,
    OP_SET_GLOBAL,
    OP_SET_GLOBAL_16,
    OP_GET_GLOBAL,
    OP_GET_GLOBAL_16,
    OP_SET_UPVALUE,
    OP_GET_UPVALUE,
    OP_SET_PROPERTY,
    OP_GET_PROPERTY,
    OP_GET_SUPER,
    OP_CLOSE_UPVALUE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CLOSURE,
    OP_CALL,
    OP_INVOKE,
    OP_SUPER_INVOKE,
    OP_CLASS,
    OP_INHERIT,
    OP_METHOD,
    OP_ARRAY,
    OP_SET_INDEX,
    OP_GET_INDEX,
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
void free_chunk(chunk_t *chunk);
int add_constant(chunk_t *chunk, value_t value);

#endif
