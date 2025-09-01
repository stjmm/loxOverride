#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"

#define STACK_MAX 256

typedef struct {
    chunk_t *chunk;
    uint8_t *ip;
    value_t stack[STACK_MAX];
    value_t *stack_top;
    obj_t *objects;
} vm_t;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} interpret_result_t;

extern vm_t vm;

void init_vm();
void free_vm();
interpret_result_t interpret(const char *source);
void push(value_t value);
value_t pop();

#endif
