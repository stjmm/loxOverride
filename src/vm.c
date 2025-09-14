#include <stdio.h>

#include "vm.h"
#include "chunk.h"
#include "debug.h"
#include "compiler.h"

vm_t vm;

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
}

void init_vm(void)
{
    reset_stack();
}

void free_vm(void)
{

}

void push(value_t value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

value_t pop(void)
{
    vm.stack_top--;
    return *vm.stack_top;
}

static interpret_result_e run(void)
{
#define READ_BYTE() (*vm.ip++) // Reads byte/instruction and advances
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) // Reads index from bytecode and advances
#define READ_TWO_BYTES() \
    ((uint32_t)(READ_BYTE() | (READ_BYTE() << 8) ))
#define BINARY_OP(op) \
    do { \
        vm.stack_top[-2] = vm.stack_top[-2] op vm.stack_top[-1]; \
        vm.stack_top--; \
       } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("        ");
        for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[");
            print_value(*slot);
            printf("]");
        }
        printf("\n");
        dissasemble_instruction(vm.chunk,
                                (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: {
                value_t constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_CONSTANT_16: {
                uint16_t offset = READ_TWO_BYTES();
                value_t constant = vm.chunk->constants.values[offset];
                push(constant);
                break;
            }
            case OP_ADD:      BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE:   BINARY_OP(/); break;
            case OP_NEGATE:
                vm.stack_top[-1] = -vm.stack_top[-1]; break;
            case OP_RETURN: {
                print_value(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

interpret_result_e interpret(const char *source)
{
    chunk_t chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    interpret_result_e result = run();

    free_chunk(&chunk);
    return result;
}
