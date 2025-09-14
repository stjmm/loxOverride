#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "vm.h"
#include "chunk.h"
#include "debug.h"
#include "compiler.h"
#include "value.h"
#include "object.h"
#include "memory.h"

vm_t vm;

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
}

static void runtime_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    reset_stack();
}

void init_vm(void)
{
    reset_stack();
}

void free_vm(void)
{
    free_objects();
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

static value_t peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static bool is_falsey(value_t value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void)
{
    obj_string_t *b = AS_STRING(pop());
    obj_string_t *a = AS_STRING(pop());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    obj_string_t *result = take_string(chars, length);
    push(OBJ_VAL(result));
}

static interpret_result_e run(void)
{
#define READ_BYTE() (*vm.ip++) // Reads byte/instruction and advances
#define READ_TWO_BYTES() ((uint32_t)(READ_BYTE() | (READ_BYTE() << 8) ))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) // Reads index from bytecode and advances
#define READ_CONSTANT_16() (vm.chunk->constants.values[READ_TWO_BYTES()]) 
#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        vm.stack_top[-2] = value_type(AS_NUMBER(vm.stack_top[-2]) op AS_NUMBER(vm.stack_top[-1])); \
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
            case OP_EQUAL: {
                vm.stack_top[-2] = BOOL_VAL(values_equal(vm.stack_top[-2], vm.stack_top[-1]));
                vm.stack_top--;
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    BINARY_OP(NUMBER_VAL, +);
                } else {
                    runtime_error("Operands must be two numbers or two string.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_NOT:
                push(BOOL_VAL(is_falsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                AS_NUMBER(vm.stack_top[-1]) = -AS_NUMBER(vm.stack_top[-1]);
                break;
            case OP_RETURN: {
                print_value(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_TWO_BYTES
#undef READ_CONSTANT
#undef READ_CONSTANT_16
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
