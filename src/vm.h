#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "object.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    obj_closure_t *closure; // Which function is executed
    uint8_t *ip; // Instruction pointer to that function chunk
    value_t *slots; // Where this functions local variables start in the vm stack
} call_frame_t;

typedef struct {
    call_frame_t frames[FRAMES_MAX];
    int frame_count;
    value_t stack[STACK_MAX]; // Stack for values (eg. OP_RETURN pops 1)
    value_t *stack_top; // Points to first empty stack element
    table_t globals;
    table_t strings; // Interned strings
    obj_upvalue_t *open_upvalues;
    size_t bytes_allocated;
    size_t next_gc;
    obj_t *objects;
    int gray_count;
    int gray_capacity;
    obj_t **gray_stack;
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
