#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"

#define STACK_MAX 256

typedef struct {
    chunk_t *chunk;
    uint8_t *ip; // Instruction pointer, points to bytecode instruction
    value_t stack[STACK_MAX]; // Stack for values (eg. OP_RETURN pops 1)
    value_t *stack_top; // Points to first empty stack element
    obj_t *objects;
} vm_t;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} interpret_result_e;

extern vm_t vm;

void init_vm(void);
void free_vm(void);
interpret_result_e interpret(const char *source);
void push(value_t value);
value_t pop(void);


#endif
