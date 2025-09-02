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

static void reset_stack()
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

void init_vm()
{
    reset_stack();
    vm.objects = NULL;
}

void free_vm()
{
    free_objects();
}

void push(value_t value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

value_t pop()
{
    vm.stack_top--;
    return *vm.stack_top;
}

value_t peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static bool is_falsey(value_t value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static obj_string_t *number_to_string(double value)
{
    char buffer[64];
    int length = snprintf(buffer, sizeof(buffer), "%g", value);
    obj_string_t *result = ALLOCATE_FAM(obj_string_t, length + 1, OBJ_STRING);
    memcpy(result->chars, buffer, length);
    result->chars[length] = '\0';
    result->length = length;
    return result;
}

static void concatenate(value_t a, value_t b)
{
    obj_string_t *str_a, *str_b;

    if (IS_STRING(a)) {
        str_a = AS_STRING(a);
    } else {
        str_a = number_to_string(AS_NUMBER(a));
    }

    if (IS_STRING(b)) {
        str_b = AS_STRING(b);
    } else {
        str_b = number_to_string(AS_NUMBER(b));
    }

    int length = str_a->length + str_b->length;
    obj_string_t *result = ALLOCATE_FAM(obj_string_t, length + 1, OBJ_STRING);
    memcpy(result->chars, str_a->chars, str_a->length);
    memcpy(result->chars + str_a->length, str_b->chars, str_b->length);
    result->chars[length] = '\0';

    push(OBJ_VAL(result));
}

#define NUMBER_VAL(value) ((value_t){VAL_NUMBER, {.number = value}})
static interpret_result_t run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        vm.stack_top[-2] = value_type(AS_NUMBER(peek(1)) op AS_NUMBER(peek(0))); \
        vm.stack_top--; \
    } while (0)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("        ");
        for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        dissasemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                value_t constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL:      push(NIL_VAL); break;
            case OP_TRUE:     push(BOOL_VAL(true)); break;
            case OP_FALSE:    push(BOOL_VAL(false)); break;
            case OP_EQUAL: {
                vm.stack_top[-2] = BOOL_VAL(values_equal(vm.stack_top[-2], vm.stack_top[-1]));
                vm.stack_top--;
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    BINARY_OP(NUMBER_VAL, +); 
                } else if ((IS_STRING(peek(0)) || IS_NUMBER(peek(0))) &&
                          (IS_STRING(peek(1)) || IS_NUMBER(peek(1)))){
                    value_t b = pop();
                    value_t a = pop();
                    concatenate(a, b);
                } else {
                    runtime_error("Operands must be two numbers or two strings or a string and a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break; 
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                vm.stack_top[-1] = BOOL_VAL(is_falsey(vm.stack_top[-1]));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm.stack_top[-1] = NUMBER_VAL(-AS_NUMBER(vm.stack_top[-1]));
                break;
            case OP_RETURN:
                print_value(pop());
                printf("\n");
                return INTERPRET_OK;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

interpret_result_t interpret(const char *source)
{
    chunk_t chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    interpret_result_t result = run();

    free_chunk(&chunk);
    return result;
}
